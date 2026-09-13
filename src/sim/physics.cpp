#include "sim/physics.h"

#include <algorithm>
#include <cmath>
#include <string_view>
#include <utility>

#include "sim/system.h"
#include "sim/terrain.h"

namespace opra {

const ShipSpec SHIPS[3] = {
    {"Kestrel", "Independent corvette", 82000, 1600000, 16000, 1.35, 100, 42, 120, 0.055, 1, 1,
     0.22},
    {"Mule", "Heavy salvage tug", 142000, 1950000, 30000, 0.82, 150, 58, 320, 0.075, 1, 1, 0.22},
    {"Needle", "Fast reconnaissance cutter", 43000, 1200000, 10000, 2.05, 75, 31, 40, 0.05, 1, 1,
     0.22},
};

// The stock boxes, re-measured against the plan-05 re-authored hulls: the box is the lit
// (flames-included) geometry, so the handling and the drawn ship never disagree. The Kestrel's
// width is the s3 hull's own 14.8 m plus the identity bands - the plan-04 model ran 60% over its
// own spec width, which is one of the things the s3 audit now catches.
const HullBoxes HULL_BOXES[3] = {{32.1, 8.1}, {38.8, 13.9}, {62, 22.7}};

namespace {

/* Rigid-body tuning: BRAKE_SECONDS is how long a kill-velocity burn takes to shed the current speed
   before the authority cap is applied. */
constexpr Real BRAKE_SECONDS = 0.35;

/**
 * How far above the ground the hull is still tested against it. A heightfield sample is kilometres
 * from its neighbours and the hull is 80 m long, so the test only ever matters within a few hundred
 * metres - and the ship spends almost all of its life far outside that.
 */
constexpr Real SURFACE_PROBE = 400.0;
/**
 * The four corner jets, as the thrust and torque solutions see them. Index is (starboard bit) |
 * (fore bit), so [starboard-fore, port-fore, starboard-aft, port-aft] - the corners the exporter
 * mounts `rcs-jet` cones on, and the two flame cones sit on the centreline.
 *
 * A jet's plume points outboard, so it thrusts the hull inboard: a starboard jet adds -x and a port
 * jet +x. Torque about z is then `-y * Fx` per jet, which is +1 for [starboard-fore, port-aft] and
 * -1 for the other pair - a yaw couple, not a translation.
 */
constexpr Real JET_THRUST_X[4] = {-1, 1, -1, 1};
constexpr Real JET_TORQUE[4] = {1, -1, -1, 1};

/** Assigns each jet its share of this step's demanded torque, lateral translation and braking. */
void solve_rcs(ShipState &state, Real torque_demand, Real lateral_demand, Real brake_demand) {
    for (int i = 0; i < 4; ++i) {
        Real authority = 0;
        if (torque_demand * JET_TORQUE[i] > 0) authority += std::abs(torque_demand);
        if (lateral_demand * JET_THRUST_X[i] > 0) authority += std::abs(lateral_demand);
        // A kill-velocity burn is not aimed along any one axis, so it leans on all four evenly.
        authority += brake_demand;
        state.rcsJet[i] = clampr(authority, 0, 1);
    }
}

}  // namespace

ShipState create_ship(ShipClass shipClass, Collider collider) {
    ShipState state;
    const ShipSpec &spec = SHIPS[static_cast<int>(shipClass)];
    state.position = {0, 0};
    state.velocity = {0, 0};
    state.angle = -0.63;
    state.angularVelocity = 0;
    state.fuel = spec.fuel;
    state.hull = spec.hull;
    state.heat = 0;
    state.cooling = spec.cooling;
    state.acceleration = 0;
    state.thrustLevel = 0;
    state.rcsActive = false;
    state.assist = true;
    state.shipClass = shipClass;
    state.spec = &spec;
    // A custom build carries its own shapes; no shapes means "use the stock table".
    if (collider.shapes.empty()) {
        const HullBoxes &table = HULL_BOXES[static_cast<int>(shipClass)];
        collider = box_collider(table.halfLength, table.halfWidth);
    }
    state.collider = std::move(collider);
    state.bounds = collider_bounds(state.collider);
    state.contactTimer = 1;
    return state;
}

Real length(const Vec2 &v) { return std::hypot(v.x, v.y); }
Real distance(const Vec2 &a, const Vec2 &b) { return std::hypot(a.x - b.x, a.y - b.y); }

Real heading(Real angle) {
    const Real degrees = -angle * 180.0 / 3.14159265358979323846;
    return std::fmod(std::fmod(degrees, 360.0) + 360.0, 360.0);
}

Real clampr(Real value, Real min, Real max) { return std::min(max, std::max(min, value)); }

void step_ship(ShipState &state, const FlightInput &input, Real dt, const Vec2 &gravity) {
    state.contactTimer += dt;
    const ShipSpec &spec = *state.spec;
    const Real dryMass = spec.mass;
    // s5.4: a drive-pod hit degrades thrust - the authority the whole flight model runs on is
    // (1 - damage), and a venting tank drains until it is dry or patched at a base.
    const Real maxAcceleration =
        spec.thrust * (1.0 - clampr(state.thrustDamage, 0.0, 1.0)) / (dryMass + state.fuel);
    const bool canBurn = state.fuel > 0 && state.hull > 0;
    const Real boost = (input.boost && state.heat < 0.95) ? 1.65 : 1.0;
    const Real thrust = canBurn ? clampr(input.thrust, -0.28, 1.0) * boost : 0.0;
    const Real turn = canBurn ? clampr(input.turn, -1.0, 1.0) : 0.0;
    const Real strafe = canBurn ? clampr(input.strafe, -1.0, 1.0) : 0.0;
    const Vec2 forward{-std::sin(state.angle), std::cos(state.angle)};
    const Vec2 right{std::cos(state.angle), std::sin(state.angle)};
    // s5.1: translation authority is the hull's own derived fraction, not a global constant - a
    // refit with more RCS blocks manoeuvres better.
    const Real strafe_fraction = spec.strafeFraction;
    Real ax = forward.x * thrust * maxAcceleration +
              right.x * strafe * maxAcceleration * strafe_fraction;
    Real ay = forward.y * thrust * maxAcceleration +
              right.y * strafe * maxAcceleration * strafe_fraction;
    // Gravity is a frame term, not a control input: it is added here and read nowhere else, so the
    // rest of the model - mass ratio, torque, heat, assist - is untouched by the star system.
    ax += gravity.x;
    ay += gravity.y;
    Real fuelRate = std::abs(thrust) * 14 + std::abs(turn) * 1.2 + std::abs(strafe) * 3;

    // An active RCS burn counters velocity. Coasting has no artificial drag.
    Real brakeDemand = 0;
    if (input.brake && canBurn) {
        const Real speed = length(state.velocity);
        const Real braking = std::min(maxAcceleration * 0.65, speed / dt);
        if (speed > 0.0001) {
            ax -= state.velocity.x / speed * braking;
            ay -= state.velocity.y / speed * braking;
            fuelRate += braking / maxAcceleration * 16;
            brakeDemand = maxAcceleration > 0 ? braking / maxAcceleration : 0;
        }
    }

    Real angularAcceleration = turn * spec.torque;
    if (state.assist && turn == 0 && canBurn) {
        const Real correction = clampr(-state.angularVelocity / dt, -spec.torque, spec.torque);
        angularAcceleration += correction;
        fuelRate += std::abs(correction) * 0.7;
    }

    state.angularVelocity += angularAcceleration * dt;
    state.angle += state.angularVelocity * dt;
    state.velocity.x += ax * dt;
    state.velocity.y += ay * dt;
    state.position.x += state.velocity.x * dt;
    state.position.y += state.velocity.y * dt;
    state.fuel = std::max(0.0, state.fuel - (fuelRate + state.fuelLeak) * dt);
    state.acceleration = std::hypot(ax, ay);
    state.thrustLevel = thrust;
    const Real speed = length(state.velocity);
    state.rcsActive = canBurn && (std::abs(turn) > 0 || std::abs(strafe) > 0 || thrust < 0 ||
                                  (input.brake && speed > 0.001) ||
                                  (state.assist && angularAcceleration != 0));
    state.heat = clampr(state.heat + (std::abs(thrust) > 1 ? 0.09
                                                           : std::abs(thrust) * 0.016 - state.cooling) * dt,
                        0, 1);
    solve_rcs(state, spec.torque > 0 ? angularAcceleration / spec.torque : 0, strafe, brakeDemand);
}

Vec2 cutter_beam(Real angle, const Vec2 &muzzle, const Vec2 &aim, Real arc, Real &out_bearing) {
    // forward is (-sin, cos), so the nose lies at angle + pi/2 in the atan2 frame: that, not the
    // hull angle itself, is what an aim point is measured against.
    constexpr Real PI = 3.14159265358979323846;
    Real delta = std::atan2(aim.y - muzzle.y, aim.x - muzzle.x) - (angle + PI * 0.5);
    while (delta > PI) delta -= PI * 2;
    while (delta < -PI) delta += PI * 2;
    out_bearing = clampr(delta, -arc, arc);
    const Real fired = angle + out_bearing;
    return Vec2{-std::sin(fired), std::cos(fired)};
}

void step_cutter(ShipState &state, Real fuelPerSecond, Real heatPerSecond, Real dt) {
    state.fuel = std::max<Real>(0, state.fuel - fuelPerSecond * dt);
    state.heat = clampr(state.heat + heatPerSecond * dt, 0, 1);
}

// --------------------------------------------------------------- the sector

const Sector SECTOR{-2600, 2600, -2100, 2100};
const Vec2 STATION{1560, 1180};
const Vec2 RELAY{-640, -520};
const Vec2 DERELICT{-1180, 1760};

Real docking_radius(const ShipState &state) {
    return std::max<Real>(DOCK_RADIUS,
                          170 + std::hypot(state.bounds.halfLength, state.bounds.halfWidth) + 24);
}

/** Station ring 78 m, its two 294 m arms, the relay mast and the 160 m wreck. */
const SolidBody SOLID_BODIES[5] = {
    {SolidBody::Kind::Circle, "station-ring", STATION.x, STATION.y, 78, 0, 0, 0, 1.15},
    {SolidBody::Kind::Box, "station-arm-port", STATION.x - 112, STATION.y, 0, 44.5, 24, 0, 1.15},
    {SolidBody::Kind::Box, "station-arm-starboard", STATION.x + 112, STATION.y, 0, 44.5, 24, 0, 1.15},
    {SolidBody::Kind::Circle, "relay", RELAY.x, RELAY.y, 20, 0, 0, 0, 1.0},
    {SolidBody::Kind::Box, "derelict", DERELICT.x, DERELICT.y, 0, 80, 41, 0, 1.1},
};
const int SOLID_BODY_COUNT = 5;

bool is_station_body(const SolidBody &body) { return std::string_view(body.id).starts_with("station"); }

Real rock_hp(Real radius) { return std::lround(14 + radius * radius * 0.42); }

std::vector<Cargo> create_cargo() {
    return {
        {"cargo-1", "Flight recorder", CargoKind::Archive, {1020, -1380}, false},
        {"cargo-2", "Research canister", CargoKind::Archive, {-1520, 420}, false},
        {"cargo-3", "Survey archive", CargoKind::Archive, {300, 1860}, false},
        {"blackbox", "Kite's End black box", CargoKind::Blackbox, DERELICT, false},
    };
}


std::deque<Obstacle> create_obstacles() {
    Rng rng(4712);
    std::deque<Obstacle> rocks;

    struct Keep {
        Real x, y, radius;
    };
    std::vector<Keep> keeps;
    for (const Cargo &cargo : create_cargo()) {
        keeps.push_back({cargo.position.x, cargo.position.y,
                         cargo.kind == CargoKind::Blackbox ? 200.0 : 135.0});
    }
    keeps.push_back({STATION.x, STATION.y, 320});
    keeps.push_back({RELAY.x, RELAY.y, 175});

    int id = 0;
    // R-2: a cubic roll on the radius. A quadratic tail put an 86 m rock in a field where the ship
    // is 80 m long - 307 px of a 900 px frame - and under perspective the near ones loomed, so the
    // field read as rubble rather than as a field. A cubic tail keeps the few big ones rare and
    // the many small ones common, which is the distribution a real belt has.
    // Plan 05 s3.1: the home framing doubled, so every rock draws twice the size it did and the
    // same count would read as rubble again. 192 keeps the flight view a field, not a wall.
    for (int i = 0; i < 192; ++i) {
        const Real x = (rng.next() - 0.5) * 5200;
        const Real y = (rng.next() - 0.5) * 4200;
        const Real roll = rng.next();
        const Real radius = 5 + roll * roll * roll * 44;
        if (std::hypot(x, y) < radius + 150) continue;
        bool blocked = false;
        for (const Keep &keep : keeps) {
            if (distance({keep.x, keep.y}, {x, y}) < radius + keep.radius * 0.8) {
                blocked = true;
                break;
            }
        }
        if (blocked) continue;
        const Real hp = rock_hp(radius);
        Obstacle rock;
        rock.id = id++;
        rock.x = x;
        rock.y = y;
        rock.radius = radius;
        rock.seed = i + 12;
        // The background shelf: every fifth rock sits well below the navigation plane and never
        // collides. Far enough down that perspective shrinks it to a silhouette - at -200 m a rock
        // filled the frame with a dark disc and a lit crescent, reading as a planet nobody put
        // there (plan 05 S-1's own standard: nothing at close range may read as a circle).
        rock.z = (i % 10 == 0) ? -2500.0 - rng.next() * 2500.0 : 0;
        rock.hp = hp;
        rock.maxHp = hp;
        rocks.push_back(rock);
    }
    return rocks;
}

// ------------------------------------------------------------ spatial grid

namespace {
constexpr Real CELL = 260;
int cell_key(int cx, int cy) { return cx * 8192 + cy; }
}  // namespace

SpatialGrid::SpatialGrid(std::deque<Obstacle> &obstacles) {
    for (Obstacle &obstacle : obstacles) {
        // A rock is filed by centre only, and CELL > 2 * maxRadius guarantees a 3x3 neighbourhood
        // query can never miss it.
        cells_[cell_key(static_cast<int>(std::floor(obstacle.x / CELL)),
                        static_cast<int>(std::floor(obstacle.y / CELL)))]
            .push_back(&obstacle);
    }
}

void SpatialGrid::insert(Obstacle *obstacle) {
    cells_[cell_key(static_cast<int>(std::floor(obstacle->x / CELL)),
                    static_cast<int>(std::floor(obstacle->y / CELL)))]
        .push_back(obstacle);
}

void SpatialGrid::add(Obstacle *obstacle) { insert(obstacle); }

void SpatialGrid::remove(Obstacle *obstacle) { remove_at(obstacle, obstacle->x, obstacle->y); }

void SpatialGrid::remove_at(Obstacle *obstacle, Real x, Real y) {
    auto it = cells_.find(cell_key(static_cast<int>(std::floor(x / CELL)),
                                   static_cast<int>(std::floor(y / CELL))));
    if (it == cells_.end()) return;
    auto &bucket = it->second;
    bucket.erase(std::remove(bucket.begin(), bucket.end(), obstacle), bucket.end());
}

void SpatialGrid::refile(Obstacle *obstacle, Real previousX, Real previousY) {
    const int previousKey = cell_key(static_cast<int>(std::floor(previousX / CELL)),
                                     static_cast<int>(std::floor(previousY / CELL)));
    const int key = cell_key(static_cast<int>(std::floor(obstacle->x / CELL)),
                             static_cast<int>(std::floor(obstacle->y / CELL)));
    if (previousKey == key) return;
    remove_at(obstacle, previousX, previousY);
    insert(obstacle);
}

void SpatialGrid::near(Real x, Real y, std::vector<Obstacle *> &out) const {
    out.clear();
    const int cx = static_cast<int>(std::floor(x / CELL));
    const int cy = static_cast<int>(std::floor(y / CELL));
    for (int i = -1; i <= 1; ++i) {
        for (int j = -1; j <= 1; ++j) {
            auto it = cells_.find(cell_key(cx + i, cy + j));
            if (it == cells_.end()) continue;
            out.insert(out.end(), it->second.begin(), it->second.end());
        }
    }
}

void SpatialGrid::near_segment(Real x0, Real y0, Real x1, Real y1,
                               std::vector<Obstacle *> &out) const {
    out.clear();
    const int min_cx = static_cast<int>(std::floor(std::min(x0, x1) / CELL)) - 1;
    const int max_cx = static_cast<int>(std::floor(std::max(x0, x1) / CELL)) + 1;
    const int min_cy = static_cast<int>(std::floor(std::min(y0, y1) / CELL)) - 1;
    const int max_cy = static_cast<int>(std::floor(std::max(y0, y1) / CELL)) + 1;
    for (int cx = min_cx; cx <= max_cx; ++cx) {
        for (int cy = min_cy; cy <= max_cy; ++cy) {
            auto it = cells_.find(cell_key(cx, cy));
            if (it == cells_.end()) continue;
            out.insert(out.end(), it->second.begin(), it->second.end());
        }
    }
}

// ---------------------------------------------------------------- contacts

namespace {

/** Resolves one contact and returns the hull damage it cost. */
Real apply_contact(ShipState &state, Real restitution, const Vec2 &push) {
    const Real magnitude = std::hypot(push.x, push.y);
    if (magnitude < 1e-6) return 0;
    const Real nx = push.x / magnitude, ny = push.y / magnitude;
    state.position.x += push.x;
    state.position.y += push.y;
    const Real closing = state.velocity.x * nx + state.velocity.y * ny;
    if (closing >= 0) return 0;
    // s5.1: at close quarters a nudge against a rock is a nudge. Restitution fades out below 5
    // m/s of closing speed, so low-speed contact cannot launch the hull.
    restitution *= std::clamp(-closing / 5.0, 0.0, 1.0);
    state.velocity.x -= restitution * closing * nx;
    state.velocity.y -= restitution * closing * ny;
    // Damage starts at 6 m/s and climbs slower than the closing speed, and a fresh impact costs hull
    // while grinding along a boulder does not.
    const Real damage = std::min<Real>(42, std::max<Real>(0, -closing - 6) * 0.55);
    if (damage <= 0 || state.contactTimer < CONTACT_REARM) return 0;
    state.contactTimer = 0;
    state.hull = std::max<Real>(0, state.hull - damage);
    return damage;
}

}  // namespace

Real resolve_collision(ShipState &state, const Obstacle &rock) {
    if (rock.z != 0 || rock.hp <= 0) return 0;
    // Asteroids are irregular, so their collider is a circle just inside the drawn silhouette. The
    // hull is a shape set: the deepest single MTV moves it, once.
    Vec2 push;
    if (!collider_circle_out(state.collider, state.position, state.angle, rock.x, rock.y,
                             rock.radius * 0.92, push)) {
        return 0;
    }
    return apply_contact(state, 1.3, push);
}

namespace {
Real g_station_spin = 0;
}

void set_station_spin(Real angle) { g_station_spin = angle; }

Box solid_body_box(const SolidBody &body) {
    const Real spin = is_station_body(body) ? g_station_spin : 0;
    const Real x = body.x - STATION.x, y = body.y - STATION.y;
    Box box;
    box.x = STATION.x + x * std::cos(spin) - y * std::sin(spin);
    box.y = STATION.y + x * std::sin(spin) + y * std::cos(spin);
    box.halfLength = body.halfLength;
    box.halfWidth = body.halfWidth;
    box.angle = body.angle + spin;
    return box;
}

SurfaceContact resolve_surface(ShipState &state, const glm::dvec2 &relative, const Body &body,
                               const SurfaceProfile &profile) {
    SurfaceContact contact;
    if (!body.terrain.present ||
        profile.height.size() != static_cast<size_t>(TERRAIN_SAMPLES)) {
        return contact;
    }
    const Real radius = static_cast<Real>(glm::length(relative));
    const Real theta = static_cast<Real>(std::atan2(relative.y, relative.x));
    const Real ground = static_cast<Real>(terrain_radius(profile, theta));
    // The hull is 80 m long and a sample is kilometres away: anything this far above the ground is
    // not touching it yet, and the whole body is 1e6 m across.
    if (radius > ground + SURFACE_PROBE) return contact;

    const Real step = static_cast<Real>(6.283185307179586) / static_cast<Real>(TERRAIN_SAMPLES);
    const Real turns = theta / step;
    const Real base = std::floor(turns);
    const Real theta0 = base * step;
    const Real theta1 = theta0 + step;
    const glm::dvec2 p0{terrain_radius(profile, theta0) * std::cos(theta0),
                        terrain_radius(profile, theta0) * std::sin(theta0)};
    const glm::dvec2 p1{terrain_radius(profile, theta1) * std::cos(theta1),
                        terrain_radius(profile, theta1) * std::sin(theta1)};
    const glm::dvec2 along = p1 - p0;
    const Real length = static_cast<Real>(glm::length(along));
    if (length < 1e-6) return contact;

    // The plan's narrow phase: one box, and the existing compound collider runs against it. Zero
    // thickness is not representable in a SAT test, so the deck is half a metre thick - a hull that
    // is 80 m long never sees the difference.
    Box box{};
    const glm::dvec2 mid = (p0 + p1) * 0.5;
    box.x = state.position.x + static_cast<Real>(mid.x - relative.x);
    box.y = state.position.y + static_cast<Real>(mid.y - relative.y);
    box.halfLength = length * 0.5f;
    box.halfWidth = 0.5f;
    box.angle = static_cast<Real>(std::atan2(along.y, along.x));

    Vec2 push;
    if (!collider_box_out(state.collider, state.position, state.angle, box, push)) return contact;
    contact.touched = true;
    // Ground does not bounce: 0.05 is the little that is left after the legs and the regolith.
    contact.damage = apply_contact(state, 0.05, push);
    return contact;
}

Real resolve_bodies(ShipState &state, bool docked) {
    Real damage = 0;
    for (int i = 0; i < SOLID_BODY_COUNT; ++i) {
        const SolidBody &body = SOLID_BODIES[i];
        // Docking clearance opens the station collar; everything else stays solid.
        if (docked && is_station_body(body)) continue;
        Vec2 push;
        // Both paths return an MTV that moves the hull, whichever primitive the body is.
        const bool hit = body.kind == SolidBody::Kind::Circle
                             ? collider_circle_out(state.collider, state.position, state.angle,
                                                   body.x, body.y, body.radius, push)
                             : collider_box_out(state.collider, state.position, state.angle,
                                                solid_body_box(body), push);
        if (!hit) continue;
        damage += apply_contact(state, body.restitution, push);
    }
    return damage;
}

bool can_dock(const ShipState &state) {
    return state.hull > 0 && distance(state.position, STATION) <= docking_radius(state) &&
           length(state.velocity) < DOCK_SPEED;
}

Real recovery_radius(const ShipState &state, const Cargo &cargo) {
    const Real dx = cargo.position.x - state.position.x;
    const Real dy = cargo.position.y - state.position.y;
    const Real range = std::hypot(dx, dy);
    const Real cos = std::cos(state.angle), sin = std::sin(state.angle);
    const Real extent =
        range > 0 ? (std::abs(dx * cos + dy * sin) * state.bounds.halfWidth +
                     std::abs(-dx * sin + dy * cos) * state.bounds.halfLength) / range
                  : 0;
    return std::max<Real>(75, extent + (cargo.kind == CargoKind::Blackbox ? 110 : 50));
}

bool can_recover(const ShipState &state, const Cargo &cargo) {
    return state.hull > 0 && !cargo.collected &&
           distance(state.position, cargo.position) <= recovery_radius(state, cargo) &&
           length(state.velocity) < 12;
}

// ------------------------------------------------------------ rock breaking

FractureResult fracture_rock(Obstacle &rock, int &nextId, Rng &rng) {
    rock.hp = 0;
    FractureResult result;
    const int pieces = rock.radius >= 20 ? (rock.radius > 48 ? 3 : 2) : 0;
    for (int i = 0; i < pieces; ++i) {
        const Real angle = (static_cast<Real>(i) / pieces) * 3.14159265358979323846 * 2 + rng.next() * 0.8;
        // Conserve area, not radius: r_child = r_parent / sqrt(pieces).
        const Real radius = std::max<Real>(9, rock.radius / std::sqrt(static_cast<Real>(pieces)) *
                                                 (0.9 + rng.next() * 0.1));
        const Real impulse = 14 + rng.next() * 22;
        const int id = nextId++;
        Obstacle fragment;
        fragment.id = id;
        fragment.seed = id * 7 + 3;
        fragment.z = 0;
        fragment.x = rock.x + std::cos(angle) * rock.radius * 0.5;
        fragment.y = rock.y + std::sin(angle) * rock.radius * 0.5;
        fragment.radius = radius;
        fragment.hp = rock_hp(radius);
        fragment.maxHp = fragment.hp;
        fragment.vx = std::cos(angle) * impulse;
        fragment.vy = std::sin(angle) * impulse;
        fragment.moving = true;
        result.fragments.push_back(fragment);
    }
    const int drops = 1 + static_cast<int>(rock.radius / 26);
    for (int i = 0; i < drops; ++i) {
        const Real angle = rng.next() * 3.14159265358979323846 * 2;
        const Real impulse = 8 + rng.next() * 26;
        Ore chunk;
        chunk.id = nextId++;
        chunk.x = rock.x + std::cos(angle) * rock.radius * 0.4;
        chunk.y = rock.y + std::sin(angle) * rock.radius * 0.4;
        chunk.vx = std::cos(angle) * impulse;
        chunk.vy = std::sin(angle) * impulse;
        chunk.amount = std::lround(6 + rock.radius * 0.9);
        chunk.life = 90;
        result.ore.push_back(chunk);
    }
    return result;
}

Real step_ore(std::vector<Ore> &ore, const ShipState &state, Real dt, Real pickupRadius, Real space) {
    Real taken = 0;
    // Backwards, matching the original: erasing while walking forwards skips the next chunk.
    for (size_t i = ore.size(); i-- > 0;) {
        Ore &chunk = ore[i];
        chunk.x += chunk.vx * dt;
        chunk.y += chunk.vy * dt;
        chunk.vx *= 0.995;
        chunk.vy *= 0.995;
        chunk.life -= dt;
        const Real range = std::hypot(chunk.x - state.position.x, chunk.y - state.position.y);
        if (range < pickupRadius && taken < space) {
            // Inside the collector envelope it is drawn in, so pickup reads as a deliberate scoop.
            const Real pull = (1 - range / pickupRadius) * 260 * dt;
            chunk.x += (state.position.x - chunk.x) * std::min<Real>(1, pull / std::max<Real>(range, 1));
            chunk.y += (state.position.y - chunk.y) * std::min<Real>(1, pull / std::max<Real>(range, 1));
        }
        if (range < 26 && taken < space) {
            const Real loaded = std::min(chunk.amount, space - taken);
            taken += loaded;
            chunk.amount -= loaded;
            if (chunk.amount <= 0 || chunk.life <= 0) ore.erase(ore.begin() + static_cast<long>(i));
        } else if (chunk.life <= 0) {
            ore.erase(ore.begin() + static_cast<long>(i));
        }
    }
    return taken;
}

void step_fragment(Obstacle &fragment, SpatialGrid &grid, Real dt) {
    if (!fragment.moving) return;
    const Real previousX = fragment.x, previousY = fragment.y;
    const Real decay = std::pow(0.995, dt * 120);
    fragment.vx *= decay;
    fragment.vy *= decay;
    fragment.x += fragment.vx * dt;
    fragment.y += fragment.vy * dt;
    grid.refile(&fragment, previousX, previousY);
}

}  // namespace opra
