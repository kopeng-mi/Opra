#include "game/camera_follow.h"

#include <algorithm>
#include <cmath>

#include "game/config.h"
#include "render/camera.h"

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

glm::dvec2 look_cone_clamp(const glm::dvec2 &cone) {
    // The cone (J1): thirty degrees from home in *any* direction, so the clamp is on the magnitude
    // of the two-axis offset, not on each axis alone - a diagonal look reaches thirty degrees too.
    const double limit = config::LOOK_CONE_DEG * 3.14159265358979323846 / 180.0;
    const double angle = glm::length(cone);
    if (angle <= limit || angle <= 0.0) return cone;
    return cone * (limit / angle);
}

void look_cone_step(glm::dvec2 &cone, double anchor_x, double anchor_y, double cursor_x,
                    double cursor_y, double height) {
    if (!(height > 0.0)) return;
    // Drag to angle: the same scale at every zoom, because the cone is a degree of freedom about
    // the home axis and not a measure of world space (J1).
    const glm::dvec2 drag(cursor_x - anchor_x, cursor_y - anchor_y);
    cone = look_cone_clamp(drag * (config::LOOK_RADIANS_PER_PIXEL * 900.0 / height));
}

glm::dvec2 look_cone_release(const glm::dvec2 &cone, Real dt, double tau) {
    if (tau <= 0.0) return glm::dvec2(0.0);
    return cone * std::exp(-static_cast<double>(dt) / tau);
}

glm::vec3 look_eye(double half_height, float pitch, const glm::dvec2 &cone) {
    // The home orbit, swung: elevation first (the cone's y leans the orbit up or down, clamped so
    // the camera's own limits hold), then azimuth about world up. North stays screen-up because
    // the cone never rolls and never crosses the pole - the up vector is always +Y.
    constexpr float DEG = 3.14159265358979323846f / 180.0f;
    const float elevation = glm::clamp(pitch + static_cast<float>(cone.y), 8.0f * DEG, 88.0f * DEG);
    const glm::vec3 home = orbit_eye(half_height, elevation);
    const float azimuth = static_cast<float>(cone.x);
    const float c = std::cos(azimuth), s = std::sin(azimuth);
    return {home.x * c - home.y * s, home.x * s + home.y * c, home.z};
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
