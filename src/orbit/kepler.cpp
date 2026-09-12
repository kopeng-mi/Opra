// Kepler propagation. The only numerically delicate part is the anomaly solver at high
// eccentricity, so that is where the care is.
#include "orbit/kepler.h"

#include <cmath>

namespace opra::orbit {
namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr double kTwoPi = 2.0 * kPi;

/** Newton's residual target: 1e-13 rad is a metre at 10 Gm, far below anything drawn. */
constexpr double kAnomalyTolerance = 1e-13;

/**
 * The plan budgets five Newton steps from Danby's starter, which does converge for e < 0.9. At
 * e = 0.99 it does not: the first step overshoots the root because 1 - e cos E is only ~1e-2 at
 * periapsis, and the 1e-13 residual arrives around the eighth step. The cap is a guard against a
 * wild input, not a convergence budget — four iterations is the common case.
 */
constexpr int kNewtonIterations = 20;

/** Inside this band the branches meet and the Newton derivative vanishes: see solve_kepler. */
constexpr double kParabolicBand = 1e-4;

/** Below this the ellipse is a circle and the argument of periapsis has no meaning. */
constexpr double kCircularEccentricity = 1e-8;

/** Mean anomaly into [-pi, pi], so high warp counts do not lose precision to a huge M. */
double wrap_pi(double angle) {
    return angle - kTwoPi * std::floor((angle + kPi) / kTwoPi);
}

glm::dvec2 rotate(const glm::dvec2 &v, double angle) {
    const double c = std::cos(angle);
    const double s = std::sin(angle);
    return glm::dvec2(c * v.x - s * v.y, s * v.x + c * v.y);
}

/** Everything a conic yields at a time that both the position and the velocity need. */
struct Anomaly {
    double nu;      // true anomaly, radians
    double r;       // radius, metres
    double rdot;    // radial speed, m/s
    double rnudot;  // transverse speed, m/s
};

Anomaly anomaly_at(const Elements &elements, double t) {
    const double n = mean_motion(elements);
    Anomaly out{};
    if (is_elliptic(elements)) {
        const double M = wrap_pi(elements.M0 + n * (t - elements.t0));
        const double E = solve_kepler(M, elements.e);
        const double half = 0.5 * E;
        out.nu = 2.0 * std::atan2(std::sqrt(1.0 + elements.e) * std::sin(half),
                                  std::sqrt(1.0 - elements.e) * std::cos(half));
        out.r = elements.a * (1.0 - elements.e * std::cos(E));
        out.rdot = std::sqrt(elements.mu * elements.a) / out.r * elements.e * std::sin(E);
        out.rnudot =
            std::sqrt(elements.mu * elements.a * (1.0 - elements.e * elements.e)) / out.r;
        return out;
    }
    // Hyperbolic: M grows without bound, so it is deliberately not wrapped.
    const double M = elements.M0 + n * (t - elements.t0);
    const double H = solve_hyperbolic(M, elements.e);
    const double half = 0.5 * H;
    out.nu = 2.0 * std::atan2(std::sqrt(elements.e + 1.0) * std::sinh(half),
                              std::sqrt(elements.e - 1.0) * std::cosh(half));
    out.r = elements.a * (1.0 - elements.e * std::cosh(H));
    out.rdot = std::sqrt(elements.mu * -elements.a) / out.r * elements.e * std::sinh(H);
    out.rnudot =
        std::sqrt(elements.mu * -elements.a * (elements.e * elements.e - 1.0)) / out.r;
    return out;
}

}  // namespace

double mean_motion(const Elements &elements) {
    const double a = elements.a;
    return std::sqrt(elements.mu / std::fabs(a * a * a));
}

double solve_kepler(double M, double e) {
    // There is no elliptic conic at e >= 1, and none Newton can resolve within 1e-4 of it: the
    // derivative 1 - e cos E goes to zero at periapsis. Clamp to the nearest solvable ellipse —
    // a true parabola is a measure-zero case no player can hold, and this keeps the whole
    // propagation total instead of returning a NaN that would poison the ship's state.
    if (e > 1.0 - kParabolicBand) e = 1.0 - kParabolicBand;
    if (e <= 0.0) return M;  // circular: E and M coincide
    double E = M + e * std::sin(M);  // Danby's starter, good to e < 0.6 in three steps
    for (int i = 0; i < kNewtonIterations; ++i) {
        const double f = E - e * std::sin(E) - M;
        if (std::fabs(f) < kAnomalyTolerance) break;
        E -= f / (1.0 - e * std::cos(E));
    }
    return E;
}

double solve_hyperbolic(double M, double e) {
    if (e < 1.0 + kParabolicBand) e = 1.0 + kParabolicBand;  // no hyperbolic conic below e = 1
    double H = std::asinh(M / e);  // the branching form of the classic starter
    for (int i = 0; i < kNewtonIterations; ++i) {
        const double f = e * std::sinh(H) - H - M;
        if (std::fabs(f) < kAnomalyTolerance) break;
        H -= f / (e * std::cosh(H) - 1.0);  // the derivative is positive for every e > 1
    }
    return H;
}

glm::dvec2 position_at(const Elements &elements, double t) {
    const Anomaly s = anomaly_at(elements, t);
    return rotate(glm::dvec2(s.r * std::cos(s.nu), s.r * std::sin(s.nu)), elements.omega);
}

glm::dvec2 velocity_at(const Elements &elements, double t) {
    const Anomaly s = anomaly_at(elements, t);
    return rotate(glm::dvec2(s.rdot * std::cos(s.nu) - s.rnudot * std::sin(s.nu),
                             s.rdot * std::sin(s.nu) + s.rnudot * std::cos(s.nu)),
                  elements.omega);
}

State state_at(const Elements &elements, double t) {
    const Anomaly s = anomaly_at(elements, t);
    const double c = std::cos(elements.omega);
    const double sn = std::sin(elements.omega);
    State out{};
    out.r = glm::dvec2(c * s.r * std::cos(s.nu) - sn * s.r * std::sin(s.nu),
                       sn * s.r * std::cos(s.nu) + c * s.r * std::sin(s.nu));
    out.v = glm::dvec2(c * (s.rdot * std::cos(s.nu) - s.rnudot * std::sin(s.nu)) -
                           sn * (s.rdot * std::sin(s.nu) + s.rnudot * std::cos(s.nu)),
                       sn * (s.rdot * std::cos(s.nu) - s.rnudot * std::sin(s.nu)) +
                           c * (s.rdot * std::sin(s.nu) + s.rnudot * std::cos(s.nu)));
    return out;
}

Elements from_state(const glm::dvec2 &r, const glm::dvec2 &v, double mu, double t0) {
    const double radius = glm::length(r);
    const double speed2 = glm::dot(v, v);
    const double radial = glm::dot(r, v);

    Elements out{};
    out.mu = mu;
    out.t0 = t0;
    out.a = -mu / (speed2 - 2.0 * mu / radius);  // = -mu / (2 * specific energy)
    const glm::dvec2 e_vec =
        ((speed2 - mu / radius) * r - radial * v) / mu;
    out.e = glm::length(e_vec);

    double nu = 0.0;
    if (out.e < kCircularEccentricity) {
        out.omega = 0.0;                 // circular: the periapsis direction is arbitrary
        nu = std::atan2(r.y, r.x);       // so the polar angle is the whole anomaly
    } else {
        out.omega = std::atan2(e_vec.y, e_vec.x);
        // Angle from e to r, signed by the radial velocity as the plan's §3.2 specifies. For a
        // prograde orbit the two agree; a retrograde one lands on the prograde conic through the
        // same point, which is all rot(omega) can express (see the note in kepler.h).
        const double magnitude =
            std::atan2(std::fabs(e_vec.x * r.y - e_vec.y * r.x), glm::dot(e_vec, r));
        nu = radial < 0.0 ? -magnitude : magnitude;
    }

    if (out.e < 1.0) {
        const double half = 0.5 * nu;
        const double E = 2.0 * std::atan2(std::sqrt(1.0 - out.e) * std::sin(half),
                                          std::sqrt(1.0 + out.e) * std::cos(half));
        out.M0 = E - out.e * std::sin(E);
    } else {
        // tan(nu/2) = sqrt((e+1)/(e-1)) tanh(H/2), so the inverse runs through atanh — the
        // atan2 form that inverts the elliptic E would be wrong here (atan != atanh).
        const double half = 0.5 * nu;
        const double H =
            2.0 * std::atanh(std::sqrt((out.e - 1.0) / (out.e + 1.0)) * std::tan(half));
        out.M0 = out.e * std::sinh(H) - H;
    }
    return out;
}

bool is_elliptic(const Elements &elements) { return elements.e < 1.0; }

double period(const Elements &elements) {
    return is_elliptic(elements) ? kTwoPi / mean_motion(elements) : HUGE_VAL;
}

double periapsis(const Elements &elements) { return elements.a * (1.0 - elements.e); }

double apoapsis(const Elements &elements) {
    return is_elliptic(elements) ? elements.a * (1.0 + elements.e) : HUGE_VAL;
}

bool near_parabolic(double e) { return std::fabs(e - 1.0) < kParabolicBand; }

}  // namespace opra::orbit
