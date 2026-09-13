#include "game/effects.h"

#include <algorithm>
#include <cmath>

#include "sim/component.h"
#include "sim/world.h"
#include "ui/tokens.h"

namespace opra {

namespace {

/** Golden angle: the first N points of a sunflower spiral do not cluster, unlike an even split. */
constexpr Real kGoldenAngle = 2.39996322972865332;
constexpr Real kTau = 6.28318530717958648;
/** Brightness below which an additive quad cannot change an 8-bit target: the floor of "invisible". */
constexpr Real kInvisible = 1.0 / (255.0 * 255.0);

/**
 * The point on the hull's silhouette along the contact normal: where the spark belongs. The bounds
 * are the hull frame's own extents, so the normal is rotated back before the support is taken.
 */
Vec2 contact_point(const ShipState &ship, const Vec2 &normal) {
    // physics.cpp: forward = (-sin, cos), starboard = (cos, sin).
    const Real c = std::cos(ship.angle), s = std::sin(ship.angle);
    const Real along_forward = std::abs(-s * normal.x + c * normal.y);
    const Real along_starboard = std::abs(c * normal.x + s * normal.y);
    const Real support =
        along_starboard * ship.bounds.halfWidth + along_forward * ship.bounds.halfLength;
    return {ship.position.x - normal.x * support, ship.position.y - normal.y * support};
}

}  // namespace

Real effect_age(const Effect &effect, Real now) { return now - effect.born; }

Real effect_life(const Effect &effect, Real now) {
    if (!(effect.duration > 0.0)) return 1.0;
    return clampr(effect_age(effect, now) / effect.duration, 0.0, 1.0);
}

Real effect_fade(const Effect &effect, Real now) {
    const Real age = effect_age(effect, now);
    // A clock behind the emission is a reset run, not an effect that is somehow ahead: retire it.
    if (age < 0.0 || !(effect.duration > 0.0) || age >= effect.duration) return 0.0;
    const Real left = 1.0 - age / effect.duration;
    const Real fade = left * left;
    // A contribution below this never lands a count in the 8-bit target, so "the end of life" is
    // the last byte the effect could change rather than the last ulp of a subtraction.
    return fade < kInvisible ? 0.0 : fade;
}

Real effect_intensity(const Effect &effect, Real now) {
    return effect.strength * effect_fade(effect, now);
}

Vec2 effect_position(const Effect &effect, Real now) {
    const Real age = std::max<Real>(0.0, effect_age(effect, now));
    return {effect.origin.x + effect.dir.x * effect.speed * age,
            effect.origin.y + effect.dir.y * effect.speed * age};
}

Real effect_size(const Effect &effect, Real now) {
    const Real life = effect_life(effect, now);
    switch (effect.kind) {
        case EffectKind::Dust:
            // Dust opens as it travels: half its authored size at the rock, 2.2x at the end.
            return effect.size * (0.5 + 1.7 * life);
        case EffectKind::Trail:
            return effect.size * (1.0 - 0.8 * life);
        case EffectKind::Pulse:
            return effect.size * (0.7 + 0.6 * life);
        case EffectKind::Spark:
            return effect.size * (1.0 - 0.5 * life);
    }
    return effect.size;
}

Real effect_angle(const Effect &effect, Real now) {
    return effect.phase + effect.spin * std::max<Real>(0.0, effect_age(effect, now));
}

glm::vec3 effect_color(const Effect &effect, Real now) {
    // Additive blend is source alpha times colour, and the instance alpha is always one (scene.add
    // has no alpha channel), so the brightness is carried by the colour alone. The gain undoes the
    // flame profile's 0.319, which the effect pass shares with the engines.
    const float intensity = static_cast<float>(effect_intensity(effect, now) * fx::EFFECT_GAIN);
    glm::vec4 token = ui::tokens::ETCH;
    switch (effect.kind) {
        case EffectKind::Spark: token = ui::tokens::DRIVE; break;   // energy, the hull's own heat
        case EffectKind::Dust: token = ui::tokens::VELLUM; break;   // rock, ground down
        case EffectKind::Trail: token = ui::tokens::VELLUM; break;
        case EffectKind::Pulse: token = ui::tokens::NAV; break;     // where it should be
    }
    return glm::vec3(token) * intensity;
}

Real hash01(std::uint32_t n) {
    n = (n ^ 61u) ^ (n >> 16);
    n *= 9u;
    n = n ^ (n >> 4);
    n *= 0x27d4eb2du;
    n = n ^ (n >> 15);
    return static_cast<Real>(n & 0x00ffffffu) / static_cast<Real>(0x00ffffffu);
}

int spark_count(Real speed) {
    const Real over = std::max<Real>(0.0, speed - fx::CONTACT_FLOOR);
    return std::min(fx::SPARK_MAX, fx::SPARK_MIN + static_cast<int>(over / fx::SPARK_PER_SPEED));
}

Real impact_strength(Real speed) {
    return clampr((speed - fx::CONTACT_FLOOR) / (fx::SPARK_FULL_SPEED - fx::CONTACT_FLOOR), 0.0, 1.0);
}

int emit_impact(Effects &pool, const Vec2 &point, const Vec2 &normal, Real speed, Real born) {
    const Real normal_length = std::hypot(normal.x, normal.y);
    if (!(normal_length > 1e-9)) return 0;
    const Real base = std::atan2(normal.y / normal_length, normal.x / normal_length);
    const int count = spark_count(speed);
    const Real strength = impact_strength(speed);
    const Real launch = clampr(speed * 0.75, fx::SPARK_LAUNCH_MIN, fx::SPARK_LAUNCH_MAX);
    const Real size = fx::SPARK_SIZE_MIN + (fx::SPARK_SIZE_MAX - fx::SPARK_SIZE_MIN) * strength;

    int emitted = 0;
    for (int i = 0; i < count; ++i) {
        // Spread across the fan by index, jittered deterministically: the middle spark leaves down
        // the normal, so the burst points at what the hull hit even when it is only two wide.
        const Real t = count > 1 ? static_cast<Real>(i) / static_cast<Real>(count - 1) * 2.0 - 1.0 : 0.0;
        const std::uint32_t seed = static_cast<std::uint32_t>(i) * 7u + 1u;
        const Real angle = base + t * fx::SPARK_FAN + (hash01(seed) - 0.5) * 0.22;
        Effect effect;
        effect.kind = EffectKind::Spark;
        effect.origin = point;
        effect.dir = {std::cos(angle), std::sin(angle)};
        effect.speed = launch * (0.55 + 0.45 * hash01(seed + 1u));
        effect.size = size * (0.7 + 0.6 * hash01(seed + 2u));
        effect.born = born;
        effect.duration = fx::IMPACT_SECONDS * (0.8 + 0.4 * hash01(seed + 3u));
        effect.strength = strength * (0.75 + 0.25 * hash01(seed + 4u));
        effect.spin = 6.0 + 12.0 * hash01(seed + 5u);
        effect.phase = kTau * hash01(seed + 6u);
        if (pool.spawn(effect)) ++emitted;
    }
    return emitted;
}

int emit_blast(Effects &pool, const Vec2 &centre, Real rock_radius, Real born) {
    // The puff scales with the body but is never authored below the field's own floor for a visible
    // event, and its grains are born on a small ring so the cloud has volume on its first frame.
    const Real puff = std::max(rock_radius, fx::DUST_MIN_RADIUS);
    const Real size = puff * fx::DUST_SIZE;
    const Real launch = puff * fx::DUST_SPEED;
    const Real ring = puff * fx::DUST_BIRTH_RING;

    int emitted = 0;
    for (int i = 0; i < fx::DUST_COUNT; ++i) {
        // A sunflower spiral: even coverage of the disc at any count, and the same spray every
        // fracture, because a burst that varies with an RNG is a burst no screenshot can repeat.
        const std::uint32_t seed = 0x9e37u + static_cast<std::uint32_t>(i) * 3u;
        const Real angle = kGoldenAngle * static_cast<Real>(i);
        const Vec2 dir{std::cos(angle), std::sin(angle)};
        Effect effect;
        effect.kind = EffectKind::Dust;
        effect.origin = {centre.x + dir.x * ring, centre.y + dir.y * ring};
        effect.dir = dir;
        effect.speed = launch * (0.6 + 0.9 * hash01(seed));
        effect.size = size * (0.6 + 0.8 * hash01(seed + 1u));
        effect.born = born;
        effect.duration = fx::BLAST_SECONDS;
        effect.strength = fx::DUST_STRENGTH * (0.6 + 0.8 * hash01(seed + 2u));
        effect.spin = 2.0 + 5.0 * hash01(seed + 3u);
        effect.phase = kTau * hash01(seed + 4u);
        if (pool.spawn(effect)) ++emitted;
    }
    return emitted;
}

int emit_trail(Effects &pool, const Vec2 &from, const Vec2 &velocity, Real radius, Real born) {
    const Real speed = std::hypot(velocity.x, velocity.y);
    if (!(speed > 0.01)) return 0;
    const Vec2 dir{velocity.x / speed, velocity.y / speed};
    const Real spacing = radius * 0.9;

    int emitted = 0;
    for (int i = 0; i < fx::TRAIL_CUBES; ++i) {
        // Each cube starts further back along the piece's own velocity and is dimmer and smaller
        // than the one in front of it, while every cube rides with the piece at the piece's own
        // speed: the row reads as a trail without a particle ever being integrated.
        const Real back = spacing * static_cast<Real>(i + 1);
        Effect effect;
        effect.kind = EffectKind::Trail;
        effect.origin = {from.x - dir.x * back, from.y - dir.y * back};
        effect.dir = dir;
        effect.speed = speed;
        effect.size = radius * std::max<Real>(0.0, 0.45 - 0.09 * static_cast<Real>(i));
        effect.born = born;
        effect.duration = fx::TRAIL_SECONDS;
        effect.strength = fx::TRAIL_STRENGTH * (1.0 - 0.22 * static_cast<Real>(i));
        effect.spin = 1.5 + 3.0 * hash01(static_cast<std::uint32_t>(i) + 17u);
        effect.phase = kTau * hash01(static_cast<std::uint32_t>(i) + 23u);
        if (pool.spawn(effect)) ++emitted;
    }
    return emitted;
}

int emit_dock_pulse(Effects &pool, const Vec2 &centre, Real born) {
    int emitted = 0;
    for (int i = 0; i < fx::PULSE_COUNT; ++i) {
        // An even ring, not a spray: a capture is a confirmation, and a confirmation is a circle.
        const Real angle = kTau * static_cast<Real>(i) / static_cast<Real>(fx::PULSE_COUNT);
        const Vec2 dir{std::cos(angle), std::sin(angle)};
        Effect effect;
        effect.kind = EffectKind::Pulse;
        effect.origin = {centre.x + dir.x * fx::PULSE_RADIUS,
                         centre.y + dir.y * fx::PULSE_RADIUS};
        effect.dir = dir;
        effect.speed = fx::PULSE_SPEED;
        effect.size = fx::PULSE_SIZE;
        effect.born = born;
        effect.duration = fx::DOCK_SECONDS;
        effect.strength = 0.9;
        effect.spin = 0.0;
        effect.phase = angle;
        if (pool.spawn(effect)) ++emitted;
    }
    return emitted;
}

Effects::Effects(int capacity) : capacity_(std::clamp(capacity, 0, fx::POOL_MAX)) {}

bool Effects::spawn(const Effect &effect) {
    if (capacity_ <= 0) return false;
    // The write cursor is the oldest slot once the pool is full and the first free one before
    // that, so one rule covers both: the pool never grows and the oldest particle is what makes
    // room for a new one.
    slots_[static_cast<std::size_t>(next_)] = effect;
    next_ = (next_ + 1) % capacity_;
    if (count_ < capacity_) ++count_;
    return true;
}

int Effects::live(Real now) const {
    int count = 0;
    for (int slot = 0; slot < count_; ++slot) {
        if (effect_fade(slots_[static_cast<std::size_t>(slot)], now) > 0.0) ++count;
    }
    return count;
}

void Effects::clear() {
    count_ = 0;
    next_ = 0;
    retired_seen_.fill(false);
    last_velocity_ = {};
    last_contact_timer_ = 1.0;
    last_hull_ = 0.0;
    last_clock_ = 0.0;
    last_fragments_ = 0;
    last_docked_ = false;
    primed_ = false;
}

void Effects::update(const World &world) {
    const ShipState &ship = world.ship;
    const Real now = world.elapsed;

    // A clock that ran backwards is a fresh run, not an effect that is impossibly old: forget the
    // edges rather than read this world against the last one's.
    if (primed_ && now < last_clock_) clear();
    // The first call only records: a run cannot open on a phantom impact left in the pool's memory.
    if (primed_) {
        // A damaging contact is the only thing that clears the re-arm timer, so the edge is the
        // event - and physics.cpp charges one damage event per CONTACT_REARM, so a frame can hold
        // exactly one. The hull it cost is the closing speed that caused it, inverted.
        if (ship.contactTimer < CONTACT_REARM && last_contact_timer_ >= CONTACT_REARM) {
            const Real damage = std::max<Real>(0.0, last_hull_ - ship.hull);
            const Real speed =
                damage > 0.0 ? damage / fx::HULL_PER_SPEED + fx::CONTACT_FLOOR : 0.0;
            // The hull is pushed away from whatever it hit, and a frame of thrust is three orders
            // below an impact's impulse, so the velocity step points where the spark belongs.
            const Vec2 push{ship.velocity.x - last_velocity_.x, ship.velocity.y - last_velocity_.y};
            const Real push_length = std::hypot(push.x, push.y);
            if (speed > 0.0 && push_length > 1e-6) {
                const Vec2 normal{push.x / push_length, push.y / push_length};
                emit_impact(*this, contact_point(ship, normal), normal, speed, now);
            }
        }

        // A body that left the field is a fracture: break_rock retires it in place and neither the
        // rock field nor the deque is ever reordered, so the ids alone are the identity. This is
        // also the only reading that catches a rock too small to leave any pieces, and the puff it
        // makes is the body's own position and radius rather than a reconstruction.
        const auto fresh = [this](const Obstacle &body) {
            const std::size_t id = static_cast<std::size_t>(body.id);
            if (id >= fx::SEEN_MAX || retired_seen_[id]) return false;
            retired_seen_[id] = true;
            return true;
        };
        for (const Obstacle &rock : world.rocks) {
            if (rock.retired && fresh(rock)) emit_blast(*this, Vec2{rock.x, rock.y}, rock.radius, now);
        }
        for (const Obstacle &piece : world.fragments) {
            if (piece.retired && fresh(piece)) {
                emit_blast(*this, Vec2{piece.x, piece.y}, piece.radius, now);
            }
        }

        // The pieces of a break are what the trail is made of. They arrive on the tail of the deque,
        // so growth there is the break, and each piece rides its own velocity for the trail's life.
        if (world.fragments.size() > last_fragments_) {
            for (std::size_t i = last_fragments_; i < world.fragments.size(); ++i) {
                const Obstacle &piece = world.fragments[i];
                emit_trail(*this, Vec2{piece.x, piece.y}, Vec2{piece.vx, piece.vy}, piece.radius,
                           now);
            }
        }

        // `docked` goes true once, on the frame the capture reaches hard dock, and stays true until
        // undock: the rising edge is exactly the moment worth a pulse, and the only one.
        if (world.docked && !last_docked_) {
            emit_dock_pulse(*this, Vec2{ship.position.x, ship.position.y}, now);
        }
    }

    primed_ = true;
    last_velocity_ = {ship.velocity.x, ship.velocity.y};
    last_contact_timer_ = ship.contactTimer;
    last_hull_ = ship.hull;
    last_clock_ = now;
    last_fragments_ = world.fragments.size();
    last_docked_ = world.docked;
}

Effects &world_effects() {
    static Effects pool;
    return pool;
}

void clear_effects() { world_effects().clear(); }

WreckageResult emit_wreckage(Effects &pool, const ShipDesign &design, const PartTable &parts,
                             const Vec2 &ship_pos, const Vec2 &ship_vel, Real ship_angle,
                             Real ship_omega, Real blast_energy, Real born,
                             Real station_bounds_radius) {
    WreckageResult result;
    if (design.placements.empty()) return result;

    const Vec2 com_local = centre_of_mass(design, parts);
    Real m_total = 0.0;
    for (const auto &p : design.placements) {
        auto it = parts.find(p.part);
        if (it != parts.end()) m_total += it->second.dry_mass;
    }
    if (m_total <= 0.0) m_total = 1000.0;

    const Real cos_a = std::cos(ship_angle);
    const Real sin_a = std::sin(ship_angle);

    // Speed formula from §11:
    // s = clamp(0.6 · sqrt(E_blast / m_total), 4, 60) m/s
    const Real eff_energy = blast_energy > 0.0 ? blast_energy : 1.0e8;
    const Real s = std::clamp(0.6 * std::sqrt(eff_energy / m_total), 4.0, 60.0);

    // 1. Central puff: 24 grains
    constexpr int kCentralPuff = 24;
    for (int i = 0; i < kCentralPuff; ++i) {
        const Real a = 6.2831853f * hash01(static_cast<std::uint32_t>(i * 3 + 1));
        const Real speed = 10.0 + 30.0 * hash01(static_cast<std::uint32_t>(i * 7 + 2));
        Effect grain;
        grain.kind = EffectKind::Dust;
        grain.origin = {ship_pos.x + std::cos(a) * 2.0, ship_pos.y + std::sin(a) * 2.0};
        grain.dir = {std::cos(a), std::sin(a)};
        grain.speed = speed;
        grain.size = 2.0 + 2.0 * hash01(static_cast<std::uint32_t>(i * 5 + 3));
        grain.born = born;
        grain.duration = fx::BLAST_SECONDS;
        grain.strength = fx::DUST_STRENGTH;
        grain.spin = (hash01(static_cast<std::uint32_t>(i * 11 + 4)) * 2.0 - 1.0) * 3.0;
        grain.phase = a;
        if (pool.spawn(grain)) ++result.particles;
    }

    // 2. For each surviving placement:
    for (std::size_t i = 0; i < design.placements.size(); ++i) {
        const Placement &p = design.placements[i];
        if (p.destroyed) continue;

        const Real part_r = 2.5;
        const Real L = static_cast<Real>(design.spine.slots) * design.spine.pitch;
        const Real centre_y = L * 0.5 - (static_cast<Real>(p.slot) + 0.5 * static_cast<Real>(p.span)) * design.spine.pitch;
        Real pos_x_local = 0.0;
        if (!is_axial(p.facing)) {
            pos_x_local = face_dir(p.facing).x * (design.spine.half_width - design.spine.recess);
        }

        const Vec2 r_local{pos_x_local - com_local.x, centre_y - com_local.y};
        const Vec2 r{r_local.x * cos_a - r_local.y * sin_a,
                     r_local.x * sin_a + r_local.y * cos_a};

        const Vec2 pos_part = {ship_pos.x + r_local.x * cos_a - r_local.y * sin_a,
                               ship_pos.y + r_local.x * sin_a + r_local.y * cos_a};

        const Real r_len = std::hypot(r.x, r.y);
        Vec2 n_hat{1.0, 0.0};
        if (r_len >= 0.1) {
            n_hat = {r.x / r_len, r.y / r_len};
        }

        const Vec2 tang{-ship_omega * r.y, ship_omega * r.x};
        const Vec2 v_sep{ship_vel.x + tang.x + n_hat.x * s,
                         ship_vel.y + tang.y + n_hat.y * s};

        const Real omega = ship_omega + (hash01(static_cast<std::uint32_t>(i)) * 2.0 - 1.0) * 1.5;

        const Real total_offset = part_r + station_bounds_radius;
        const Vec2 spawn = {pos_part.x + n_hat.x * total_offset,
                            pos_part.y + n_hat.y * total_offset};

        DebrisBody db;
        db.part = p.part;
        db.pos = spawn;
        db.vel = v_sep;
        db.angle = ship_angle;
        db.angular_vel = omega;
        db.bounds_radius = part_r;
        result.debris.push_back(db);

        // 4 grains per part
        for (int g = 0; g < 4; ++g) {
            const Real ga = 6.2831853f * hash01(static_cast<std::uint32_t>(i * 4 + g + 10));
            Effect pgrain;
            pgrain.kind = EffectKind::Trail;
            pgrain.origin = pos_part;
            pgrain.dir = {std::cos(ga), std::sin(ga)};
            pgrain.speed = std::hypot(v_sep.x, v_sep.y) * 0.5 + 5.0;
            pgrain.size = 1.5;
            pgrain.born = born;
            pgrain.duration = fx::TRAIL_SECONDS;
            pgrain.strength = fx::TRAIL_STRENGTH;
            pgrain.spin = (hash01(static_cast<std::uint32_t>(i * 4 + g + 50)) * 2.0 - 1.0) * 2.0;
            pgrain.phase = ga;
            if (pool.spawn(pgrain)) ++result.particles;
        }
    }

    return result;
}

}  // namespace opra
