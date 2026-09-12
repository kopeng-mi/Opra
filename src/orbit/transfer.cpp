#include "orbit/transfer.h"

#include <cmath>

namespace opra::orbit {
namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr double kTwoPi = 2.0 * kPi;

double wrap_positive(double angle) {
    return angle - kTwoPi * std::floor(angle / kTwoPi);  // [0, 2 pi)
}

}  // namespace

Hohmann hohmann(double r1, double r2, double mu) {
    Hohmann out{};
    out.a_transfer = 0.5 * (r1 + r2);
    out.dv1 = std::sqrt(mu / r1) * (std::sqrt(2.0 * r2 / (r1 + r2)) - 1.0);
    out.dv2 = std::sqrt(mu / r2) * (1.0 - std::sqrt(2.0 * r1 / (r1 + r2)));
    out.dv_total = std::fabs(out.dv1) + std::fabs(out.dv2);
    out.transfer_time = kPi * std::sqrt(out.a_transfer * out.a_transfer * out.a_transfer / mu);
    return out;
}

double phase_angle_required(double n2, double transfer_time) {
    return kPi - n2 * transfer_time;
}

double time_to_window(double phase_now, double phase_required, double n1, double n2) {
    const double relative = n1 - n2;  // the rate the lead changes at, with a direction
    if (relative == 0.0) return HUGE_VAL;

    // The plan's expression, (phase_required - phase_now) / (n1 - n2), is this one written for
    // the depart-from-inside case only, where the lead closes and the numerator is the gap left
    // to run. Leaving from outside the target's orbit the lead opens instead, so the gap is
    // measured the other way; both then wrap into [0, synodic).
    const double gap =
        relative > 0.0 ? phase_now - phase_required : phase_required - phase_now;
    return wrap_positive(gap) / std::fabs(relative);
}

double synodic_period(double n1, double n2) {
    const double relative = std::fabs(n1 - n2);
    return relative == 0.0 ? HUGE_VAL : kTwoPi / relative;
}

namespace {

/** Stumpff C(z): the universal-variable form of (1 - cos E)/E^2, finite through z = 0. */
double stumpff_c(double z) {
    if (z > 1e-8) {
        const double root = std::sqrt(z);
        return (1.0 - std::cos(root)) / z;
    }
    if (z < -1e-8) {
        const double root = std::sqrt(-z);
        return (std::cosh(root) - 1.0) / -z;
    }
    return 0.5;
}

/** Stumpff S(z): the universal-variable form of (E - sin E)/E^3, finite through z = 0. */
double stumpff_s(double z) {
    if (z > 1e-8) {
        const double root = std::sqrt(z);
        return (root - std::sin(root)) / (z * root);
    }
    if (z < -1e-8) {
        const double root = std::sqrt(-z);
        return (std::sinh(root) - root) / (-z * root);
    }
    return 1.0 / 6.0;
}

/** Free-flight time for a given z; negative when z puts the conic below the chord. */
double free_flight(double z, double a_chord, double r1, double r2, double mu) {
    const double c = stumpff_c(z);
    const double s = stumpff_s(z);
    const double y = r1 + r2 + a_chord * (z * s - 1.0) / std::sqrt(c);
    if (y < 0.0) return -1.0;
    const double x = std::sqrt(y / c);
    return (x * x * x * s + a_chord * std::sqrt(y)) / std::sqrt(mu);
}

}  // namespace

LambertSolution lambert(const glm::dvec2 &r1, const glm::dvec2 &r2, double dt, double mu) {
    LambertSolution out;
    const double radius1 = glm::length(r1);
    const double radius2 = glm::length(r2);
    if (radius1 <= 0.0 || radius2 <= 0.0 || dt <= 0.0 || mu <= 0.0) return out;

    // The chord geometry: dtheta is measured the direct way round, which is the way every orbit in
    // this game runs, so the "short way" branch is the only one the planner needs.
    double dtheta = std::atan2(r1.x * r2.y - r1.y * r2.x, glm::dot(r1, r2));
    if (dtheta <= 0.0) dtheta += kTwoPi;
    // Endpoints on the same ray have no transfer plane and divide by zero below.
    const double chord = 1.0 - std::cos(dtheta);
    if (chord < 1e-12) return out;
    // Exactly 180 degrees is the other end of the same degeneracy: the chord term A vanishes, and
    // with it the numerator of the departure velocity, so the division below would return zeros.
    // That case is a Hohmann transfer with its own closed form, so this solver refuses it and the
    // caller routes it to hohmann() rather than flying a fabricated conic.
    if (1.0 + std::cos(dtheta) < 1e-9) return out;
    const double a_chord = std::sin(dtheta) * std::sqrt(radius1 * radius2 / chord);

    // Bisection on z. The free-flight time is monotonic in z, so the bracket only has to contain
    // the answer: start at the whole physical range and widen if the far end is short.
    double low = -4.0 * kPi * kPi;
    double high = 4.0 * kPi * kPi;
    while (free_flight(high, a_chord, radius1, radius2, mu) < dt && high < 1.0e6) high *= 2.0;
    while (free_flight(low, a_chord, radius1, radius2, mu) > dt && low > -1.0e6) low *= 2.0;

    double z = 0.5 * (low + high);
    for (int i = 0; i < 60; ++i) {
        z = 0.5 * (low + high);
        const double time = free_flight(z, a_chord, radius1, radius2, mu);
        ++out.iterations;
        if (time < 0.0) {
            low = z;  // this branch is below the chord: it cannot be the transfer
            continue;
        }
        if (std::fabs(time - dt) <= 1e-9 * std::max(1.0, dt)) break;
        if (time < dt) {
            low = z;
        } else {
            high = z;
        }
    }

    const double c = stumpff_c(z);
    const double s = stumpff_s(z);
    const double y = radius1 + radius2 + a_chord * (z * s - 1.0) / std::sqrt(c);
    if (!(y > 0.0)) return out;
    const double f = 1.0 - y / radius1;
    const double g = a_chord * std::sqrt(y / mu);
    if (std::fabs(g) < 1e-12) return out;
    const double gdot = 1.0 - y / radius2;

    out.v1 = (r2 - f * r1) / g;
    out.v2 = (gdot * r2 - r1) / g;
    out.ok = std::isfinite(out.v1.x) && std::isfinite(out.v2.x) && std::isfinite(out.v2.y);
    return out;
}

Elements apply_node(const Elements &before, const Node &node) {
    if (before.mu <= 0.0) return before;
    const State at = state_at(before, node.t);
    const double speed = glm::length(at.v);
    if (speed <= 0.0) return before;
    const glm::dvec2 prograde = at.v / speed;
    const glm::dvec2 radial = glm::dvec2(prograde.y, -prograde.x);  // direct orbit: radial is -90
    const glm::dvec2 outward =
        glm::dot(radial, at.r) >= 0.0 ? radial : glm::dvec2(-radial.x, -radial.y);
    const glm::dvec2 velocity = at.v + prograde * node.prograde + outward * node.radial;
    return from_state(at.r, velocity, before.mu, node.t);
}

Elements apply_nodes(const Elements &start, const std::vector<Node> &nodes) {
    Elements current = start;
    for (const Node &node : nodes) current = apply_node(current, node);
    return current;
}

State state_after(const Elements &start, const std::vector<Node> &nodes, double t) {
    // The caller keeps the list sorted by time: the planner re-sorts it whenever a node is dragged,
    // so a burn that happens after `t` is not applied and the ones before it are, in order.
    Elements current = start;
    for (const Node &node : nodes) {
        if (node.t > t) break;
        current = apply_node(current, node);
    }
    return state_at(current, t);
}

}  // namespace opra::orbit
