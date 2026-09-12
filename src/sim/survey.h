// Survey and satellites (plan 03 section 3.7). A pass records an arc when the ship is low and slow;
// a deployment takes the ship's own conic and holds it to a box one period later, so a marginal
// orbit fails honestly rather than at the instant of release.
#pragma once

#include <string>
#include <vector>

#include "sim/world.h"

namespace opra {

/** The band a scan pass has to be flown in, and how slow: it costs fuel and risk, not time. */
inline constexpr double SCAN_MIN_ALTITUDE = 60.0e3;
inline constexpr double SCAN_MAX_ALTITUDE = 400.0e3;
inline constexpr double SCAN_MAX_GROUND_SPEED = 1200.0;

/** The tolerance a deployment holds the satellite to, against the orbit it was released on. */
inline constexpr double SATELLITE_A_TOLERANCE = 0.02;  // fraction of the semi-major axis
inline constexpr double SATELLITE_E_TOLERANCE = 0.05;  // absolute eccentricity

/**
 * Records the arc the ship covered this step, when it is inside the scan band and slow enough.
 * Arcs that overlap the last one are merged, so a hundred passes over one longitude are one arc.
 */
void record_scan(World &world, int body, double altitude, double ground_speed, double theta);

/** True when the body's whole circumference has been mapped. */
bool survey_complete(const World &world, int body);

/** The arcs' total coverage, radians, for the contract row. */
double surveyed_radians(const World &world, int body);

/**
 * Deploys a satellite on the ship's current conic about `body`, named by `contract`. Returns false
 * when there is no conic worth deploying on: the ship must be in orbit, not falling into the air.
 */
bool deploy_satellite(World &world, int body, const std::string &contract);

/**
 * Checks every satellite whose period has elapsed since deployment. A satellite passes when it is
 * still inside its box and its periapsis is still above the air; one whose periapsis is inside the
 * atmosphere is predicted forward and fails if the pass ends on the ground. Returns the contracts
 * that were checked this call, so the caller can say so.
 */
std::vector<std::string> update_satellites(World &world);

/** True when the elements are inside the box the contract names, within tolerance. */
bool inside_box(const orbit::Elements &elements, double box_a, double box_e);

}  // namespace opra
