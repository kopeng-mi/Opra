#include "sim/combat.h"

#include <algorithm>
#include <cmath>

#include "sim/collision.h"

namespace opra {

namespace {

constexpr Real PI = 3.14159265358979323846;

/** s5.4's k: a sustained PDC burst is a threat, a single round is not. A 20 g round at 1100 m/s
 *  carries 12.1 kJ; k turns joules into hull points. */
constexpr Real DAMAGE_K = 1.0 / 6000.0;

}  // namespace

Real lead_solve(const Vec2 &r, const Vec2 &v, Real s) {
    const Real rr = r.x * r.x + r.y * r.y;
    const Real rv = r.x * v.x + r.y * v.y;
    const Real vv = v.x * v.x + v.y * v.y;
    if (!(s > 0.0)) return -1;
    // |v| ~ 0: a stationary target is a direct aim (s5.2).
    if (vv < 1.0e-9) return rr > 0.0 ? std::sqrt(rr) / s : -1;
    const Real a = vv - s * s;
    if (std::fabs(a) < 1.0e-9 * std::max(s * s, 1.0)) {
        // v.v = s^2: the quadratic collapses to linear, valid only when the range is closing.
        if (rv >= 0.0 || rr <= 0.0) return -1;
        return -rr / (2.0 * rv);
    }
    const Real discriminant = rv * rv - a * rr;
    if (discriminant < 0.0) return -1;  // cannot be caught: hold fire (s5.2)
    const Real root = std::sqrt(discriminant);
    const Real t1 = (-rv - root) / a;
    const Real t2 = (-rv + root) / a;
    // Smallest positive root; both negative means the intercept is in the past.
    if (t1 > 0.0 && t2 > 0.0) return std::min(t1, t2);
    if (t1 > 0.0) return t1;
    if (t2 > 0.0) return t2;
    return -1;
}

bool step_round(Round &round, Real dt) {
    round.position.x += round.velocity.x * dt;
    round.position.y += round.velocity.y * dt;
    round.life += dt;
    // s5.5: lifetime AND range cap. Twelve seconds at the round's own speed is the range cap.
    return round.life <= ROUND_LIFETIME;
}

void step_torpedo(Torpedo &torpedo, const Vec2 &target, const Vec2 &target_velocity,
                  bool target_alive, Real dt) {
    torpedo.age += dt;
    if (!torpedo.armed && torpedo.age >= TORPEDO_ARM_TIME &&
        torpedo.travelled >= TORPEDO_ARM_RANGE) {
        torpedo.armed = true;
    }
    if (torpedo.fuel > 0.0) torpedo.fuel -= dt;

    const Real dx = target.x - torpedo.position.x;
    const Real dy = target.y - torpedo.position.y;
    const Real range = std::hypot(dx, dy);
    const bool guiding = torpedo.armed && torpedo.fuel > 0.0 && target_alive &&
                         torpedo.target >= 0 && range > TORPEDO_MIN_GUIDE_RANGE;

    if (guiding) {
        // Proportional navigation (s5.3): lambda_dot from the 2D scalar cross, V_c the closing
        // speed, a_cmd = N * lambda_dot * V_c perpendicular to the LOS, clamped to authority.
        const Real rx = dx / range, ry = dy / range;
        const Real rvx = target_velocity.x - torpedo.velocity.x;
        const Real rvy = target_velocity.y - torpedo.velocity.y;
        // lambda_dot = (r x v) / r.r, with r and v raw: the LOS's angular rate in rad/s. Dividing
        // the normalized cross by r.r a second time was the quiet factor-of-range that left the
        // torpedo nudging at millimetres per second squared.
        const Real lambda_dot = (dx * rvy - dy * rvx) / std::max(range * range, 1.0);
        const Real closing = -(rx * rvx + ry * rvy);
        // The command opposes the LOS rate: lambda_dot positive (LOS swinging to the left of the
        // flight) demands leftward authority. (-ry, rx) is the LOS's own left normal, so the sign
        // is the plain product - and a target that turns hard late outruns the clamp (s5.3).
        Real a_cmd = TORPEDO_NAV_GAIN * lambda_dot * closing;
        a_cmd = std::clamp(a_cmd, -TORPEDO_LATERAL, TORPEDO_LATERAL);
        torpedo.velocity.x += -ry * a_cmd * dt;
        torpedo.velocity.y += rx * a_cmd * dt;
    }

    // The torpedo points where it goes: its plume and its sweeps both read from the angle.
    if (const Real speed = std::hypot(torpedo.velocity.x, torpedo.velocity.y); speed > 1e-6) {
        torpedo.angle = std::atan2(torpedo.velocity.y, torpedo.velocity.x) - PI * 0.5;
    }

    const Real px = torpedo.position.x, py = torpedo.position.y;
    torpedo.position.x += torpedo.velocity.x * dt;
    torpedo.position.y += torpedo.velocity.y * dt;
    torpedo.travelled += std::hypot(torpedo.position.x - px, torpedo.position.y - py);
}

Real kinetic_damage(Real mass, Real relative_speed) {
    const Real energy = 0.5 * mass * relative_speed * relative_speed;
    return energy * DAMAGE_K;
}

bool shape_is_drive(const Shape &shape, const Collider &collider) {
    // The drive structure is what reaches into the hull's aft quarter. The exporter's clustering
    // is per connected component, so today's merged-hull shape spans the whole hull and its aft
    // reach counts as drive structure; when per-component meshes split the clusters (s6.2's
    // walk), the same rule sharpens to the pods alone. Geometric either way - a refit's own
    // shape set classifies itself.
    Real half_length = 1.0;
    for (const Shape &other : collider.shapes) {
        half_length = std::max<Real>(half_length, std::abs(other.local_pos.y) +
                                                      (other.kind == Shape::Kind::Circle
                                                           ? other.radius
                                                           : other.half_length));
    }
    const Real aft_reach =
        shape.local_pos.y - (shape.kind == Shape::Kind::Circle ? shape.radius : shape.half_length);
    return aft_reach < -half_length * 0.5;
}

Real apply_ship_damage(ShipState &ship, int shape, Real energy) {
    // `energy` arrives already converted to hull points by the caller; the kinetic conversion is
    // kinetic_damage's, and this function is where s5.4's per-shape rule lives.
    const Real dealt = std::max(0.0, energy);
    ship.hull = std::max<Real>(0, ship.hull - dealt);
    ship.contactTimer = 0;
    if (shape < 0 || shape >= static_cast<int>(ship.collider.shapes.size())) return dealt;
    const Shape &hit = ship.collider.shapes[static_cast<size_t>(shape)];
    // s5.4: a hit on a drive pod degrades thrust; a hit on a tank vents propellant - and keeps
    // venting, which is what makes the hit matter after the round is gone.
    if (shape_is_drive(hit, ship.collider)) {
        ship.thrustDamage = std::min<Real>(0.85, ship.thrustDamage + dealt * 0.004);
    } else {
        const Real vented = std::min(ship.fuel, dealt * 0.8);
        ship.fuel -= vented;
        ship.fuelLeak += dealt * 0.35;
    }
    return dealt;
}

Vec2 muzzle_position(const Vec2 &mount, const Vec2 &direction, Real hull_radius) {
    // s5.5: at the muzzle plus the hull radius along the barrel, never at the turret origin - and
    // then marched clear, because a midship mount firing aft would otherwise spawn its round
    // inside its own hull no matter what the radius was.
    Vec2 at{mount.x + direction.x * hull_radius, mount.y + direction.y * hull_radius};
    return at;
}

/**
 * The spawn point the sim actually uses: `muzzle_position`, then advanced along the barrel until
 * the point is outside every one of the firing ship's own compound shapes. True when the point
 * ended clear; the caller fires from it.
 */
Vec2 clear_muzzle(const Collider &own, const Vec2 &ship_position, Real ship_angle,
                  const Vec2 &mount, const Vec2 &direction, Real hull_radius) {
    Vec2 at = muzzle_position(mount, direction, hull_radius);
    const Real c = std::cos(ship_angle), s = std::sin(ship_angle);
    const auto inside = [&](const Vec2 &p) {
        for (const Shape &shape : own.shapes) {
            if (shape.kind == Shape::Kind::Circle) {
                const Real wx = ship_position.x + shape.local_pos.x * c - shape.local_pos.y * s;
                const Real wy = ship_position.y + shape.local_pos.x * s + shape.local_pos.y * c;
                if (point_in_circle(Circle{wx, wy, shape.radius}, p.x, p.y)) return true;
            } else {
                const Real wx = ship_position.x + shape.local_pos.x * c - shape.local_pos.y * s;
                const Real wy = ship_position.y + shape.local_pos.x * s + shape.local_pos.y * c;
                if (point_in_box(Box{wx, wy, shape.half_length, shape.half_width,
                                     shape.local_angle + ship_angle},
                                 p.x, p.y)) {
                    return true;
                }
            }
        }
        return false;
    };
    if (own.shapes.empty()) return at;
    for (int march = 0; march < 32 && inside(at); ++march) {
        at.x += direction.x * hull_radius * 0.25;
        at.y += direction.y * hull_radius * 0.25;
    }
    return at;
}

bool aim_clears_own_hull(const Collider &own, const Vec2 &ship_position, Real ship_angle,
                         const Vec2 &muzzle, const Vec2 &direction, Real range) {
    if (own.shapes.empty() || own.bounds_radius <= 0.0) return true;
    // s5.2: test out to twice the hull radius - beyond that the round is clear of its own ship,
    // whatever the traverse. The segment runs from the muzzle, not the mount, so a correctly
    // spawned round never re-hits the shape that launched it.
    const Real reach = std::min(range, own.bounds_radius * 2.0);
    const Vec2 clipped{muzzle.x + direction.x * reach, muzzle.y + direction.y * reach};
    const Real c = std::cos(ship_angle), s = std::sin(ship_angle);
    for (const Shape &shape : own.shapes) {
        // Shape centre, world frame: rotate the local mount by the hull angle.
        const Real wx = ship_position.x + shape.local_pos.x * c - shape.local_pos.y * s;
        const Real wy = ship_position.y + shape.local_pos.x * s + shape.local_pos.y * c;
        if (shape.kind == Shape::Kind::Circle) {
            Real t = 2.0;
            if (segment_circle_hit(muzzle.x, muzzle.y, clipped.x, clipped.y, Circle{wx, wy, 0.0},
                                   shape.radius, t)) {
                return false;
            }
            continue;
        }
        // The swept segment against a box: the hull's boxes are a few metres across and the aim
        // segment is straight, so stepping the point test along the segment is exact enough to
        // catch any crossing at hull scales.
        const Box box{wx, wy, shape.half_length, shape.half_width, shape.local_angle + ship_angle};
        const int samples = 8;
        for (int k = 0; k <= samples; ++k) {
            const Real t = static_cast<Real>(k) / samples;
            const Vec2 at{muzzle.x + (clipped.x - muzzle.x) * t,
                          muzzle.y + (clipped.y - muzzle.y) * t};
            if (point_in_box(box, at.x, at.y)) return false;
        }
    }
    return true;
}

}  // namespace opra
