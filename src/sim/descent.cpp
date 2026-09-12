#include "sim/descent.h"

#include <algorithm>
#include <cmath>

namespace opra {
namespace {

/** How far one term is past its limit, as a fraction of the limit. Zero when it is inside. */
double over(double value, double limit) {
    if (limit <= 0.0) return 0.0;
    return std::max(0.0, std::fabs(value) / limit - 1.0);
}

}  // namespace

TouchdownGate touchdown_gate(const ShipState &state, const glm::dvec2 &relative, const Body &body,
                             const SurfaceProfile &profile, int pad) {
    TouchdownGate gate;
    gate.pad = pad >= 0;

    const double radius = glm::length(relative);
    if (radius < 1.0) return gate;
    const double theta = std::atan2(relative.y, relative.x);
    // Local up is the radial direction at the contact, and the surface's own normal is close enough
    // to it that using the radial one costs less than the noise it would take to do better.
    const glm::dvec2 up{std::cos(theta), std::sin(theta)};
    const glm::dvec2 tangent{-up.y, up.x};

    // Sign convention: descending is positive, because every limit in the table is about arriving.
    gate.vertical = -(state.velocity.x * up.x + state.velocity.y * up.y);
    gate.lateral = state.velocity.x * tangent.x + state.velocity.y * tangent.y;

    // The hull's own axis, from the body's local up: the nose points along `angle`.
    const double axis = std::atan2(std::cos(state.angle), std::sin(state.angle));
    const double up_angle = std::atan2(up.y, up.x);
    double tilt = std::fmod(axis - up_angle, 6.283185307179586);
    if (tilt > 3.141592653589793) tilt -= 6.283185307179586;
    if (tilt < -3.141592653589793) tilt += 6.283185307179586;
    gate.tilt_deg = std::fabs(tilt) * 57.29577951308232;
    gate.rate_deg = std::fabs(state.angularVelocity) * 57.29577951308232;

    if (gate.pad) {
        const Pad &landing = body.terrain.pads[static_cast<size_t>(pad)];
        gate.offset = std::fabs(std::atan2(std::sin(theta - landing.theta),
                                           std::cos(theta - landing.theta))) *
                      radius;
        gate.excess += over(gate.vertical, TOUCHDOWN_VERTICAL);
        gate.excess += over(gate.lateral, TOUCHDOWN_LATERAL);
        gate.excess += over(gate.tilt_deg, TOUCHDOWN_TILT_DEG);
        gate.excess += over(gate.offset, TOUCHDOWN_OFFSET);
        gate.excess += over(gate.rate_deg, TOUCHDOWN_RATE_DEG);
        return gate;
    }

    // Open ground: allowed, and it costs hull in proportion to what it is like to stand on. A
    // twenty degree slope is 2.5 units and 30 hull; a flat plain is a landing.
    const double slope_deg = std::fabs(terrain_slope(profile, theta)) * 57.29577951308232;
    gate.excess += slope_deg / TOUCHDOWN_TILT_DEG;
    gate.excess += over(gate.lateral, TOUCHDOWN_LATERAL);
    gate.excess += over(gate.tilt_deg, TOUCHDOWN_TILT_DEG);
    return gate;
}

Hoverslam hoverslam(double descent_speed, double a_max, double gravity) {
    Hoverslam burn;
    const double net = a_max - gravity;
    // A drive that cannot beat gravity has no hoverslam: the honest answer is that there is no
    // solution, not an infinite altitude.
    if (net <= 0.0 || descent_speed <= 0.0) return burn;
    burn.useful = true;
    burn.altitude = descent_speed * descent_speed / (2.0 * net);
    burn.seconds = descent_speed / net;
    return burn;
}

}  // namespace opra
