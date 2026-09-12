// Powered descent (plan 03 section 3.4): the hoverslam solution the director cues, and the gate that
// decides whether a contact is a landing or a crash. Both are pure: given a ship state and a body,
// they return numbers the HUD, the director and the world can each read.
#pragma once

#include <glm/glm.hpp>

#include "core/units.h"
#include "sim/physics.h"
#include "sim/terrain.h"

namespace opra {

/** The plan's touchdown limits, and they are limits, not targets: every one is a hull. */
inline constexpr Real TOUCHDOWN_VERTICAL = 4.0;    // m/s, along local up
inline constexpr Real TOUCHDOWN_LATERAL = 1.5;     // m/s, across it
inline constexpr Real TOUCHDOWN_TILT_DEG = 8.0;    // the hull's axis from local up
inline constexpr Real TOUCHDOWN_OFFSET = 12.0;     // metres from the pad's centre
inline constexpr Real TOUCHDOWN_RATE_DEG = 4.0;    // degrees per second of roll about local up
/** Hull per unit of relative excess: a term twice its limit costs 12, four times it costs 36. */
inline constexpr Real TOUCHDOWN_DAMAGE = 12.0;

/** The five terms, in the plan's order, plus what a failed one costs. */
struct TouchdownGate {
    bool pad = false;         // the contact is inside a declared pad
    double vertical = 0.0;    // m/s along local up (closing)
    double lateral = 0.0;     // m/s across it
    double tilt_deg = 0.0;    // hull axis from local up
    double offset = 0.0;      // metres from the pad's centre along the surface
    double rate_deg = 0.0;    // degrees per second
    /** Sum of (value/limit - 1) over the terms that are outside their limit; 0 when all pass. */
    double excess = 0.0;
    bool ok() const { return excess <= 0.0; }
};

/**
 * Evaluates the gate for a ship touching `body` at `relative` (its offset from the body's centre).
 * `pad` is the pad index the contact landed in, or -1 for open ground: open ground is allowed and
 * costs hull in proportion to the local slope, exactly as the plan says.
 */
TouchdownGate touchdown_gate(const ShipState &state, const glm::dvec2 &relative, const Body &body,
                             const SurfaceProfile &profile, int pad);

/** The hoverslam solution: how high the burn starts and how long it lasts. */
struct Hoverslam {
    bool useful = false;    // false when the drive cannot beat gravity, or nothing is falling
    double altitude = 0.0;  // metres above the ground at which to light it
    double seconds = 0.0;   // seconds of burn
};

/** `a_max` is the hull's maximum deceleration, `gravity` the local pull, both m/s^2. */
Hoverslam hoverslam(double descent_speed, double a_max, double gravity);

}  // namespace opra
