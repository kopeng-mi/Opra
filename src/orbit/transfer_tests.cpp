// Assert suite for the transfer layer: Lambert closes on both endpoints and reproduces a Hohmann,
// and a maneuver node does exactly what the orbital mechanics say it does.
#include "orbit/transfer_tests.h"

#include <cmath>
#include <vector>

#include <glm/glm.hpp>

#include "orbit/kepler.h"
#include "orbit/transfer.h"

// As in orbit/tests.cpp: the assert harness belongs to the game, and orbit/ includes nothing
// outside <cmath>, <vector> and glm, so its two entry points are declared here.
namespace opra::selftest {
void check(bool ok, const char *what);
void check_close(double actual, double expected, double tolerance, const char *what);
}  // namespace opra::selftest

namespace opra::orbit {
namespace {

int g_failures = 0;

void check(bool ok, const char *what) {
    opra::selftest::check(ok, what);
    if (!ok) ++g_failures;
}

void check_close(double actual, double expected, double tolerance, const char *what) {
    opra::selftest::check_close(actual, expected, tolerance, what);
    if (!(std::fabs(actual - expected) <= tolerance)) ++g_failures;
}

const double kMu = 2.07e19;  // Nereid
const double kPi = 3.14159265358979323846;

/** Energy per unit mass: the quantity a burn is supposed to change, and nothing else should. */
double specific_energy(const State &state, double mu) {
    return 0.5 * glm::dot(state.v, state.v) - mu / glm::length(state.r);
}

double angular_momentum(const State &state) {
    return state.r.x * state.v.y - state.r.y * state.v.x;
}

void test_lambert_matches_hohmann() {
    const double r1 = 1.0e10;
    const double r2 = 1.8e10;
    const Hohmann ideal = hohmann(r1, r2, kMu);

    // Not quite 180 degrees: exactly 180 is the universal-variable singularity, which the solver
    // refuses (see the case below) and Hohmann owns. Two degrees short, the transfer is essentially
    // the Hohmann ellipse, so both ends must land on its numbers.
    const double theta = kPi - 0.035;
    const LambertSolution solution =
        lambert({r1, 0.0}, {r2 * std::cos(theta), r2 * std::sin(theta)}, ideal.transfer_time, kMu);
    check(solution.ok, "lambert: a near-Hohmann transfer solves");
    check(solution.iterations <= 60, "lambert: bisection stays inside its budget");
    if (!solution.ok) return;

    // The departure speed is the circular speed plus the Hohmann dv1. The arrival speed is the
    // transfer ellipse's own speed at r2 (vis-viva), not the circularised one: the Lambert v2 is
    // the velocity on the transfer conic, and the arrival burn is a separate number.
    const double expected_v1 = std::sqrt(kMu / r1) + ideal.dv1;
    const double expected_v2 = std::sqrt(kMu * (2.0 / r2 - 1.0 / ideal.a_transfer));
    const double v1 = glm::length(solution.v1);
    const double v2 = glm::length(solution.v2);
    check_close(v1, expected_v1, expected_v1 * 0.005,
                "lambert: the departure speed matches Hohmann's");
    check_close(v2, expected_v2, expected_v2 * 0.005,
                "lambert: the arrival speed matches the transfer ellipse's");

    // Exactly 180 degrees: refused, not fudged.
    const LambertSolution opposite =
        lambert({r1, 0.0}, {-r2, 0.0}, ideal.transfer_time, kMu);
    check(!opposite.ok, "lambert: an exactly opposite transfer is refused, not faked");
}

void test_lambert_closure() {
    const double radius = 1.2e10;
    const glm::dvec2 r1(radius, 0.0);
    const glm::dvec2 r2(0.0, radius);  // a quarter turn, direct
    const double dt = 0.35 * 2.0 * 3.14159265358979323846 * std::sqrt(radius * radius * radius / kMu);

    const LambertSolution solution = lambert(r1, r2, dt, kMu);
    check(solution.ok, "lambert: a quarter-turn transfer solves");
    if (!solution.ok) return;

    // Closure: fly the solved conic and it must arrive at r2 at exactly dt. This is the assertion
    // that catches a sign error in the chord geometry, which a Hohmann case cannot see.
    const Elements elements = from_state(r1, solution.v1, kMu, 0.0);
    const glm::dvec2 arrived = position_at(elements, dt);
    check_close(arrived.x, r2.x, radius * 1e-6, "lambert: the transfer arrives at r2 in x");
    check_close(arrived.y, r2.y, radius * 1e-6, "lambert: the transfer arrives at r2 in y");
    const glm::dvec2 arrived_velocity = velocity_at(elements, dt);
    check_close(arrived_velocity.x, solution.v2.x, 1e-3, "lambert: v2 matches the propagated conic");
    check_close(arrived_velocity.y, solution.v2.y, 1e-3, "lambert: v2 matches the propagated conic");
}

void test_lambert_rejects_nonsense() {
    const LambertSolution collinear = lambert({1.0e10, 0.0}, {2.0e10, 0.0}, 1.0e6, kMu);
    check(!collinear.ok, "lambert: collinear endpoints have no transfer plane");
    const LambertSolution zero_time = lambert({1.0e10, 0.0}, {0.0, 1.0e10}, 0.0, kMu);
    check(!zero_time.ok, "lambert: a zero time of flight is refused");
}

void test_node_prograde_raises_apoapsis() {
    // A circular orbit, then a prograde burn: the periapsis stays where it was and the apoapsis
    // rises, which is the whole definition of a departure burn.
    const double radius = 1.0e10;
    const double speed = std::sqrt(kMu / radius);
    const Elements circular = from_state({radius, 0.0}, {0.0, speed}, kMu, 0.0);
    check_close(periapsis(circular), radius, radius * 1e-9, "node: the circular orbit starts round");

    const Node burn{0.0, 4000.0, 0.0};  // 4000 m/s on a 45 km/s orbit: e rises to ~0.18
    const Elements raised = apply_node(circular, burn);
    check_close(periapsis(raised), radius, radius * 1e-6,
                "node: a prograde burn at periapsis leaves periapsis alone");
    check(apoapsis(raised) > radius * 1.05, "node: a prograde burn raises the apoapsis");
    check(raised.e > circular.e, "node: the orbit is no longer circular");
}

void test_node_radial_preserves_angular_momentum() {
    // A radial burn is along the radius, so it cannot change r x v: the angular momentum is the
    // invariant that proves the two components were separated correctly.
    const double radius = 1.0e10;
    const double speed = std::sqrt(kMu / radius);
    const State start{{radius, 0.0}, {0.0, speed}};
    const Elements circular = from_state(start.r, start.v, kMu, 0.0);
    const double before = angular_momentum(start);

    const Elements burned = apply_node(circular, Node{0.0, 0.0, 250.0});
    const State after = state_at(burned, 0.0);
    check_close(angular_momentum(after), before, std::fabs(before) * 1e-9,
                "node: a radial burn does not change the angular momentum");
    check(specific_energy(after, kMu) > specific_energy(start, kMu),
          "node: adding speed adds energy");
}

void test_node_sequence_and_prediction() {
    const double radius = 1.0e10;
    const double speed = std::sqrt(kMu / radius);
    const Elements circular = from_state({radius, 0.0}, {0.0, speed}, kMu, 0.0);
    const double period = 2.0 * 3.14159265358979323846 * std::sqrt(radius * radius * radius / kMu);

    const std::vector<Node> plan{{0.0, 300.0, 0.0}, {period * 0.25, 120.0, -40.0}};
    const Elements after_first = apply_node(circular, plan[0]);
    const Elements after_both = apply_nodes(circular, plan);
    const Elements manual = apply_node(after_first, plan[1]);
    check_close(after_both.a, manual.a, std::fabs(manual.a) * 1e-12,
                "node: a list applies in order");
    check_close(after_both.e, manual.e, 1e-12, "node: a list applies in order (e)");

    // The prediction is the same conic the ship would fly: after the last node has passed, the
    // state must equal the propagated elements.
    const double t = period * 0.5;
    const State predicted = state_after(circular, plan, t);
    const State flown = state_at(after_both, t);
    check_close(predicted.r.x, flown.r.x, std::fabs(flown.r.x) * 1e-12,
                "node: the prediction is the flown conic");
    check_close(predicted.v.y, flown.v.y, std::fabs(flown.v.y) * 1e-12,
                "node: the prediction is the flown conic (v)");

    // A node in the future is not applied yet.
    const std::vector<Node> later{{period * 2.0, 500.0, 0.0}};
    const State now = state_after(circular, later, period * 0.1);
    const State unburned = state_at(circular, period * 0.1);
    check_close(now.r.x, unburned.r.x, std::fabs(unburned.r.x) * 1e-12,
                "node: a node after the prediction time is not applied");
}

}  // namespace

int transfer_tests() {
    g_failures = 0;
    test_lambert_matches_hohmann();
    test_lambert_closure();
    test_lambert_rejects_nonsense();
    test_node_prograde_raises_apoapsis();
    test_node_radial_preserves_angular_momentum();
    test_node_sequence_and_prediction();
    return g_failures;
}

}  // namespace opra::orbit
