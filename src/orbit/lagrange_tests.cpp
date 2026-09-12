#include "orbit/lagrange_tests.h"

#include <cmath>
#include <vector>

#include <glm/glm.hpp>

#include "orbit/kepler.h"
#include "orbit/lagrange.h"
#include "selftest.h"
#include "sim/system.h"

// Every expected number below is derived on paper from PLAN-03 §3.5 and written into the comment
// beside it - nothing is captured from the run it tests. Run through `Opra.exe --selftest` once
// the case is registered there.
namespace opra::orbit {
namespace {

using selftest::check;
using selftest::check_close;

constexpr double kPi = 3.14159265358979323846;
constexpr double kMuNereid = 2.07e19;
constexpr double kMuTessera = 1.9e13;
constexpr double kTesseraA = 1.720e10;

/** A two-body system with an L1, an L4 and a rotated L1 seat, built by hand: no asset, no SDL. */
SystemDef lagrange_system() {
    SystemDef system;
    system.name = "lagrange-test";

    Body star;
    star.id = "nereid";
    star.name = "Nereid";
    star.kind = BodyClass::Star;
    star.mu = kMuNereid;
    star.radius = 3.4e8;
    system.bodies.push_back(star);

    Body tessera;
    tessera.id = "tessera";
    tessera.name = "Tessera";
    tessera.parent = 0;
    tessera.mu = kMuTessera;
    tessera.radius = 6.3e6;
    tessera.elements.a = kTesseraA;
    tessera.elements.e = 0.034;
    tessera.elements.omega = 1.97;
    tessera.elements.M0 = 1.52;
    tessera.elements.mu = kMuNereid;
    system.bodies.push_back(tessera);

    Body l1;
    l1.id = "tessera_l1";
    l1.name = "Tessera L1";
    l1.parent = 1;
    l1.kind = BodyClass::Station;
    l1.lagrange = {true, 1, 1, 0.0};
    system.bodies.push_back(l1);

    Body l4;
    l4.id = "tessera_l4";
    l4.name = "Tessera L4";
    l4.parent = 1;
    l4.kind = BodyClass::Station;
    l4.lagrange = {true, 1, 4, 0.0};
    system.bodies.push_back(l4);

    Body drifted;
    drifted.id = "tessera_park";
    drifted.name = "Tessera Park";
    drifted.parent = 1;
    drifted.kind = BodyClass::Station;
    drifted.lagrange = {true, 1, 1, 0.25};
    system.bodies.push_back(drifted);
    return system;
}

double wrap_pi(double angle) { return angle - 2.0 * kPi * std::floor((angle + kPi) / (2.0 * kPi)); }

void test_mass_ratio() {
    check_close(mass_ratio(3.0, 1.0), 0.25, 1e-15, "lagrange: mu is the secondary's share");
    check_close(mass_ratio(kMuNereid, kMuTessera), kMuTessera / (kMuNereid + kMuTessera), 0.0,
                "lagrange: a GM pair gives the mass ratio");
    check_close(mass_ratio(1.0, 0.0), 0.0, 0.0, "lagrange: a lone primary has no secondary");
    check_close(mass_ratio(0.0, 0.0), 0.0, 0.0, "lagrange: no masses give no ratio, not NaN");
}

/**
 * Tessera around Nereid: mu = 1.9e13 / (2.07e19 + 1.9e13) = 9.17873553642632e-7 and the plan's
 * seed factor (mu/3)^(1/3) = 0.0067383546895293. At a = 1.720e10 m the seeds are 1.7084100299e10
 * (L1) and 1.7315899701e10 (L2); the exact collinear condition puts the roots 260.9 km outward of
 * each, which is the shift the three Newton steps have to find.
 */
void test_tessera_points() {
    const double mu = kMuTessera / (kMuNereid + kMuTessera);
    const double l1 = collinear_point(mu, kTesseraA, 1);
    const double l2 = collinear_point(mu, kTesseraA, 2);
    const double l3 = collinear_point(mu, kTesseraA, 3);
    check_close(l1, 1.7084361183e10, 1.0e3, "lagrange: Tessera L1 radius");
    check_close(l2, 1.7316159462e10, 1.0e3, "lagrange: Tessera L2 radius");
    check(l1 < kTesseraA && l2 > kTesseraA, "lagrange: L1 sits inside, L2 beyond the secondary");
    check_close(l3, -1.7200006578e10, 1.0, "lagrange: L3 is the plan's closed form, opposite");

    // The root is where the balance closes, not where the seed stopped: the residual is the
    // dimensionless collinear condition, and the seed alone misses it by 260 km.
    const double x1 = l1 / kTesseraA;
    const double x2 = l2 / kTesseraA;
    check(std::fabs((1.0 - mu) / (x1 * x1) - mu / ((1.0 - x1) * (1.0 - x1)) - (x1 - mu)) < 1e-12,
          "lagrange: three Newton steps land on the L1 condition");
    check(std::fabs((1.0 - mu) / (x2 * x2) + mu / ((x2 - 1.0) * (x2 - 1.0)) - (x2 - mu)) < 1e-12,
          "lagrange: three Newton steps land on the L2 condition");

    // L4 leads the secondary by sixty degrees at a full separation; L5 trails it.
    const double root3_over_2 = 0.8660254037844386;
    const glm::dvec2 l4 = lagrange_offset(mu, kTesseraA, 4, 0.0);
    const glm::dvec2 l5 = lagrange_offset(mu, kTesseraA, 5, 0.0);
    check_close(glm::length(l4), kTesseraA, 1e-3, "lagrange: L4 is a full separation out");
    check_close(l4.x, 0.5 * kTesseraA, 1e-3, "lagrange: L4 leads the secondary");
    check_close(l4.y, root3_over_2 * kTesseraA, 1e-3, "lagrange: L4 is sixty degrees ahead");
    check_close(l5.y, -root3_over_2 * kTesseraA, 1e-3, "lagrange: L5 is sixty degrees behind");

    // `dtheta` rotates the placement about the primary: a quarter turn puts the collinear point on
    // the +y axis at its own radius.
    const glm::dvec2 turned = lagrange_offset(mu, kTesseraA, 1, kPi / 2.0);
    check_close(turned.x, 0.0, 1e-3, "lagrange: dtheta turns the collinear point");
    check_close(turned.y, l1, 1e-3, "lagrange: dtheta keeps the radius");

    check_close(collinear_point(mu, kTesseraA, 0), 0.0, 0.0, "lagrange: point 0 is no point");
    check_close(collinear_point(mu, kTesseraA, 6), 0.0, 0.0, "lagrange: point 6 is no point");
    check_close(collinear_point(mu, 0.0, 1), 0.0, 0.0, "lagrange: a degenerate pair gives 0");
    check_close(glm::length(lagrange_offset(mu, kTesseraA, 9, 0.0)), 0.0, 0.0,
                "lagrange: an unknown point has no offset");
}

/**
 * The Earth-Moon pair, where the seed is 0.9% off and the Newton steps have real work: the exact
 * roots are L1 = 0.849 065 711 4 a and L2 = 1.167 832 751 0 a from the primary (both match the
 * standard published distances, 326 400 km and 448 900 km against a = 384 400 km).
 */
void test_earth_moon_refinement() {
    const double mu = 0.0121505856;
    const double seed1 = 1.0 - std::cbrt(mu / 3.0);
    const double seed2 = 1.0 + std::cbrt(mu / 3.0);
    const double l1 = collinear_point(mu, 1.0, 1);
    const double l2 = collinear_point(mu, 1.0, 2);
    check_close(l1, 0.8490657114197124, 1e-9, "lagrange: Earth-Moon L1");
    check_close(l2, 1.1678327510078692, 1e-9, "lagrange: Earth-Moon L2");
    check(std::fabs(l1 - seed1) > 1e-3 && std::fabs(l2 - seed2) > 1e-3,
          "lagrange: the refinement is doing the work, not the seed");
    check(std::fabs((1.0 - mu) / (l1 * l1) - mu / ((1.0 - l1) * (1.0 - l1)) - (l1 - mu)) < 1e-8,
          "lagrange: the L1 residual closes at Earth-Moon mu");
    check(std::fabs((1.0 - mu) / (l2 * l2) + mu / ((l2 - 1.0) * (l2 - 1.0)) - (l2 - mu)) < 1e-8,
          "lagrange: the L2 residual closes at Earth-Moon mu");
}

/**
 * A body with a Lagrange seat is placed by the cheat, not by its own elements: it holds the
 * secondary's angular position plus its own offset while its radius from the primary stays put,
 * for a full Tessera period. The frame's origin is the star, so the state's length is that radius.
 */
void test_lagrange_placement() {
    const SystemDef system = lagrange_system();
    const double mu = mass_ratio(kMuNereid, kMuTessera);
    const double r_l1 = collinear_point(mu, kTesseraA, 1);
    const double period = orbit::period(system.bodies[1].elements);
    check(period > 3.0e6 && period < 3.2e6, "lagrange: the test planet has a real period");

    std::vector<BodyState> states;
    for (int k = 0; k <= 8; ++k) {
        const double t = period * static_cast<double>(k) / 8.0;
        propagate(system, t, states);
        const double secondary = std::atan2(states[1].position.y, states[1].position.x);
        const glm::dvec2 l1 = states[2].position;
        const glm::dvec2 l4 = states[3].position;
        const glm::dvec2 park = states[4].position;

        check_close(wrap_pi(std::atan2(l1.y, l1.x) - secondary), 0.0, 1e-12,
                    "lagrange: L1 holds the secondary's angle for a full period");
        check_close(glm::length(l1), r_l1, 1.0,
                    "lagrange: L1 holds its own radius for a full period");
        check_close(wrap_pi(std::atan2(l4.y, l4.x) - secondary), kPi / 3.0, 1e-12,
                    "lagrange: L4 leads the secondary by sixty degrees");
        check_close(glm::length(l4), kTesseraA, 1.0e-6,
                    "lagrange: L4 sits at the secondary's own separation");
        check_close(wrap_pi(std::atan2(park.y, park.x) - secondary), 0.25, 1e-12,
                    "lagrange: dtheta rides the placement");

        // The placed body is co-rotating, so its velocity is the rigid rotation of its radius, not
        // the velocity its own elements would give: perpendicular to the arm, and nonzero.
        // Relative, not absolute: the dot of two vectors this size is ~1e15, so anything below a
        // part in 1e12 is the arithmetic rather than the placement.
        const double alignment = glm::dot(l1, states[2].velocity) /
                                 (glm::length(l1) * glm::length(states[2].velocity));
        check(std::fabs(alignment) < 1.0e-12,
              "lagrange: the L1 placement's velocity is tangential");
        check(glm::length(states[2].velocity) > 1.0,
              "lagrange: the L1 placement is moving with the frame");
    }
}

}  // namespace

int lagrange_tests() {
    const int before = selftest::failures();
    test_mass_ratio();
    test_tessera_points();
    test_earth_moon_refinement();
    test_lagrange_placement();
    return selftest::failures() - before;
}

}  // namespace opra::orbit
