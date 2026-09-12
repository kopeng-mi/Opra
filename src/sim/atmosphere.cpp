#include "sim/atmosphere.h"

#include <cmath>
#include <vector>

#include "orbit/kepler.h"

#include "sim/terrain.h"

namespace opra {
namespace {

/** Sutton-Graves, SI: q = k * sqrt(rho / R_nose) * v^3, W/m^2. */
constexpr double SUTTON_GRAVES_K = 1.7415e-4;

}  // namespace

double air_density(const Atmosphere &air, double altitude) {
    if (!air.present) return 0.0;
    if (altitude >= air.top) return 0.0;
    if (altitude <= 0.0) return air.rho0;  // below the datum the model stops being an atmosphere
    return air.rho0 * std::exp(-altitude / air.scale_height);
}

double stagnation_flux(double rho, double speed, double nose_radius) {
    if (rho <= 0.0 || speed <= 0.0 || nose_radius <= 0.0) return 0.0;
    return SUTTON_GRAVES_K * std::sqrt(rho / nose_radius) * speed * speed * speed;
}

AirStep air_step(const Body &body, const SurfaceProfile *surface, const glm::dvec2 &offset,
                 const glm::dvec2 &velocity, double cd_area_over_mass, double nose_radius) {
    AirStep step;
    if (!body.atmosphere.present) return step;

    const double distance = glm::length(offset);
    // The ground under the ship, not the mean radius: on Tessera the two differ by kilometres, and
    // an altitude measured from the wrong one is an entry that starts at the wrong time.
    const double ground =
        body.radius + (surface ? terrain_height(*surface, std::atan2(offset.y, offset.x)) : 0.0);
    step.altitude = distance - ground;
    step.density = air_density(body.atmosphere, step.altitude);
    if (step.density <= 0.0) return step;

    const double speed = glm::length(velocity);
    if (speed < 1.0e-6) return step;

    const double decel = 0.5 * step.density * speed * speed * cd_area_over_mass;
    step.drag = Vec2{-velocity.x / speed * decel, -velocity.y / speed * decel};
    step.flux = stagnation_flux(step.density, speed, nose_radius);
    return step;
}

PredictedPass predict_pass(const Body &body, const SurfaceProfile *surface, const glm::dvec2 &offset,
                           const glm::dvec2 &velocity, double mu, double cd_area_over_mass,
                           double nose_radius, int steps) {
    PredictedPass pass;
    if (!body.atmosphere.present || mu <= 0.0 || steps <= 1) return pass;

    // A conic that stays above the top is exactly the conic the map already draws. Only a pass that
    // dips into the air is worth integrating, and that is what the osculating periapsis says.
    const orbit::Elements elements = orbit::from_state(offset, velocity, mu, 0.0);
    if (orbit::periapsis(elements) > body.radius + body.atmosphere.top) return pass;

    // Coarse by construction: the step is a fraction of the local orbital timescale, so it is small
    // where the ship is fast and deep, and large where it is not. What the plan asks for is the
    // shape of the pass - whether the aerobrake leaves an orbit worth having - not an ephemeris.
    glm::dvec2 r = offset;
    glm::dvec2 v = velocity;
    bool inside = false;
    for (int i = 0; i < steps; ++i) {
        const double radius = glm::length(r);
        if (radius <= 1.0) break;
        const double ground =
            body.radius + (surface ? terrain_height(*surface, std::atan2(r.y, r.x)) : 0.0);
        const double altitude = radius - ground;
        if (altitude <= body.atmosphere.top) inside = true;
        if (inside) {
            pass.points.push_back(r);
            // Out of the air and climbing: the pass is over, and everything after it is a conic the
            // map already knows how to draw.
            if (altitude > body.atmosphere.top && (r.x * v.x + r.y * v.y) > 0.0) break;
        }
        if (altitude < 0.0) break;  // the predicted pass ends on the ground, not through it

        const AirStep air = air_step(body, surface, r, v, cd_area_over_mass, nose_radius);
        const double dt = 0.002 * std::cbrt(radius * radius * radius / mu);
        const double pull = -mu / (radius * radius * radius);
        v.x += (pull * r.x + air.drag.x) * dt;
        v.y += (pull * r.y + air.drag.y) * dt;
        r.x += v.x * dt;
        r.y += v.y * dt;
    }
    pass.enters = !pass.points.empty();
    if (!pass.enters) pass.points.clear();
    return pass;
}

}  // namespace opra
