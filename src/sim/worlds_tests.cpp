#include "sim/worlds_tests.h"

#include <cmath>

#include "selftest.h"
#include "sim/atmosphere.h"
#include "sim/descent.h"
#include "sim/survey.h"
#include "sim/terrain.h"

namespace opra {
namespace {

using selftest::check;
using selftest::check_close;

constexpr double kPi = 3.141592653589793;

/** Tessera, as the system file declares it: the atmosphere and the pads the rules are sized for. */
Body tessera() {
    Body body;
    body.id = "tessera";
    body.name = "Tessera";
    body.kind = BodyClass::Planet;
    body.mu = 1.9e13;
    body.radius = 6.3e6;
    body.atmosphere.present = true;
    body.atmosphere.rho0 = 1.10;
    body.atmosphere.scale_height = 7800.0;
    body.atmosphere.top = 110000.0;
    body.terrain.present = true;
    body.terrain.seed = 8801;
    body.terrain.amplitude = 0.0035;
    body.terrain.pads.push_back({1.20, 2600.0, "Tessera Downport"});
    return body;
}

void test_terrain() {
    const Body body = tessera();
    const SurfaceProfile profile = generate_surface(body);
    check(profile.height.size() == static_cast<size_t>(TERRAIN_SAMPLES),
          "terrain: the profile has the sample count the shader shares");

    // Deterministic: the same file generates the same ground, every run.
    const SurfaceProfile again = generate_surface(body);
    bool identical = true;
    for (size_t i = 0; i < profile.height.size(); ++i) {
        if (profile.height[i] != again.height[i]) identical = false;
    }
    check(identical, "terrain: the same seed generates the same ground");

    // The octaves stay inside the amplitude they are given: six of them at gain 0.55 sum to under
    // 2.4 A, and a body whose relief ran away from its declared amplitude would be a different world.
    double peak = 0.0;
    for (double h : profile.height) peak = std::max(peak, std::fabs(h));
    check(peak > 0.0 && peak < body.radius * body.terrain.amplitude * 2.5,
          "terrain: the relief stays inside the declared amplitude");

    // The pad is flat across its span and steps away outside it, with the taper doing the join.
    const Pad &pad = body.terrain.pads[0];
    const double half_span = pad.width / (2.0 * body.radius);
    const double at_centre = terrain_height(profile, pad.theta);
    check_close(terrain_height(profile, pad.theta + half_span * 0.6), at_centre, 1.0e-9,
                "terrain: a pad is flat across its span");
    check_close(terrain_height(profile, pad.theta - half_span * 0.9), at_centre, 1.0e-9,
                "terrain: the flat reaches the pad's own edge");
    bool steps = false;
    for (double away = 3.0; away < 12.0 && !steps; away += 1.0) {
        if (std::fabs(terrain_height(profile, pad.theta + half_span * away) - at_centre) > 1.0) {
            steps = true;
        }
    }
    check(steps, "terrain: the ground outside the pad is not the pad");
    check(pad_index_at(body, pad.theta) == 0 && pad_index_at(body, pad.theta + 3.0) == -1,
          "terrain: the pad's own arc is the only place that counts as a pad");

    // And the sample the bracket lands on is the sample the height comes from.
    check_close(terrain_height(profile, 0.0), profile.height[0], 1.0e-9,
                "terrain: an exact sample returns its own height");
    check(std::fabs(terrain_slope(profile, 0.0)) < kPi * 0.5, "terrain: the slope is an angle");
}

void test_air() {
    const Body body = tessera();
    const SurfaceProfile profile = generate_surface(body);
    const Atmosphere &air = body.atmosphere;

    check_close(air_density(air, 0.0), air.rho0, 1.0e-12, "air: the datum is rho0");
    check_close(air_density(air, air.scale_height), air.rho0 / std::exp(1.0), 1.0e-12,
                "air: one scale height is one e-fold");
    check(air_density(air, air.top) == 0.0 && air_density(air, air.top * 2.0) == 0.0,
          "air: nothing above the top");

    // Drag only exists where there is air, and it opposes the motion.
    const glm::dvec2 out{body.radius + 60000.0, 0.0};
    const glm::dvec2 moving{0.0, 3000.0};
    const AirStep step = air_step(body, &profile, out, moving, DRAG_CD_AREA_OVER_MASS, NOSE_RADIUS);
    check(step.density > 0.0 && step.drag.x == 0.0 && step.drag.y < 0.0,
          "air: drag opposes the velocity that causes it");
    check(step.flux > 0.0, "air: a fast pass through air heats the hull");
    // From the ground, not from the mean radius: the heightfield is a kilometre-scale thing here,
    // and an entry that started at the wrong datum would start at the wrong time.
    check_close(step.altitude, 60000.0 - terrain_height(profile, 0.0), 1.0e-6,
                "air: altitude is measured from the ground");

    const AirStep vacuum =
        air_step(body, &profile, {body.radius * 4.0, 0.0}, moving, DRAG_CD_AREA_OVER_MASS,
                 NOSE_RADIUS);
    check(vacuum.density == 0.0 && vacuum.drag.y == 0.0 && vacuum.flux == 0.0,
          "air: above the top there is nothing");

    // The pass prediction only exists for a conic that reaches the air, so this one starts well
    // above the top instead of inside it.
    const glm::dvec2 high{body.radius + body.atmosphere.top + 400000.0, 0.0};
    const glm::dvec2 circular{0.0, std::sqrt(body.mu / glm::length(high))};
    check(!predict_pass(body, &profile, high, circular, body.mu, DRAG_CD_AREA_OVER_MASS,
                        NOSE_RADIUS)
               .enters,
          "air: a conic that clears the air has no pass to predict");
    // A low periapsis does dip in, and the pass has the shape of one.
    const glm::dvec2 low{body.radius + 70000.0, 0.0};
    const glm::dvec2 fast{0.0, std::sqrt(body.mu / glm::length(low)) * 0.97};
    const PredictedPass pass =
        predict_pass(body, &profile, low, fast, body.mu, DRAG_CD_AREA_OVER_MASS, NOSE_RADIUS);
    check(pass.enters && pass.points.size() > 2, "air: a dipping conic predicts a pass");
    bool closing = true;
    for (size_t i = 1; i < pass.points.size(); ++i) {
        if (glm::length(pass.points[i]) > glm::length(pass.points[i - 1]) + 1.0) closing = false;
    }
    check(closing, "air: the predicted pass descends before it climbs");
}

void test_descent() {
    const Body body = tessera();
    const SurfaceProfile profile = generate_surface(body);
    const Pad &pad = body.terrain.pads[0];

    // A gentle set-down inside the gate: inside every limit, and it costs nothing.
    ShipState gentle;
    gentle.position = {0.0, 0.0};
    gentle.velocity = {-0.5, 0.2};
    // The hull's nose is (sin a, cos a) and its heading in the atan2 sense is pi/2 - a, so
    // a = pi/2 - theta puts the nose along local up and the tilt term at zero.
    gentle.angle = kPi * 0.5 - pad.theta;
    gentle.angularVelocity = 0.01;
    const glm::dvec2 at =
        glm::dvec2(terrain_radius(profile, pad.theta) * std::cos(pad.theta),
                   terrain_radius(profile, pad.theta) * std::sin(pad.theta));
    const TouchdownGate clean = touchdown_gate(gentle, at, body, profile, 0);
    check(clean.pad && clean.ok(), "descent: a gentle set-down on a pad is a landing");
    check(clean.excess == 0.0, "descent: a landing inside every limit costs nothing");

    // A slam: the excess scales, and the damage with it.
    ShipState slam = gentle;
    slam.velocity = {-14.0, 0.0};
    const TouchdownGate hard = touchdown_gate(slam, at, body, profile, 0);
    check(!hard.ok() && hard.excess > 1.0, "descent: a fourteen metre slam is outside the gate");
    check(hard.excess * TOUCHDOWN_DAMAGE > 12.0, "descent: the damage scales with the excess");

    // Off the pad: allowed, and it costs hull in proportion to the slope.
    const double off = pad.theta + 0.9;
    const glm::dvec2 rough{terrain_radius(profile, off) * std::cos(off),
                           terrain_radius(profile, off) * std::sin(off)};
    const TouchdownGate open = touchdown_gate(gentle, rough, body, profile, -1);
    check(!open.pad && open.excess >= 0.0, "descent: open ground is still a landing, with a price");

    // The hoverslam: the plan's own two formulas.
    const Hoverslam burn = hoverslam(100.0, 16.45, 6.45);
    check(burn.useful, "descent: a drive that beats gravity has a hoverslam");
    check_close(burn.altitude, 100.0 * 100.0 / (2.0 * 10.0), 1.0e-6,
                "descent: the burn starts at v^2 / 2a_net");
    check_close(burn.seconds, 10.0, 1.0e-9, "descent: and it lasts v / a_net");
    check(!hoverslam(100.0, 6.0, 6.0).useful, "descent: no hoverslam when the drive cannot win");
    check(!hoverslam(0.0, 16.45, 6.45).useful, "descent: nothing to do while not descending");
}

void test_survey() {
    World world;
    world.system.bodies.push_back(tessera());
    world.bodies.resize(1);
    world.scanned.assign(1, {});

    // A pass inside the band and under the speed limit records; anything else does not.
    record_scan(world, 0, 1.0e5, 900.0, 0.10);
    check_close(surveyed_radians(world, 0), 2.0 * 1.0e-4, 1.0e-12, "survey: a low slow pass records");
    record_scan(world, 0, 1.0e6, 900.0, 1.00);
    check_close(surveyed_radians(world, 0), 2.0 * 1.0e-4, 1.0e-12,
                "survey: a pass outside the band records nothing");
    record_scan(world, 0, 1.0e5, 9000.0, 2.00);
    check_close(surveyed_radians(world, 0), 2.0 * 1.0e-4, 1.0e-12,
                "survey: a fast pass records nothing");

    // Adjacent passes merge, and the seam closes because the arcs are merged as spans.
    for (int i = 0; i < 20000; ++i) {
        record_scan(world, 0, 1.0e5, 900.0, static_cast<double>(i) * 1.0e-4);
    }
    check(surveyed_radians(world, 0) > 2.0 * 1.0e-4, "survey: consecutive samples merge into a pass");
    check(!survey_complete(world, 0), "survey: one pass is not the whole body");

    // A deployment on a real orbit succeeds and is checked a period later; a decaying one fails.
    world.elapsed = 0.0;
    const Body &body = world.system.bodies[0];
    world.anchor.position = glm::dvec2(0.0);
    world.bodies[0].position = glm::dvec2(0.0);
    world.bodies[0].velocity = glm::dvec2(0.0);
    world.ship.position = Vec2{body.radius + 400000.0, 0.0};
    world.ship.velocity = Vec2{0.0, std::sqrt(body.mu / (body.radius + 400000.0))};
    check(deploy_satellite(world, 0, "SAT-1"), "survey: a circular orbit is a deployment");
    check(update_satellites(world).empty(), "survey: a fresh satellite is not checked early");

    const double period = orbit::period(world.satellites[0].elements);
    world.elapsed = period + 1.0;
    const std::vector<std::string> checked = update_satellites(world);
    check(checked.size() == 1 && world.satellites[0].checked && world.satellites[0].valid,
          "survey: a stable orbit passes its check one period later");

    // A decaying release: apoapsis where the ship is, periapsis ninety kilometres up, which is
    // inside the air. The orbit is real, so it deploys; the air is above its periapsis, so the
    // check one period later fails it.
    world.satellites.clear();
    world.elapsed = 0.0;
    const double r_apo = body.radius + 300000.0;
    const double r_peri = body.radius + 90000.0;
    const double semi = (r_apo + r_peri) * 0.5;
    world.ship.position = Vec2{r_apo, 0.0};
    world.ship.velocity = Vec2{0.0, std::sqrt(body.mu * (2.0 / r_apo - 1.0 / semi))};
    world.surfaces.assign(1, generate_surface(body));
    check(deploy_satellite(world, 0, "SAT-2"), "survey: an eccentric orbit is still a deployment");
    world.elapsed = orbit::period(world.satellites[0].elements) + 1.0;
    update_satellites(world);
    check(world.satellites[0].checked && !world.satellites[0].valid,
          "survey: a release the air is eating fails its check");
}

}  // namespace

int worlds_tests() {
    const int before = selftest::failures();
    test_terrain();
    test_air();
    test_descent();
    test_survey();
    return selftest::failures() - before;
}

}  // namespace opra
