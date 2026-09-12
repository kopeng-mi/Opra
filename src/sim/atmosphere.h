// The air, where a body has any: density, drag and stagnation-point heating (plan 03 section 3.3).
// Exponential, one scale height per body, and Sutton-Graves for the flux, because the plan's point
// is that reentry and a hard burn compete for the same heat budget the gauge has always shown.
#pragma once

#include <glm/glm.hpp>

#include "core/units.h"
#include "sim/system.h"

namespace opra {

struct SurfaceProfile;

/** Density at `altitude` metres above the datum: rho0 * exp(-h/H), zero above the top. */
double air_density(const Atmosphere &air, double altitude);

/** Sutton-Graves stagnation flux, W/m^2. k is the SI constant; `nose_radius` is in metres. */
double stagnation_flux(double rho, double speed, double nose_radius);

/**
 * Everything one step of air does to a ship. The caller owns the ship: this returns numbers.
 *
 * `offset` and `velocity` are relative to the body, in metres and m/s, in the zone frame. The frame
 * itself co-orbits its anchor and does not rotate, so an acceleration measured here is the same
 * acceleration the zone frame sees.
 */
struct AirStep {
    Vec2 drag{0.0, 0.0};      // acceleration along -v, m/s^2 (>= 0 magnitude)
    double flux = 0.0;        // W/m^2 at the stagnation point
    double density = 0.0;     // kg/m^3, the number the HUD and the map quote
    double altitude = 0.0;    // metres above the ground, terrain included
};

/**
 * `cd_area` is the hull's drag area over its mass, m^2/kg; `nose_radius` its stagnation radius.
 * `surface` may be null for a body with an atmosphere and no terrain.
 */
AirStep air_step(const Body &body, const SurfaceProfile *surface, const glm::dvec2 &offset,
                 const glm::dvec2 &velocity, double cd_area_over_mass, double nose_radius);

/** A predicted atmospheric pass, as the map draws it (plan 3.3). */
struct PredictedPass {
    /** False when the osculating conic never reaches the air, and the map has nothing extra to draw. */
    bool enters = false;
    /** The path, as offsets from the body's centre, in the body's own frame, in metres. */
    std::vector<glm::dvec2> points;
};

/**
 * Integrates the pass forward at coarse steps, but only when the osculating periapsis is below the
 * atmosphere's top: aerobraking is a consequence of the drag model, and what the plan owes it is the
 * prediction - without one, aiming a pass is guesswork.
 *
 * The body's own motion along its orbit is ignored for the duration of the pass, which is minutes;
 * the result is drawn relative to the body for exactly that reason.
 */
PredictedPass predict_pass(const Body &body, const SurfaceProfile *surface, const glm::dvec2 &offset,
                           const glm::dvec2 &velocity, double mu, double cd_area_over_mass,
                           double nose_radius, int steps = 512);

}  // namespace opra
