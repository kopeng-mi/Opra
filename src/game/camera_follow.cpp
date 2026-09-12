#include "game/camera_follow.h"

#include <algorithm>

namespace opra {
namespace {

/** The critically-damped step, in the form that is stable at any dt. `decay` is the reciprocal of
 *  the quartic that the closed-form integral of the spring produces. */
glm::dvec2 follow_toward(FollowState &state, const glm::dvec2 &goal, Real dt,
                         const FollowParams &params) {
    const double omega = 2.0 / params.tau;
    const double x = omega * static_cast<double>(dt);
    const double decay = 1.0 / (1.0 + x + 0.48 * x * x + 0.235 * x * x * x);
    const glm::dvec2 change = state.target - goal;
    const glm::dvec2 temp = (state.velocity + change * omega) * static_cast<double>(dt);
    state.velocity = (state.velocity - temp * omega) * decay;
    state.target = goal + (change + temp) * decay;
    return state.target;
}

}  // namespace

glm::dvec2 follow_goal(const glm::dvec2 &ship, const glm::dvec2 &ship_velocity,
                       const FollowParams &params) {
    const double speed = glm::length(ship_velocity);
    if (speed < 1.0e-9) return ship;
    // Along the velocity and capped: at 3 km/s an uncapped 0.6 s lead would centre the view 1.8 km
    // ahead of the ship, which is a different screen, not a framing.
    const double lead = std::min(speed * params.lead_seconds, params.lead_max);
    return ship + ship_velocity * (lead / speed);
}

void follow_snap(FollowState &state, const glm::dvec2 &ship, const glm::dvec2 &ship_velocity,
                 const FollowParams &params) {
    state.target = follow_goal(ship, ship_velocity, params);
    // The camera is already moving with the ship after a snap, so the spring does not have to
    // re-accelerate from a standstill and the first frames do not visibly lurch.
    state.velocity = ship_velocity;
}

void look_step(float &pitch, glm::dvec2 &pan, const glm::vec2 &delta_px, float metres_per_px,
               double pitch_min, double pitch_max, double pan_limit) {
    // Down is positive in both: dragging the world down raises the camera, which is the gesture a
    // turntable has used for as long as there have been turntables.
    pitch = static_cast<float>(std::clamp(
        static_cast<double>(pitch) + static_cast<double>(delta_px.y) * 0.005, pitch_min, pitch_max));
    pan.x += static_cast<double>(delta_px.x) * metres_per_px;
    pan.y -= static_cast<double>(delta_px.y) * metres_per_px;
    const double reach = glm::length(pan);
    if (reach > pan_limit) pan *= pan_limit / reach;
}

glm::dvec2 pan_release(const glm::dvec2 &pan, Real dt, double tau) {
    if (tau <= 0.0) return glm::dvec2(0.0);
    const double rate = 1.0 / (1.0 + static_cast<double>(dt) / tau);
    return pan * rate;
}

glm::dvec2 follow_step(FollowState &state, const glm::dvec2 &ship, const glm::dvec2 &ship_velocity,
                       Real dt, const FollowParams &params) {
    if (glm::length(ship - state.target) > params.snap_distance) {
        follow_snap(state, ship, ship_velocity, params);
        return state.target;
    }
    const glm::dvec2 goal = follow_goal(ship, ship_velocity, params);
    // The deadzone is measured from where the camera *is*: inside it the camera does not move at
    // all, so a drift shorter than the zone does not slide the whole frame.
    const glm::dvec2 offset = goal - state.target;
    const double distance = glm::length(offset);
    const glm::dvec2 aim = distance <= params.deadzone
                               ? state.target
                               : state.target + offset * (1.0 - params.deadzone / distance);
    return follow_toward(state, aim, dt, params);
}

}  // namespace opra
