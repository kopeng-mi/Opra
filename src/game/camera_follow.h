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
 * One step of a hand on the camera: Ctrl and the mouse. Vertical movement raises and lowers the
 * orbit (the pitch is the camera's own angle above the play plane), horizontal movement slides the
 * eye sideways and along the plane without turning it - yaw stays locked to world north, which is
 * what keeps the collar's bearing frame honest. `metres_per_px` is the frame's own scale, so a drag
 * across the screen moves the eye across the same distance whatever the zoom.
 *
 * The pan is an offset on the follow point, not a change to it: release the key and `pan_release`
 * walks it back, so a look is a look and never a new home.
 */
void look_step(float &pitch, glm::dvec2 &pan, const glm::vec2 &delta_px, float metres_per_px,
               double pitch_min, double pitch_max, double pan_limit);

/** The pan eased back to nothing once the pilot lets go of the look key. */
glm::dvec2 pan_release(const glm::dvec2 &pan, Real dt, double tau);

/** One step of the follow. Returns the new centre; `state` is updated in place. */
glm::dvec2 follow_step(FollowState &state, const glm::dvec2 &ship, const glm::dvec2 &ship_velocity,
                       Real dt, const FollowParams &params);

}  // namespace opra
