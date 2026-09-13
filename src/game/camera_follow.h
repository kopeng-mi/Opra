// The flight camera's follow: a deadzone, a velocity lead and critical damping, in the navigation
// plane only. The camera has no pitch and no yaw control during flight (F1) - it moves in x/y and
// nothing else - so the whole of it is two numbers of state and one pure step.
//
// The step is the numerically stable form of a critically-damped spring, correct at any dt. A naive
// `lerp(a, b, dt * k)` is frame-rate dependent, which is the class of bug this replaces: at 30 fps
// it would lag half a frame behind the ship and at 240 it would not lag at all.
#pragma once

#include <glm/glm.hpp>

#include "core/units.h"
#include "game/config.h"

namespace opra {

/** The follow's tuning. Defaults are config.h's, and all of them want checking against the real
 *  thing: a lead that reads well at 200 m/s reads as a lurch at 2 km/s. */
struct FollowParams {
    double lead_seconds = config::LEAD_SECONDS;
    double lead_max = config::LEAD_MAX;
    double deadzone = config::FOLLOW_DEADZONE;
    double tau = config::FOLLOW_TAU;
    /**
     * Past this separation the camera is put back on the ship outright, with no damping. A warp
     * jump, a reset, a docking handoff: a step that big would otherwise leave the ship outside the
     * frame for a second while the spring caught up.
     */
    double snap_distance = config::FOLLOW_SNAP;
};

/** The camera's own state. `target` is where the view is centred, in world double metres. */
struct FollowState {
    glm::dvec2 target{0.0};
    glm::dvec2 velocity{0.0};
};

/** Where the camera wants to be for this ship state, before the deadzone and the damping. */
glm::dvec2 follow_goal(const glm::dvec2 &ship, const glm::dvec2 &ship_velocity,
                       const FollowParams &params);

/** Puts the camera on the ship with the right lead and no transient. A fresh run, a warp jump. */
void follow_snap(FollowState &state, const glm::dvec2 &ship, const glm::dvec2 &ship_velocity,
                 const FollowParams &params);

/**
 * One look step of the camera's freedom (plan 05 J1): Ctrl and the mouse swing the *eye* about the
 * follow point, inside a cone about the home axis. `anchor_px` is where the cursor was when Ctrl
 * went down; `cursor_px` is where it sits now; both in pixels, with `height` the viewport height.
 * The drag maps to an angular offset - azimuth on x, elevation on y - clamped to the cone, so the
 * frame can swing anywhere within thirty degrees of home and never further, and it can never invert
 * or roll: the collar's world north survives any look.
 *
 * This replaces the plan-04 plane grab: with one continuous zoom the eye has to stay near the
 * follow point at every scale, and an unbounded pan at system zoom would lose the ship outright.
 */
void look_cone_step(glm::dvec2 &cone, double anchor_x, double anchor_y, double cursor_x,
                    double cursor_y, double height);

/** The angle eased back to home once the pilot lets go: exact exp(-dt/tau), frame-rate clean. */
glm::dvec2 look_cone_release(const glm::dvec2 &cone, Real dt, double tau);

/** The cone clamp: a look may lean thirty degrees from home in any direction, and no further. */
glm::dvec2 look_cone_clamp(const glm::dvec2 &cone);

/**
 * The eye offset for a look: the home orbit swung by `cone` radians (x = azimuth about world up,
 * y = elevation). Elevation stays inside the camera's own limits whatever the cone does.
 */
glm::vec3 look_eye(double half_height, float pitch, const glm::dvec2 &cone);

/** One step of the follow. Returns the new centre; `state` is updated in place. */
glm::dvec2 follow_step(FollowState &state, const glm::dvec2 &ship, const glm::dvec2 &ship_velocity,
                       Real dt, const FollowParams &params);

}  // namespace opra
