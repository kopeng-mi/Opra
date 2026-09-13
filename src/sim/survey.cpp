#include "sim/survey.h"

#include <algorithm>
#include <cmath>

#include "sim/atmosphere.h"
#include "sim/terrain.h"

namespace opra {
namespace {

constexpr double TWO_PI = 6.283185307179586;
constexpr double PI = 3.14159265358979323846;
/** Half a sample wide: consecutive steps of a pass are far closer than this, so they merge. */
constexpr double SCAN_ARC = 1.0e-4;

void merge_arc(std::vector<SurveyArc> &arcs, double from, double to) {
    arcs.push_back({from, to});
    std::sort(arcs.begin(), arcs.end(),
              [](const SurveyArc &a, const SurveyArc &b) { return a.from < b.from; });
    std::vector<SurveyArc> merged;
    for (const SurveyArc &arc : arcs) {
        if (!merged.empty() && arc.from <= merged.back().to + 1e-9) {
            merged.back().to = std::max(merged.back().to, arc.to);
            continue;
        }
        merged.push_back(arc);
    }
    arcs.swap(merged);
}

/**
 * One period with the air, when the periapsis is inside it. A satellite released into a decaying
 * orbit is exactly the marginal case the plan wants failed honestly: a pure conic would pass the
 * check for ever, and a drag-integrated one drifts out of its own box.
 */
orbit::Elements drift_one_period(const Body &body, const SurfaceProfile *surface,
                                 const orbit::Elements &elements, double mu) {
    const double period = orbit::period(elements);
    if (!std::isfinite(period) || period <= 0.0) return elements;
    const double altitude = orbit::periapsis(elements) - body.radius;
    if (!body.atmosphere.present || altitude > body.atmosphere.top) return elements;

    orbit::State state = orbit::state_at(elements, 0.0);
    glm::dvec2 r = state.r;
    glm::dvec2 v = state.v;
    // The same coarse scheme the pass prediction uses: a fraction of the local orbital timescale,
    // so the step is small where the satellite is fast and deep.
    const int steps = 4096;
    for (int i = 0; i < steps; ++i) {
        const double radius = glm::length(r);
        if (radius <= 1.0) break;
        const AirStep air = air_step(body, surface, r, v, DRAG_CD_AREA_OVER_MASS, NOSE_RADIUS);
        const double dt = period / static_cast<double>(steps);
        const double pull = -mu / (radius * radius * radius);
        v.x += (pull * r.x + air.drag.x) * dt;
        v.y += (pull * r.y + air.drag.y) * dt;
        r.x += v.x * dt;
        r.y += v.y * dt;
    }
    return orbit::from_state(r, v, mu, 0.0);
}

}  // namespace

void record_scan(World &world, int body, double altitude, double ground_speed, double theta) {
    if (body < 0 || static_cast<size_t>(body) >= world.scanned.size()) return;
    if (altitude < SCAN_MIN_ALTITUDE || altitude > SCAN_MAX_ALTITUDE) return;
    if (ground_speed > SCAN_MAX_GROUND_SPEED) return;
    merge_arc(world.scanned[static_cast<size_t>(body)], theta - SCAN_ARC, theta + SCAN_ARC);
    note_scan_theta(world, body, theta);
}

double surveyed_radians(const World &world, int body) {
    if (body < 0 || static_cast<size_t>(body) >= world.scanned.size()) return 0.0;
    double total = 0.0;
    for (const SurveyArc &arc : world.scanned[static_cast<size_t>(body)]) {
        total += std::max(0.0, arc.to - arc.from);
    }
    return total;
}

bool survey_complete(const World &world, int body) {
    return surveyed_radians(world, body) >= TWO_PI - 1e-6;
}

bool inside_box(const orbit::Elements &elements, double box_a, double box_e) {
    if (box_a <= 0.0) return false;
    if (std::fabs(elements.a - box_a) > std::fabs(box_a) * SATELLITE_A_TOLERANCE) return false;
    return std::fabs(elements.e - box_e) <= SATELLITE_E_TOLERANCE;
}

bool deploy_satellite(World &world, int body, const std::string &contract) {
    if (body < 0 || static_cast<size_t>(body) >= world.system.bodies.size()) return false;
    const Body &primary = world.system.bodies[static_cast<size_t>(body)];
    if (primary.mu <= 0.0) return false;

    const glm::dvec2 relative = world.system_position() - world.bodies[static_cast<size_t>(body)].position;
    const glm::dvec2 velocity = world.system_velocity() - world.bodies[static_cast<size_t>(body)].velocity;
    const orbit::Elements elements = orbit::from_state(relative, velocity, primary.mu, world.elapsed);
    // Not an orbit, or one whose periapsis is in the air: there is nothing to deploy onto.
    if (!orbit::is_elliptic(elements)) return false;
    if (orbit::periapsis(elements) < primary.radius) return false;

    Satellite satellite;
    satellite.contract = contract;
    satellite.body = body;
    satellite.elements = elements;
    satellite.deployed_at = world.elapsed;
    satellite.box_a = elements.a;
    satellite.box_e = elements.e;
    world.satellites.push_back(satellite);
    return true;
}

std::vector<World::Discovery> generate_discoveries(const SystemDef &system) {
    std::vector<World::Discovery> out;
    static const char *kinds[] = {"derelict", "anomaly", "depot"};
    static const char *names[] = {"cold hulk", "mass anomaly", "cached depot"};
    // One candidate for every body with ground, at a longitude hashed from the body's own seed:
    // the same file always draws the same thing to find, and a second system is a second set.
    for (size_t i = 1; i < system.bodies.size(); ++i) {
        const Body &body = system.bodies[i];
        if (!body.terrain.present) continue;
        const unsigned int hash = body.terrain.seed * 2654435761u;
        World::Discovery discovery;
        discovery.kind = kinds[hash % 3];
        discovery.name = std::string(names[hash % 3]) + " - " + body.name;
        discovery.body = static_cast<int>(i);
        discovery.theta = (static_cast<double>(hash >> 8) / 16777216.0) * TWO_PI;
        out.push_back(std::move(discovery));
    }
    return out;
}

void note_scan_theta(World &world, int body, double theta) {
    for (World::Discovery &discovery : world.discoveries) {
        if (discovery.found || discovery.body != body) continue;
        double delta = std::fabs(theta - discovery.theta);
        if (delta > PI) delta = TWO_PI - delta;
        if (delta <= SCAN_ARC * 2.0) discovery.found = true;
    }
}

std::vector<std::string> update_satellites(World &world) {
    std::vector<std::string> checked;
    for (Satellite &satellite : world.satellites) {
        if (satellite.checked) continue;
        const double period = orbit::period(satellite.elements);
        if (!std::isfinite(period) || period <= 0.0) continue;
        if (world.elapsed - satellite.deployed_at < period) continue;

        const Body &body = world.system.bodies[static_cast<size_t>(satellite.body)];
        const SurfaceProfile *surface = static_cast<size_t>(satellite.body) < world.surfaces.size()
                                            ? &world.surfaces[static_cast<size_t>(satellite.body)]
                                            : nullptr;
        const orbit::Elements drifted =
            drift_one_period(body, surface, satellite.elements, body.mu);
        // Two ways to fail, and both of them are the contract's: it drifted out of the box it was
        // released on, or the air is eating it.
        satellite.valid = inside_box(drifted, satellite.box_a, satellite.box_e) &&
                          orbit::periapsis(drifted) > body.radius;
        satellite.checked = true;
        checked.push_back(satellite.contract);
    }
    return checked;
}

}  // namespace opra
