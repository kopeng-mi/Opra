#include "sim/dock.h"

#include <cmath>

namespace opra {
namespace {

double dot(const Vec2 &a, const Vec2 &b) { return a.x * b.x + a.y * b.y; }
double length(const Vec2 &v) { return std::sqrt(dot(v, v)); }
Vec2 sub(const Vec2 &a, const Vec2 &b) { return {a.x - b.x, a.y - b.y}; }
Vec2 add(const Vec2 &a, const Vec2 &b) { return {a.x + b.x, a.y + b.y}; }
Vec2 scale(const Vec2 &v, double by) { return {v.x * by, v.y * by}; }

/** Cubic Hermite basis. */
double h00(double s) { return 2.0 * s * s * s - 3.0 * s * s + 1.0; }
double h10(double s) { return s * s * s - 2.0 * s * s + s; }
double h01(double s) { return -2.0 * s * s * s + 3.0 * s * s; }
double h11(double s) { return s * s * s - s * s; }

/** The capture uses the Hermite's own derivatives, not a finite difference: the sim reads them. */
double d_h00(double s) { return 6.0 * s * s - 6.0 * s; }
double d_h10(double s) { return 3.0 * s * s - 4.0 * s + 1.0; }
double d_h01(double s) { return -6.0 * s * s + 6.0 * s; }
double d_h11(double s) { return 3.0 * s * s - 2.0 * s; }

double clamp01(double value) { return value < 0.0 ? 0.0 : (value > 1.0 ? 1.0 : value); }

}  // namespace

bool ApproachGate::all_ok() const {
    return axial_ok && lateral_ok && closing_ok && alignment_ok && rate_ok;
}

ApproachGate evaluate_gate(const PortState &from, const Vec2 &from_velocity, const PortState &to,
                           const Vec2 &to_velocity, double relative_spin, double held_for,
                           double dt, const GateLimits &limits) {
    ApproachGate gate;
    const Vec2 relative_position = sub(from.position, to.position);
    const Vec2 relative_velocity = sub(from_velocity, to_velocity);

    // Everything is measured in the target's corridor frame: axial down the normal, lateral across.
    gate.axial = dot(relative_position, to.normal);
    const Vec2 lateral_vector = sub(relative_position, scale(to.normal, gate.axial));
    gate.lateral = length(lateral_vector);
    gate.closing = -dot(relative_velocity, to.normal);
    // The ship's port faces back down the corridor, so its outward normal is -to.normal when docked.
    const Vec2 facing = {-from.normal.x, -from.normal.y};
    gate.alignment = std::acos(std::fmax(-1.0, std::fmin(1.0, dot(facing, to.normal))));
    gate.rate = std::fabs(relative_spin);

    gate.axial_ok = gate.axial > 0.0 && gate.axial < limits.axial_max;
    gate.lateral_ok = gate.lateral < limits.lateral_max;
    gate.closing_ok = gate.closing > 0.0 && gate.closing < limits.closing_max;
    gate.alignment_ok = gate.alignment < limits.alignment_max;
    gate.rate_ok = gate.rate < limits.rate_max;

    // The hold has to be continuous: the caller keeps the count while the terms keep holding, and a
    // frame that fails any term starts it again from zero.
    if (gate.all_ok()) {
        gate.held_for = held_for + dt;
        gate.held = gate.held_for >= limits.hold;
    }
    return gate;
}

Vec2 Capture::offset_at(double t) const {
    const double s = clamp01(t / duration);
    const double span = duration;
    // Hard dock is the origin: the target's own frame is where the ship ends up.
    return add(scale(from_offset, h00(s)),
               add(scale(from_velocity, span * h10(s)), Vec2{0.0, 0.0}));
}

Vec2 Capture::velocity_at(double t) const {
    const double s = clamp01(t / duration);
    return add(scale(from_offset, d_h00(s) / duration),
               scale(from_velocity, d_h10(s)));
}

Vec2 undock_impulse(const PortState &target) { return scale(target.normal, UNDOCK_SPEED); }

}  // namespace opra
