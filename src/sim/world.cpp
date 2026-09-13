#include "sim/world.h"

#include <algorithm>
#include <cmath>

#include "sim/atmosphere.h"
#include "sim/survey.h"
#include "sim/terrain.h"

namespace opra {

std::vector<Contact> World::contacts() const {
    std::vector<Contact> list;
    for (size_t i = 0; i < cargos.size(); ++i) {
        if (cargos[i].collected) continue;
        const int index = static_cast<int>(i);
        list.push_back({cargos[i].name, cargos[i].position, true, index, index});
    }
    list.push_back({"Wayfarer station", STATION, false, -1, CONTACT_STATION});
    list.push_back({"Relay mast", RELAY, false, -1, CONTACT_RELAY});
    list.push_back({"Kite's End", DERELICT, false, -1, CONTACT_DERELICT});
    return list;
}

int contact_index(const std::vector<Contact> &list, int id) {
    for (size_t i = 0; i < list.size(); ++i) {
        if (list[i].id == id) return static_cast<int>(i);
    }
    return -1;
}

void World::attach_system(const SystemDef &loaded, int anchor_index) {
    system = loaded;
    anchor_body = anchor_index;
    // The sector's clock is the system's clock: `elapsed` starts at the epoch and the bodies are
    // propagated from it, so a run is reproducible from the file alone.
    propagate(system, elapsed, bodies);
    if (anchor_body >= 0 && anchor_body < static_cast<int>(bodies.size())) {
        anchor = bodies[static_cast<size_t>(anchor_body)];
    }
    primary = -1;
    // The ground and the survey live per body, built once and deterministically here: the silhouette
    // the map drew is the one the collision code samples (plan 3.2).
    surfaces.clear();
    surfaces.reserve(system.bodies.size());
    scanned.assign(system.bodies.size(), {});
    // The survey's finds (plan 05 s6.1), described by the system's own seeds.
    discoveries = generate_discoveries(system);
    for (const Body &body : system.bodies) surfaces.push_back(generate_surface(body));
}

glm::dvec2 World::system_position() const {
    return anchor.position + glm::dvec2(ship.position.x, ship.position.y);
}

glm::dvec2 World::system_velocity() const {
    return anchor.velocity + glm::dvec2(ship.velocity.x, ship.velocity.y);
}

Vec2 World::gravity() {
    if (system.bodies.empty() || bodies.empty()) return {0.0, 0.0};
    // The deepest body whose sphere holds the ship: in the zone that is the star, so the walk is
    // one pass over its children. Outside it, this is the body the ship is actually falling toward.
    const glm::dvec2 here = system_position();
    primary = primary_of(system, bodies, primary, here);
    const size_t index = static_cast<size_t>(primary < 0 ? 0 : primary);
    const Body &pull = system.bodies[index];
    if (pull.mu <= 0.0) return {0.0, 0.0};
    const glm::dvec2 centre = bodies[index].position;
    const glm::dvec2 to_ship = centre - here;
    const glm::dvec2 to_anchor = centre - anchor.position;
    const double ship_r2 = glm::dot(to_ship, to_ship);
    const double anchor_r2 = glm::dot(to_anchor, to_anchor);
    if (ship_r2 <= 0.0 || anchor_r2 <= 0.0) return {0.0, 0.0};
    // The anchor is in the same free fall, so its own pull is subtracted: what the ship feels in
    // the zone frame is the difference, which over a five-kilometre field is the tidal term.
    const glm::dvec2 at_ship = to_ship * (pull.mu / (ship_r2 * std::sqrt(ship_r2)));
    const glm::dvec2 at_anchor = to_anchor * (pull.mu / (anchor_r2 * std::sqrt(anchor_r2)));
    const glm::dvec2 relative = at_ship - at_anchor;
    return {relative.x, relative.y};
}

namespace {

/** A port in the world, given its owner's pose. The hull angle convention is forward = (-sin, cos). */
PortState port_state(const Port &port, const Vec2 &position, Real angle) {
    const Real c = std::cos(angle);
    const Real s = std::sin(angle);
    PortState out;
    out.position = {position.x + port.local_pos.x * c - port.local_pos.y * s,
                    position.y + port.local_pos.x * s + port.local_pos.y * c};
    out.normal = {port.local_normal.x * c - port.local_normal.y * s,
                  port.local_normal.x * s + port.local_normal.y * c};
    return out;
}

/** The station turns at this rate: its ports move with it, and so does the corridor. */
constexpr Real STATION_SPIN_RATE = 0.035;

}  // namespace

void World::set_ports(const std::vector<Port> &station, const Port &hull) {
    station_ports = station;
    ship_port = hull;
}

void World::undock() {
    if (docked_for < 0.0 || target_port < 0) return;
    const Vec2 push = undock_impulse(target_state);
    ship.velocity = {ship.velocity.x + push.x, ship.velocity.y + push.y};
    docked_for = -1.0;
    capture_started = -1.0;
    docked = false;
    // The gate has to be re-earned: a ship that has just pushed off is not in the corridor.
    gate = ApproachGate{};
}

void World::update_docking(Real dt) {
    if (station_ports.empty()) return;

    // The ship's port rides the hull; the station's ports turn with the station.
    ship_port_state = port_state(ship_port, ship.position, ship.angle);
    const Vec2 station_position = STATION;

    // Approach the nearest port that is in front of the ship's own: a port behind the hull is not
    // something the pilot can capture, and the gate would refuse it anyway.
    if (target_port < 0 || docked_for >= 0.0) {
        Real best = 260.0;  // the capture envelope, metres: beyond it nothing is being approached
        int found = -1;
        for (size_t i = 0; i < station_ports.size(); ++i) {
            const PortState candidate = port_state(station_ports[i], station_position, stationSpin);
            const Real range = distance(ship_port_state.position, candidate.position);
            if (range < best) {
                best = range;
                found = static_cast<int>(i);
            }
        }
        target_port = found;
    }
    if (target_port < 0) return;
    const Port &berth = station_ports[static_cast<size_t>(target_port)];
    target_state = port_state(berth, station_position, stationSpin);

    // The port's own velocity is the station's rotation about its centre: small, but it is the
    // difference between a clean capture and one that fights the ship at the last metre.
    const Vec2 arm{target_state.position.x - station_position.x,
                   target_state.position.y - station_position.y};
    const Vec2 port_velocity{-STATION_SPIN_RATE * arm.y, STATION_SPIN_RATE * arm.x};

    if (docked_for >= 0.0) {
        // Hard docked: the ship rides the port, so the pilot's controls do nothing until R.
        ship.position = target_state.position;
        ship.velocity = port_velocity;
        ship.angle = std::atan2(-target_state.normal.y, -target_state.normal.x) + 1.5707963267948966;
        ship.angularVelocity = STATION_SPIN_RATE;
        docked_for += dt;
        return;
    }

    if (capture_started >= 0.0) {
        // The capture: a cubic Hermite in the port's frame, with the pilot's control blended out.
        const Real t = static_cast<Real>(elapsed - capture_started);
        const Vec2 offset = capture.offset_at(t);
        const Vec2 velocity = capture.velocity_at(t);
        ship.position = {target_state.position.x + offset.x, target_state.position.y + offset.y};
        ship.velocity = {port_velocity.x + velocity.x, port_velocity.y + velocity.y};
        ship.thrustLevel = 0.0;
        ship.rcsActive = false;
        if (t >= capture.duration) {
            capture_started = -1.0;
            docked_for = 0.0;
            docked = true;
        }
        return;
    }

    gate = evaluate_gate(ship_port_state, ship.velocity, target_state, port_velocity,
                         ship.angularVelocity - STATION_SPIN_RATE, gate.held_for, dt);
    if (gate.held) {
        capture.duration = 2.0;
        capture.from_offset = {ship_port_state.position.x - target_state.position.x,
                               ship_port_state.position.y - target_state.position.y};
        capture.from_velocity = {ship.velocity.x - port_velocity.x, ship.velocity.y - port_velocity.y};
        capture_started = elapsed;
        docked = false;
    }
}

void World::step(const FlightInput &input, Real dt) {
    // Bodies first: they are on rails, so this is exact at any dt, and both the ship's frame
    // (the anchor) and its gravity this step are measured against the result.
    elapsed += dt;
    if (!system.bodies.empty()) {
        propagate(system, elapsed, bodies);
        if (anchor_body >= 0 && anchor_body < static_cast<int>(bodies.size())) {
            anchor = bodies[static_cast<size_t>(anchor_body)];
        }
    }

    // Berthing: a capture or a hard dock drives the ship itself, so the pilot's controls, the
    // integrator and the contact solver all stand down until it is docked or released.
    const bool berthing = docked_for >= 0.0 || capture_started >= 0.0;
    if (berthing) {
        update_docking(dt);
    } else {
        step_ship(ship, input, dt, gravity());
        // The primary may have changed under the ship: the caller drops warp on it, because a
        // frame switch is exactly the moment the conic stops describing where the ship is going.
        const int before = primary;
        primary = primary_of(system, bodies, primary, system_position());
        soi_switched = system.bodies.empty() ? false : (primary != before);

        // One grid query serves both rocks and fragments: both are filed in the same cells, so the
        // step never walks the field and a fragment is as solid as a rock.
        std::vector<Obstacle *> nearby;
        grid.near(ship.position.x, ship.position.y, nearby);
        for (Obstacle *body : nearby) {
            if (!body->retired) resolve_collision(ship, *body);
        }
        resolve_bodies(ship, false);
        // Air then ground: the drag acts on the velocity the ground contact then resolves.
        apply_air(dt);
        apply_ground(dt);
        update_docking(dt);
    }

    stationSpin += dt * STATION_SPIN_RATE;
    set_station_spin(stationSpin);
    run_nodes();
    // Close quarters runs inside the fixed step: rounds, torpedoes and their hits are sim state.
    step_combat(dt);
    for (Obstacle &fragment : fragments) {
        if (fragment.retired) continue;
        step_fragment(fragment, grid, dt);
    }
    oreHeld += step_ore(ore, ship, dt);
}

void World::apply_air(Real dt) {
    air_density = 0.0;
    air_flux = 0.0;
    air_altitude = 0.0;
    air_body = -1;
    if (system.bodies.empty() || primary < 0) return;
    const size_t index = static_cast<size_t>(primary);
    if (index >= system.bodies.size()) return;
    const Body &body = system.bodies[index];
    if (!body.atmosphere.present) return;

    // Relative to the body, in the system frame. The zone frame translates with its anchor and does
    // not rotate, so an acceleration measured in the body's frame is the one the ship feels.
    const glm::dvec2 relative = system_position() - bodies[index].position;
    const glm::dvec2 velocity = system_velocity() - bodies[index].velocity;
    const SurfaceProfile *profile = index < surfaces.size() ? &surfaces[index] : nullptr;
    const AirStep air = air_step(body, profile, relative, velocity, DRAG_CD_AREA_OVER_MASS,
                                NOSE_RADIUS);
    air_density = air.density;
    air_flux = air.flux;
    air_altitude = air.altitude;
    air_body = primary;
    if (air.density <= 0.0) return;

    ship.velocity.x += air.drag.x * dt;
    ship.velocity.y += air.drag.y * dt;
    // The same heat budget the drives draw on: a hot entry and a hard burn cannot both be ignored.
    ship.heat = clampr(ship.heat + air.flux / HEAT_FLUX_FULL * dt, 0.0, 1.0);
}

void World::apply_ground(Real dt) {
    if (system.bodies.empty() || primary < 0) return;
    const size_t index = static_cast<size_t>(primary);
    if (index >= system.bodies.size() || index >= surfaces.size()) return;
    const Body &body = system.bodies[index];
    if (!body.terrain.present) return;
    const glm::dvec2 relative = system_position() - bodies[index].position;
    const double radius = glm::length(relative);
    const double theta = std::atan2(relative.y, relative.x);
    const double altitude = radius - terrain_radius(surfaces[index], theta);

    // Survey before contact: a pass is flown long before anything touches, and what it records is
    // the ground track it covered (plan 3.7).
    const glm::dvec2 velocity = system_velocity() - bodies[index].velocity;
    const glm::dvec2 up{std::cos(theta), std::sin(theta)};
    record_scan(*this, primary, altitude, std::fabs(-velocity.x * up.y + velocity.y * up.x), theta);

    const SurfaceContact contact = resolve_surface(ship, relative, body, surfaces[index]);
    if (!contact.touched) {
        // Airborne again: the next contact is a new arrival, not a continuation of this one.
        landed_body = -1;
        landed_for = -1.0;
        return;
    }

    touchdown = touchdown_gate(ship, relative, body, surfaces[index], pad_index_at(body, theta));
    if (!touchdown.ok()) {
        // The plan's rule: any term outside its limit costs hull, scaled by the excess. The closing
        // speed is already charged by the contact itself, so this is the rest of the table.
        const Real cost = static_cast<Real>(touchdown.excess) * TOUCHDOWN_DAMAGE;
        if (cost > 0.0 && ship.contactTimer >= CONTACT_REARM) {
            ship.hull = std::max<Real>(0, ship.hull - cost);
            ship.contactTimer = 0;
        }
        landed_body = -1;
        landed_for = -1.0;
        return;
    }
    // A clean set-down inside the limits: landed, and the clock that refuelling and contracts read
    // starts here. The hull stays put because the ground contact holds it every step, which is what
    // "parented to the body's frame" means in a sim where the body does not spin.
    if (landed_body != primary) landed_for = 0.0;
    landed_body = primary;
    if (landed_for >= 0.0) landed_for += dt;

    // The base on this pad services the ship while it sits there (plan G16): refuel, repair, and the
    // contract work that needs a pad under it. No construction, no inventory: a pad is a place.
    base_body = -1;
    base_name.clear();
    if (body.surface.present && pad_index_at(body, theta) == body.surface.pad) {
        base_body = primary;
        base_name = body.surface.name;
        const double fuel_rate = BASE_FUEL_PER_SECOND * dt;
        const Real capacity = ship.spec ? ship.spec->fuel : ship.fuel;
        ship.fuel = std::min(capacity, ship.fuel + static_cast<Real>(fuel_rate));
        ship.hull = std::min<Real>(ship.spec ? ship.spec->hull : ship.hull,
                                   ship.hull + static_cast<Real>(BASE_REPAIR_PER_SECOND * dt));
    }
}

void World::warp_step(Real seconds) {
    if (!(seconds > 0.0)) return;
    soi_switched = false;
    // The conic is derived from where the ship *was*, in the system frame, using the frame's own
    // state at that moment: taking the anchor's position after the propagation while claiming the
    // earlier epoch moves the ship by the anchor's own travel along its orbit.
    const glm::dvec2 start_position = system_position();
    const glm::dvec2 start_velocity = system_velocity();
    elapsed += seconds;
    if (system.bodies.empty() || anchor_body < 0) {
        // No system attached: there is no conic to ride, so a warp step is a straight coast.
        ship.position.x += ship.velocity.x * seconds;
        ship.position.y += ship.velocity.y * seconds;
        return;
    }
    propagate(system, elapsed, bodies);
    anchor = bodies[static_cast<size_t>(anchor_body)];

    const size_t index = static_cast<size_t>(primary < 0 ? 0 : primary);
    const Body &pull = system.bodies[index];
    if (pull.mu > 0.0) {
        // Elements about the primary at the start of the step, then the state read back at the end:
        // the conversion is lossless to 1e-9, so warping in and out does not drift the orbit.
        const orbit::Elements elements =
            orbit::from_state(start_position, start_velocity, pull.mu, elapsed - seconds);
        const orbit::State next = orbit::state_at(elements, elapsed);
        ship.position = {next.r.x - anchor.position.x, next.r.y - anchor.position.y};
        ship.velocity = {next.v.x - anchor.velocity.x, next.v.y - anchor.velocity.y};
    }
    const int before = primary;
    primary = primary_of(system, bodies, primary, system_position());
    soi_switched = primary != before;
    run_nodes();
}

void World::run_nodes() {
    if (nodes.empty() || system.bodies.empty()) return;
    const size_t chief = static_cast<size_t>(primary < 0 ? 0 : primary);
    if (chief >= system.bodies.size()) return;
    const double mu = system.bodies[chief].mu;
    if (mu <= 0.0) return;
    while (!nodes.empty() && nodes.front().t <= elapsed) {
        const orbit::Node node = nodes.front();
        nodes.erase(nodes.begin());
        // Rebuild the state at the burn's own time, add the delta-v there, and read it back at the
        // current time: the burn lands where the plan said it would, not where the frame happens to
        // have reached.
        const orbit::Elements at_burn =
            orbit::from_state(system_position(), system_velocity(), mu, node.t);
        const orbit::Elements after = orbit::apply_node(at_burn, node);
        const orbit::State now = orbit::state_at(after, elapsed);
        ship.position = {now.r.x - anchor.position.x, now.r.y - anchor.position.y};
        ship.velocity = {now.v.x - anchor.velocity.x, now.v.y - anchor.velocity.y};
    }
}

orbit::Elements World::planned_conic() const {
    const size_t chief = static_cast<size_t>(primary < 0 ? 0 : primary);
    if (system.bodies.empty() || chief >= system.bodies.size()) return orbit::Elements{};
    const double mu = system.bodies[chief].mu;
    if (mu <= 0.0) return orbit::Elements{};
    const orbit::Elements here = orbit::from_state(
        anchor.position + glm::dvec2(ship.position.x, ship.position.y),
        anchor.velocity + glm::dvec2(ship.velocity.x, ship.velocity.y), mu, elapsed);
    return orbit::apply_nodes(here, nodes);
}

bool World::warp_to_next_node() {
    if (nodes.empty()) return false;
    const double span = nodes.front().t - elapsed;
    if (span <= 0.0) return false;
    // The railed path is exact at any step, so a warp to a node lands on the node rather than near
    // it: this is the one place where "warp" and "the burn happens correctly" are the same feature.
    warp_step(span);
    run_nodes();
    return true;
}

// ---------------------------------------------------------------- close quarters (plan 05 s5)

namespace {

/** PDC ballistics, from the plan's own figures (s5.2, s5.4): a 20 g round at 1100 m/s. */
constexpr Real PDC_ROUND_MASS = 0.02;
constexpr Real PDC_ROUND_SPEED = 1100.0;
constexpr Real PDC_RANGE = 800.0;
constexpr Real PDC_COOLDOWN = 0.10;      // seconds between rounds from one mount
constexpr Real PDC_ENGAGE_RANGE = 700.0;  // the release the mount opens fire inside

/**
 * The compound shape a swept segment struck first, or -1. The round's whole segment is tested
 * against each shape - a circle by the segment-circle hit, a box by stepping the point test along
 * the segment - and the earliest strike wins, because that is the shape the damage lands on (s5.4).
 */
int swept_ship_shape(const Collider &collider, const Vec2 &ship_position, Real ship_angle,
                     const Vec2 &from, const Vec2 &to) {
    const Real c = std::cos(ship_angle), sn = std::sin(ship_angle);
    int best = -1;
    Real best_t = 2.0;
    for (int i = 0; i < static_cast<int>(collider.shapes.size()); ++i) {
        const Shape &shape = collider.shapes[static_cast<size_t>(i)];
        const Real wx = ship_position.x + shape.local_pos.x * c - shape.local_pos.y * sn;
        const Real wy = ship_position.y + shape.local_pos.x * sn + shape.local_pos.y * c;
        if (shape.kind == Shape::Kind::Circle) {
            Real t = 2.0;
            if (segment_circle_hit(from.x, from.y, to.x, to.y, Circle{wx, wy, 0.0}, shape.radius,
                                   t) &&
                t < best_t) {
                best_t = t;
                best = i;
            }
            continue;
        }
        const Box box{wx, wy, shape.half_length, shape.half_width, shape.local_angle + ship_angle};
        for (int k = 0; k <= 6; ++k) {
            const Real f = static_cast<Real>(k) / 6.0;
            if (point_in_box(box, from.x + (to.x - from.x) * f, from.y + (to.y - from.y) * f)) {
                if (f < best_t) {
                    best_t = f;
                    best = i;
                }
                break;
            }
        }
    }
    return best;
}

}  // namespace

void World::step_combat(Real dt) {
    // PDC engagement: every mount picks its intercept on the tracked contact, with the plan's own
    // degenerate handling - no solution, beyond range, own-hull occluded - and the mount holds
    // fire instead of firing uselessly (s5.2). Firing while docked is blocked (s5.5).
    static std::vector<Real> cooldowns;
    const int mounts = static_cast<int>(std::min<size_t>(pdc_mounts.size(), 8));
    cooldowns.resize(static_cast<size_t>(mounts), 0.0);
    std::vector<Contact> list = contacts();
    const int tracked = contact_index(list, target);
    if (weapons_free && tracked >= 0 && docked_for < 0.0 && capture_started < 0.0) {
        const Contact &contact = list[static_cast<size_t>(tracked)];
        for (int m = 0; m < mounts; ++m) {
            cooldowns[static_cast<size_t>(m)] = std::max(0.0, cooldowns[static_cast<size_t>(m)] - dt);
            if (cooldowns[static_cast<size_t>(m)] > 0.0) continue;
            const Real c = std::cos(ship.angle), sn = std::sin(ship.angle);
            const Vec2 mount{ship.position.x + pdc_mounts[static_cast<size_t>(m)].x * c -
                                 pdc_mounts[static_cast<size_t>(m)].y * sn,
                             ship.position.y + pdc_mounts[static_cast<size_t>(m)].x * sn +
                                 pdc_mounts[static_cast<size_t>(m)].y * c};
            const Real dx = contact.position.x - mount.x;
            const Real dy = contact.position.y - mount.y;
            if (std::hypot(dx, dy) > PDC_ENGAGE_RANGE) continue;
            // The tracked marks are static in the zone frame, so the intercept is a direct aim.
            const Vec2 relative{dx, dy};
            const Vec2 target_velocity{0.0, 0.0};
            const Real t = lead_solve(relative, target_velocity, PDC_ROUND_SPEED);
            if (t < 0.0 || PDC_ROUND_SPEED * t > PDC_RANGE) continue;
            const Real aim_len = std::hypot(dx + target_velocity.x * t, dy + target_velocity.y * t);
            if (!(aim_len > 1e-9)) continue;
            const Vec2 aim{(dx + target_velocity.x * t) / aim_len,
                           (dy + target_velocity.y * t) / aim_len};
            if (!aim_clears_own_hull(ship.collider, ship.position, ship.angle, mount, aim,
                                     PDC_RANGE)) {
                continue;  // a stern turret does not fire through its own ship (s5.2)
            }
            const Vec2 muzzle = clear_muzzle(ship.collider, ship.position, ship.angle, mount, aim,
                                             ship.bounds.halfLength + 2.0);
            Round round;
            round.position = muzzle;
            round.velocity = {aim.x * PDC_ROUND_SPEED, aim.y * PDC_ROUND_SPEED};
            round.damage = kinetic_damage(PDC_ROUND_MASS, PDC_ROUND_SPEED);
            round.owner = 0;
            rounds.push_back(round);
            cooldowns[static_cast<size_t>(m)] = PDC_COOLDOWN;
        }
    }

    // Rounds: swept-segment hits against everything solid they pass over this step (s5.2) - a
    // 1100 m/s round covers 9.2 m per 120 Hz step, so a per-step point test simply does not work.
    std::vector<Obstacle *> near;
    for (size_t i = rounds.size(); i-- > 0;) {
        Round &round = rounds[i];
        const Vec2 from{round.position.x, round.position.y};
        if (!step_round(round, dt)) {
            rounds.erase(rounds.begin() + static_cast<long>(i));
            continue;
        }
        const Vec2 to{round.position.x, round.position.y};
        grid.near_segment(from.x, from.y, to.x, to.y, near);
        bool spent = false;
        for (const Obstacle *body : near) {
            if (body->retired || body->z != 0 || body->hp <= 0) continue;
            Real t = 2.0;
            if (segment_circle_hit(from.x, from.y, to.x, to.y, Circle{body->x, body->y, 0.0},
                                   body->radius * 0.92, t)) {
                // PDC vs rock: the same damage path the cutter drives (s5.5).
                Obstacle &hit = *const_cast<Obstacle *>(body);
                hit.hp -= round.damage;
                if (hit.hp <= 0) break_rock(hit);
                spent = true;
                break;
            }
        }
        // s5.4: a round that crosses the ship's own hull lands on the compound shape it struck -
        // drive pod or tank, not a single pool. A mount's own fire spawns clear of the hull and is
        // occlusion-tested before firing, so this path is everything else.
        if (!spent) {
            const int shape = swept_ship_shape(ship.collider, ship.position, ship.angle, from, to);
            if (shape >= 0) {
                apply_ship_damage(ship, shape, round.damage);
                spent = true;
            }
        }
        if (spent) rounds.erase(rounds.begin() + static_cast<long>(i));
    }

    // Torpedoes: guide while the tracked contact lives, then coast on the last heading (s5.5).
    for (size_t i = torpedoes.size(); i-- > 0;) {
        Torpedo &torpedo = torpedoes[i];
        const Vec2 from{torpedo.position.x, torpedo.position.y};
        Vec2 aim_at = from;
        bool alive = false;
        if (target >= 0) {
            const int tracked_at = contact_index(list, target);
            if (tracked_at >= 0) {
                aim_at = list[static_cast<size_t>(tracked_at)].position;
                alive = true;
            }
        }
        step_torpedo(torpedo, aim_at, Vec2{0.0, 0.0}, alive, dt);
        const Vec2 to{torpedo.position.x, torpedo.position.y};
        bool detonated = alive && std::hypot(to.x - aim_at.x, to.y - aim_at.y) < 8.0;
        grid.near_segment(from.x, from.y, to.x, to.y, near);
        for (const Obstacle *body : near) {
            if (body->retired || body->z != 0) continue;
            Real t = 2.0;
            if (segment_circle_hit(from.x, from.y, to.x, to.y, Circle{body->x, body->y, 0.0},
                                   body->radius, t)) {
                detonated = true;
                break;
            }
        }
        // The warhead on the ship's own hull: the compound shape it struck takes it (s5.4) - the
        // kinetic term of a 450 kg torpedo at 320 m/s plus the fixed warhead.
        const int ship_shape = swept_ship_shape(ship.collider, ship.position, ship.angle, from, to);
        if (ship_shape >= 0) {
            apply_ship_damage(ship, ship_shape, kinetic_damage(450.0, 320.0) + torpedo.warhead);
            detonated = true;
        }
        if (detonated || torpedo.age > ROUND_LIFETIME) {
            torpedoes.erase(torpedoes.begin() + static_cast<long>(i));
        }
    }
}

bool World::fire_torpedo() {
    // Firing while docked is blocked (s5.5).
    if (docked_for >= 0.0 || capture_started >= 0.0) return false;
    std::vector<Contact> list = contacts();
    if (contact_index(list, target) < 0) return false;
    Torpedo torpedo;
    // Spawn at the nose plus the hull radius along the heading (s5.5's spawn rule).
    const Vec2 forward{-std::sin(ship.angle), std::cos(ship.angle)};
    torpedo.position = {ship.position.x + forward.x * (ship.bounds.halfLength + 4.0),
                        ship.position.y + forward.y * (ship.bounds.halfLength + 4.0)};
    torpedo.velocity = {forward.x * 320.0 + ship.velocity.x,
                        forward.y * 320.0 + ship.velocity.y};
    torpedo.target = target;
    torpedo.fuel = 12.0;
    torpedo.warhead = 4.0e6;
    torpedo.angle = ship.angle;
    torpedoes.push_back(torpedo);
    return true;
}

void World::break_rock(Obstacle &rock) {
    FractureResult result = fracture_rock(rock, next_id, rng);
    for (Obstacle &fragment : result.fragments) {
        fragments.push_back(fragment);
        grid.add(&fragments.back());
    }
    for (Ore &chunk : result.ore) ore.push_back(chunk);
    // Retire, do not free: the grid holds pointers into these containers, so a body that is erased
    // can leave an entry pointing at freed memory. A retired body is skipped by the beam, the
    // collision pass and the scene instead, and it leaves the grid outright.
    grid.remove(&rock);
    rock.retired = true;
}

void World::rebuild_from_design(const PartTable &parts) {
    if (!design.placements.empty()) {
        derived = derive_spec(design, parts);
        derived.name = design.name.c_str();
        derived.role = design.role.c_str();
        ship.spec = &derived;
        ship.fuel = derived.fuel;
        ship.hull = derived.hull;
        ship.cooling = derived.cooling;
        ship.rcsJet.assign(design.placements.size(), 0.0);

        const Vec2 com = centre_of_mass(design, parts);
        const Real L = static_cast<Real>(design.spine.slots) * design.spine.pitch;
        ship.rcsArms.resize(design.placements.size());
        for (size_t k = 0; k < design.placements.size(); ++k) {
            const Placement &p = design.placements[k];
            const Real centre_y = L * 0.5 - (static_cast<Real>(p.slot) + 0.5 * static_cast<Real>(p.span)) * design.spine.pitch;
            Real pos_x = 0.0;
            if (!is_axial(p.facing)) {
                pos_x = face_dir(p.facing).x * (design.spine.half_width - design.spine.recess);
            }
            ship.rcsArms[k] = {pos_x - com.x, centre_y - com.y};
        }
    } else {
        ship.spec = &SHIPS[static_cast<int>(ship.shipClass)];
        ship.rcsJet.assign(4, 0.0);
        ship.rcsArms.clear();
    }
}

void World::rebuild_from_design() {
    if (!design.placements.empty()) {
        derived = derive_spec(design);
        derived.name = design.name.c_str();
        derived.role = design.role.c_str();
        ship.spec = &derived;
        ship.rcsJet.assign(design.placements.size(), 0.0);
        ship.rcsArms.clear();
    } else {
        ship.spec = &SHIPS[static_cast<int>(ship.shipClass)];
        ship.rcsJet.assign(4, 0.0);
        ship.rcsArms.clear();
    }
}

}  // namespace opra
