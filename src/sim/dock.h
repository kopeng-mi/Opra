// Docking: named ports, the approach gate, and the capture that follows it. A port is a hardpoint
// in the model, so a modular ship brings its own (E9) and a station's ports are data like everything
// else. This file is pure geometry and time: no rendering, no world, no device.
#pragma once

#include <string>

#include "core/units.h"

namespace opra {

/** A docking port in the model's frame: where it is, which way it faces, and how big a ship it takes. */
struct Port {
    std::string id;
    Vec2 local_pos;
    /** Unit, outward from the hull: the corridor runs along it. */
    Vec2 local_normal;
    char size_class = 'M';
};

/** A port in the world: where it is and which way it faces, after the owner's pose. */
struct PortState {
    Vec2 position;
    Vec2 normal;
};

/** Class M limits, from the plan's gate table. Tunable per class, not per port. */
struct GateLimits {
    double axial_max = 60.0;      // metres down the corridor
    double lateral_max = 8.0;     // metres off axis
    double closing_max = 1.2;     // m/s toward the port
    double alignment_max = 0.2094;  // radians: 12 degrees
    double rate_max = 0.0524;     // rad/s relative rotation: 3 degrees per second
    /** Every term must hold this long before the capture starts. */
    double hold = 0.4;
};

/** The gate's five terms, each with the verdict the HUD shows. */
struct ApproachGate {
    double axial = 0.0;
    double lateral = 0.0;
    double closing = 0.0;
    double alignment = 0.0;
    double rate = 0.0;
    bool axial_ok = false;
    bool lateral_ok = false;
    bool closing_ok = false;
    bool alignment_ok = false;
    bool rate_ok = false;
    /** All five held: the terms of `held_for` have been satisfied continuously. */
    bool held = false;
    /** Seconds the terms have held, carried by the caller between frames. */
    double held_for = 0.0;

    bool all_ok() const;
};

/**
 * One frame of the approach: `from` is the ship's port, `to` the target's, `relative_spin` the
 * difference in angular velocity, `dt` this frame's step. `held_for` is last frame's value, so the
 * hold is continuous and a single good frame does not capture the ship.
 */
ApproachGate evaluate_gate(const PortState &from, const Vec2 &from_velocity, const PortState &to,
                           const Vec2 &to_velocity, double relative_spin, double held_for, double dt,
                           const GateLimits &limits = {});

/**
 * The capture: a cubic Hermite in the port's frame from where the ship was when the gate closed to
 * hard dock, with the ship's own closing velocity matched at the start and nothing at the end.
 */
struct Capture {
    double duration = 2.0;
    /** Ship port position relative to the target port at the moment of capture. */
    Vec2 from_offset;
    /** Relative velocity at that moment. */
    Vec2 from_velocity;

    /** The ship's port offset from the target port, `t` seconds in. */
    Vec2 offset_at(double t) const;
    /** Its velocity, which the sim uses to keep the ship's state consistent. */
    Vec2 velocity_at(double t) const;
};

/** The separation impulse of an undock: 0.5 m/s along the target port's normal. */
Vec2 undock_impulse(const PortState &target);

inline constexpr double UNDOCK_SPEED = 0.5;

}  // namespace opra
