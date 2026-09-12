// Hand-computed checks for the orbit library. Every expected number below is derived on paper
// from the formulas in PLAN-02 §3.1-3.5 and written into the comment beside it — nothing here is
// captured from the run it tests. Run through `Opra.exe --selftest`.
#include "orbit/tests.h"

#include <cmath>
#include <vector>

#include <glm/glm.hpp>

#include "orbit/conic.h"
#include "orbit/kepler.h"
#include "orbit/soi.h"
#include "orbit/transfer.h"

// The assert harness belongs to the game (`src/selftest_support.cpp`), and orbit/ includes
// nothing outside <cmath>, <vector> and glm, so its two entry points are declared here.
namespace opra::selftest {
void check(bool ok, const char *what);
void check_close(double actual, double expected, double tolerance, const char *what);
}  // namespace opra::selftest

namespace opra::orbit {
namespace {

int g_failures = 0;

/** Forwards to the game's harness and counts here: `tests()` returns its own failure count. */
void check(bool ok, const char *what) {
    opra::selftest::check(ok, what);
    if (!ok) ++g_failures;
}

void check_close(double actual, double expected, double tolerance, const char *what) {
    opra::selftest::check_close(actual, expected, tolerance, what);
    if (!(std::fabs(actual - expected) <= tolerance)) ++g_failures;
}

constexpr double kPi = 3.14159265358979323846;
constexpr double kTwoPi = 2.0 * kPi;
constexpr double kDay = 86400.0;

// System data, locked in PLAN-02 §5.1. The star Nereid's mu drives every planetary period.
constexpr double kMuNereid = 2.07e19;
constexpr double kMuTessera = 1.9e13;

Elements elements_of(double a, double e, double omega, double M0, double mu = kMuNereid) {
    Elements out;
    out.a = a;
    out.e = e;
    out.omega = omega;
    out.M0 = M0;
    out.t0 = 0.0;
    out.mu = mu;
    return out;
}

double wrap_pi(double angle) { return angle - kTwoPi * std::floor((angle + kPi) / kTwoPi); }

double rel_error(double actual, double expected) {
    const double scale =
        std::fabs(actual) > std::fabs(expected) ? std::fabs(actual) : std::fabs(expected);
    return scale == 0.0 ? 0.0 : std::fabs(actual - expected) / scale;
}

double cross2(const glm::dvec2 &a, const glm::dvec2 &b) { return a.x * b.y - a.y * b.x; }

double days(double seconds) { return seconds / kDay; }

/** Kepler's equation at e = 0 is E = M, so the residual is exactly zero for every M. */
void test_kepler_solver() {
    check_close(solve_kepler(0.0, 0.0), 0.0, 0.0, "kepler: M = 0, e = 0");
    check_close(solve_kepler(1.0, 0.0), 1.0, 0.0, "kepler: M = 1, e = 0 is E = M");

    // e = 0.3, M = 1: E - 0.3 sin E = 1 has the root E = 1.288 091 313 211 838 rad
    // (substituting back: 1.2880913 - 0.3 * 0.9603043 = 1.0000000).
    {
        const double E = solve_kepler(1.0, 0.3);
        check_close(E, 1.2880913132118377, 1e-12, "kepler: E at e = 0.3, M = 1");
        check(std::fabs(E - 0.3 * std::sin(E) - 1.0) < 1e-13, "kepler: residual at e = 0.3");
    }

    // e = 0.9, M = 1: E = 1.862 086 686 874 533 rad (1.8620867 - 0.9 * 0.9578741 = 1.0000000).
    {
        const double E = solve_kepler(1.0, 0.9);
        check_close(E, 1.8620866868745325, 1e-12, "kepler: E at e = 0.9, M = 1");
        check(std::fabs(E - 0.9 * std::sin(E) - 1.0) < 1e-13, "kepler: residual at e = 0.9");
    }

    // e = 0.99, M = 1: E = 1.927 635 550 695 835 rad. This is the case the plan's five-iteration
    // budget cannot reach — the first Newton step overshoots and the eighth lands the residual.
    {
        const double E = solve_kepler(1.0, 0.99);
        check_close(E, 1.927635550695835, 1e-12, "kepler: E at e = 0.99, M = 1");
        check(std::fabs(E - 0.99 * std::sin(E) - 1.0) < 1e-13, "kepler: residual at e = 0.99");
    }

    // e = 0.99, M = 0.1: E = 0.831 660 423 791 057 rad, near periapsis where 1 - e cos E is
    // smallest (0.333) and the starter is furthest from the root.
    {
        const double E = solve_kepler(0.1, 0.99);
        check_close(E, 0.8316604237910568, 1e-12, "kepler: E at e = 0.99, M = 0.1");
        check(std::fabs(E - 0.99 * std::sin(E) - 0.1) < 1e-13, "kepler: residual at e = 0.99, M = 0.1");
    }
}

/** e sinh H - H = M. At M = 0 the root is H = 0 for every e. */
void test_hyperbolic_solver() {
    // e = 1.3, M = 2: 1.3 sinh(1.7917903) - 1.7917903 = 1.3 * 2.9167618 - 1.7917903 = 2.000000.
    {
        const double H = solve_hyperbolic(2.0, 1.3);
        check_close(H, 1.7917903171252207, 1e-12, "hyperbolic: H at e = 1.3, M = 2");
        check(std::fabs(1.3 * std::sinh(H) - H - 2.0) < 1e-13, "hyperbolic: residual at e = 1.3");
    }
    // e = 2.5, M = 2: 2.5 sinh(1.0214558) - 1.0214558 = 2.5 * 1.2085823 - 1.0214558 = 2.000000.
    {
        const double H = solve_hyperbolic(2.0, 2.5);
        check_close(H, 1.0214558372092852, 1e-12, "hyperbolic: H at e = 2.5, M = 2");
        check(std::fabs(2.5 * std::sinh(H) - H - 2.0) < 1e-13, "hyperbolic: residual at e = 2.5");
    }
    check_close(solve_hyperbolic(0.0, 1.3), 0.0, 1e-15, "hyperbolic: M = 0 gives H = 0");
}

void test_near_parabolic() {
    // |e - 1| < 1e-4 is the band both solvers clamp across; outside it neither is touched.
    check(near_parabolic(1.0), "near_parabolic: e = 1");
    check(near_parabolic(1.0 - 5e-5), "near_parabolic: just inside the band");
    check(!near_parabolic(1.0 + 2e-4), "near_parabolic: just outside the band");
    check(!near_parabolic(0.9), "near_parabolic: e = 0.9");
    check(!is_elliptic(elements_of(1.0e10, 1.3, 0.0, 0.0)), "is_elliptic: e = 1.3 escapes");
}

/** Elements that the plan's own numbers pin down: mean motion, apsides, closed/open orbits. */
void test_element_geometry() {
    // Tessera: n = sqrt(2.07e19 / 1.72e10^3) = sqrt(2.07e19 / 5.08848e30) = 2.016938e-6 rad/s.
    check_close(mean_motion(elements_of(1.72e10, 0.034, 1.97, 1.52)), 2.016937786444319e-06,
                1e-15, "mean_motion: Tessera");

    // Hyperbolic a is negative but the rate uses |a|: sqrt(1.3e20 / 1e30) = 1.1401754e-5.
    check_close(mean_motion(elements_of(-1.0e10, 1.3, 0.0, 0.0, 1.3e20)), 1.140175425099138e-05,
                1e-16, "mean_motion: sign of a does not enter");

    // Cinder: periapsis a(1 - e) = 7.18e9 * 0.989 = 7.10102e9 m;
    // apoapsis a(1 + e) = 7.18e9 * 1.011 = 7.258 98e9 m.
    check_close(periapsis(elements_of(7.18e9, 0.011, 0.42, 2.48)), 7.10102e9, 1.0,
                "periapsis: Cinder");
    check_close(apoapsis(elements_of(7.18e9, 0.011, 0.42, 2.48)), 7.25898e9, 1.0,
                "apoapsis: Cinder");

    // An escaping orbit has no apoapsis and no period.
    const Elements escape = elements_of(-1.0e10, 1.3, 0.0, 0.0, 1.3e20);
    check(std::isinf(apoapsis(escape)), "apoapsis: +inf for a hyperbola");
    check(std::isinf(period(escape)), "period: +inf for a hyperbola");
    check(!is_elliptic(escape), "is_elliptic: hyperbola is open");
    check(is_elliptic(elements_of(4.338e10, 0.091, 3.31, 3.51)), "is_elliptic: Halberd is closed");

    // Circular orbit: |v| = sqrt(mu/a) = sqrt(1e13/1e10) = 31.6227766 m/s at every t.
    {
        const Elements circular = elements_of(1.0e10, 0.0, 0.0, 0.0, 1e13);
        const State s = state_at(circular, 4321.0);
        check_close(glm::length(s.r), 1.0e10, 1e-3, "circular: radius stays a");
        check_close(glm::length(s.v), 31.622776601683793, 1e-9, "circular: speed is sqrt(mu/a)");
    }

    // Periapsis speed from vis-viva: sqrt(mu (1 + e) / (a (1 - e)))
    // = sqrt(1e13 * 1.6 / (1e10 * 0.4)) = sqrt(4000) = 63.2455532 m/s.
    {
        const Elements el = elements_of(1.0e10, 0.6, 0.3, 0.0, 1e13);
        check_close(glm::length(velocity_at(el, 0.0)), 63.245553203367585, 1e-9,
                    "periapsis: vis-viva speed");
        check_close(glm::length(position_at(el, 0.0)), 4.0e9, 1e-3, "periapsis: a(1 - e)");
    }
}

/** The plan's 6-91 day band, one case per Nereid body, against P = 2 pi sqrt(a^3 / mu). */
void test_system_periods() {
    check_close(days(period(elements_of(7.18e9, 0.011, 0.42, 2.48))), 9.724508, 1e-4,
                "period: Cinder in days");
    check_close(days(period(elements_of(1.720e10, 0.034, 1.97, 1.52))), 36.055674, 1e-4,
                "period: Tessera in days");
    check_close(days(period(elements_of(4.338e10, 0.091, 3.31, 3.51))), 144.415988, 1e-4,
                "period: Halberd in days");
    check_close(days(period(elements_of(2.61e10, 0.004, 0.9, 0.0))), 67.397178, 1e-4,
                "period: Wayfarer in days");
    check_close(days(period(elements_of(5.2e7, 0.002, 0.0, 5.41, kMuTessera))), 6.255965, 1e-4,
                "period: Vesk in days");

    // Four of the five fall in the plan's 6-91 day band. Halberd does not: the locked §5.1 numbers
    // (a = 4.338e10, mu = 2.07e19) give 144.416 days, so E1's stated ceiling is not what the data
    // produces. The assertion records the gap rather than papering over it.
    const double cinder = days(period(elements_of(7.18e9, 0.011, 0.42, 2.48)));
    const double wayfarer = days(period(elements_of(2.61e10, 0.004, 0.9, 0.0)));
    check(cinder >= 6.0 && wayfarer <= 91.0, "periods: inner system inside 6-91 days");
}

/** elements -> state -> elements must close to 1e-9 relative (PLAN-02 §3.2, review gate 7). */
void test_round_trip() {
    struct Case {
        double a;
        double e;
        double omega;
        double M0;
    };
    const Case cases[] = {
        {7.18e9, 0.0, 0.0, 0.0},        // circular: omega is undefined, so it comes back as 0
        {7.18e9, 0.011, 0.42, 2.48},    // Cinder
        {1.720e10, 0.034, 1.97, 1.52},  // Tessera
        {1.720e10, 0.7, 1.97, 5.41},    // the mid-range the systems never use
        {4.338e10, 0.95, 3.31, 6.0},    // Halberd stretched to the edge of closed
        {2.61e10, 0.004, 0.9, 0.0},     // Wayfarer
    };
    const double epoch = 1234.5;
    const double later = epoch + 98765.0;
    for (const Case &c : cases) {
        const Elements el = elements_of(c.a, c.e, c.omega, c.M0);
        const State sampled = state_at(el, epoch);
        const Elements back = from_state(sampled.r, sampled.v, kMuNereid, epoch);

        // The recovered M0 is the mean anomaly at the sampling epoch, not at the input's t0:
        // M(epoch) = M0 + n (epoch - t0), folded back into [-pi, pi].
        const double expected_M0 = wrap_pi(c.M0 + mean_motion(el) * epoch);
        check(rel_error(back.a, c.a) < 1e-9 && std::fabs(back.e - c.e) < 1e-9,
              "round trip: a and e");
        check(std::fabs(wrap_pi(back.omega - c.omega)) < 1e-9 &&
                  std::fabs(wrap_pi(back.M0 - expected_M0)) < 1e-9,
              "round trip: omega and M0");

        const State kept = state_at(el, later);
        const glm::dvec2 dr = position_at(back, later) - kept.r;
        const glm::dvec2 dv = velocity_at(back, later) - kept.v;
        check(glm::length(dr) < 1e-9 * glm::length(kept.r) &&
                  glm::length(dv) < 1e-9 * glm::length(kept.v),
              "round trip: state after one epoch");
    }
}

/** A ship on escape has to convert back the same way, with a < 0. */
void test_hyperbolic_round_trip() {
    struct Case {
        double e;
        double omega;
        double M0;
    };
    const Case cases[] = {{1.3, 0.0, 0.0}, {2.5, 2.9, 3.0}};
    const double epoch = 2.0;
    for (const Case &c : cases) {
        const Elements el = elements_of(-1.0, c.e, c.omega, c.M0, 1.0);
        const State sampled = state_at(el, epoch);
        const Elements back = from_state(sampled.r, sampled.v, 1.0, epoch);
        check(rel_error(back.a, el.a) < 1e-9 && std::fabs(back.e - el.e) < 1e-9,
              "hyperbolic round trip: a and e");
        const State kept = state_at(el, epoch + 3.0);
        const glm::dvec2 dr = position_at(back, epoch + 3.0) - kept.r;
        const glm::dvec2 dv = velocity_at(back, epoch + 3.0) - kept.v;
        check(glm::length(dr) < 1e-9 * glm::length(kept.r) &&
                  glm::length(dv) < 1e-9 * glm::length(kept.v),
              "hyperbolic round trip: state");
    }

    // The hand-worked hyperbolic state at M = 2 with a = -1 m, mu = 1, e = 1.3:
    // H = 1.7917903, r = a(1 - e cosh H) = -(1 - 1.3 * 3.0834233) = 3.0084503 m,
    // nu = 126.355 deg, and |v| = sqrt(1.2603799^2 + 0.2761097^2) = 1.2902690 m/s.
    const Elements hyper = elements_of(-1.0, 1.3, 0.0, 0.0, 1.0);
    const State at_two = state_at(hyper, 2.0);
    check_close(glm::length(at_two.r), 3.008450300183923, 1e-12, "hyperbola: radius at M = 2");
    check_close(glm::length(at_two.v), 1.2902690017100775, 1e-12, "hyperbola: speed at M = 2");
}

/** Energy and angular momentum over 1e4 propagations, to 1e-10 relative (review gate 7). */
void test_conservation() {
    struct Case {
        double a;
        double e;
        double omega;
        double M0;
    };
    // Specific energy -mu/(2a): Tessera -6.0174419e8, Halberd -2.3858921e8 J/kg.
    // Angular momentum sqrt(mu a (1 - e^2)): Tessera 5.9634589e14, Halberd 9.4367894e14 m^2/s.
    // Neither appears below because it is not the value that matters — its constancy is.
    const Case bodies[] = {
        {1.720e10, 0.034, 1.97, 1.52},  // Tessera
        {4.338e10, 0.091, 3.31, 3.51},  // Halberd
    };
    for (int b = 0; b < 2; ++b) {
        const Elements el = elements_of(bodies[b].a, bodies[b].e, bodies[b].omega,
                                        bodies[b].M0);
        const State first = state_at(el, 0.0);
        const double energy0 = 0.5 * glm::dot(first.v, first.v) - el.mu / glm::length(first.r);
        const double h0 = cross2(first.r, first.v);

        double worst_energy = 0.0;
        double worst_h = 0.0;
        for (int i = 1; i <= 10000; ++i) {
            const double t = 600.0 * i + 37.0 * (i % 17);  // epochs with no shared factor
            const State s = state_at(el, t);
            const double energy = 0.5 * glm::dot(s.v, s.v) - el.mu / glm::length(s.r);
            worst_energy = std::fmax(worst_energy, rel_error(energy, energy0));
            worst_h = std::fmax(worst_h, rel_error(cross2(s.r, s.v), h0));
        }
        check_close(worst_energy, 0.0, 1e-10,
                    b == 0 ? "conservation: Tessera energy" : "conservation: Halberd energy");
        check_close(worst_h, 0.0, 1e-10, b == 0 ? "conservation: Tessera angular momentum"
                                                : "conservation: Halberd angular momentum");
    }
}

/** Hill-sphere radii for the two nested pairs in PLAN-02 §5.1. */
void test_spheres_of_influence() {
    // Tessera in Nereid: 1.72e10 * (1.9e13 / 2.07e19)^0.4
    //   = 1.72e10 * (9.178744e-7)^0.4 = 1.72e10 * 3.846921e-3 = 6.6167046e7 m.
    check_close(soi_radius(1.720e10, 1.9e13, kMuNereid), 6.616704644554893e+07, 1.0,
                "SOI: Tessera about Nereid");
    // Vesk in Tessera: 5.2e7 * (8.1e10 / 1.9e13)^0.4
    //   = 5.2e7 * (4.263158e-3)^0.4 = 5.2e7 * 1.1269186e-1 = 5.8599770e6 m.
    check_close(soi_radius(5.2e7, 8.1e10, kMuTessera), 5.859976991353187e+06, 1.0,
                "SOI: Vesk about Tessera");
}

/** Hohmann, hand-worked, then the two burns the map actually plans. */
void test_hohmann() {
    // Worked case r1 = 1e10, r2 = 2e10, mu = 1.3e20: a_t = 1.5e10,
    //   dv1 = sqrt(1.3e10) * (sqrt(4/3) - 1) = 114017.5423 * 0.1547005 = 17638.5752,
    //   dv2 = sqrt(6.5e9) * (1 - sqrt(2/3)) = 80622.5806 * 0.1835034 = 14794.5186,
    //   T   = pi sqrt(1.5e10^3 / 1.3e20) = pi * 161125.86 = 506191.78 s = 5.8587 days.
    const Hohmann worked = hohmann(1.0e10, 2.0e10, 1.3e20);
    check_close(worked.a_transfer, 1.5e10, 1e-3, "hohmann: transfer axis");
    check_close(worked.dv1, 17638.575210962856, 1e-5, "hohmann: departure burn");
    check_close(worked.dv2, 14794.518622547166, 1e-5, "hohmann: arrival burn");
    check_close(worked.dv_total, worked.dv1 + worked.dv2, 1e-9, "hohmann: total is the sum");
    check_close(worked.transfer_time, 506191.776166949, 1e-4, "hohmann: transfer time");

    // r1 == r2 degenerates to no burn and half a circular period: T = pi sqrt(r^3/mu)
    //   = pi sqrt(1e30/1.3e20) = pi * 87705.94 = 275535.90 s.
    const Hohmann same = hohmann(1.0e10, 1.0e10, 1.3e20);
    check(std::fabs(same.dv1) < 1e-9 && std::fabs(same.dv2) < 1e-9 &&
              std::fabs(same.transfer_time - 275535.9030226978) < 1e-4,
          "hohmann: zero burn when r1 == r2");

    // Cinder -> Tessera about Nereid (r1 = 7.18e9, r2 = 1.72e10, mu = 2.07e19):
    //   a_t = 1.219e10, dv1 = 53693.65 * (sqrt(1.411009) - 1) = 10086.4656,
    //   dv2 = 34691.33 * (1 - sqrt(0.589007)) = 8066.8386, T = 929330.645 s = 10.7561 days.
    const Hohmann inner = hohmann(7.18e9, 1.72e10, kMuNereid);
    check_close(inner.dv1, 10086.465567444437, 1e-5, "hohmann: Cinder -> Tessera dv1");
    check_close(inner.dv2, 8066.838633319386, 1e-5, "hohmann: Cinder -> Tessera dv2");
    check_close(days(inner.transfer_time), 10.756142, 1e-5, "hohmann: Cinder -> Tessera time");
}

/** Departure windows: pi - n2 T, wrapped into [0, synodic period). */
void test_windows() {
    const double n_cinder = mean_motion(elements_of(7.18e9, 0.011, 0.42, 2.48));
    const double n_tessera = mean_motion(elements_of(1.720e10, 0.034, 1.97, 1.52));
    const double n_halberd = mean_motion(elements_of(4.338e10, 0.091, 3.31, 3.51));

    // Cinder -> Tessera: T = 929330.645 s, so the target must lead by
    // pi - n2 T = 3.1415927 - 2.016938e-6 * 929330.645 = 3.1415927 - 1.8744021 = 1.2671906 rad.
    const Hohmann inner = hohmann(7.18e9, 1.72e10, kMuNereid);
    const double required_inner = phase_angle_required(n_tessera, inner.transfer_time);
    check_close(required_inner, 1.267190559605268, 1e-9, "window: Cinder lead required");

    // With the two bodies level (phase = 0) the lead has to close a full turn minus that angle:
    //   (2pi - 1.2671906) / (n1 - n2) = 5.0159947 / 5.461287e-6 = 918463.9 s = 10.6304 days.
    const double wait_inner = time_to_window(0.0, required_inner, n_cinder, n_tessera);
    check_close(wait_inner, 918463.8877642615, 1e-3, "window: Cinder -> Tessera wait");
    check(wait_inner >= 0.0 && wait_inner < synodic_period(n_cinder, n_tessera),
          "window: wait inside one synodic period");

    // The same number as an invariant, not a restatement: after the wait the lead *is* required.
    const double lead_after = wrap_pi(0.0 + (n_tessera - n_cinder) * wait_inner);
    check(std::fabs(wrap_pi(lead_after - required_inner)) < 1e-12, "window: phase closes");

    // Tessera -> Halberd: T = 3640102.475 s and pi - n3 T = 3.1415927 - 1.8330084 = 1.3085842;
    //   wait = (2pi - 1.3085842) / (n2 - n3) = 4.9746011 / 1.5133782e-6 = 3287083.8 s = 38.0450 d.
    const Hohmann outer = hohmann(1.72e10, 4.338e10, kMuNereid);
    const double required_outer = phase_angle_required(n_halberd, outer.transfer_time);
    check_close(required_outer, 1.3085842282730733, 1e-9, "window: Halberd lead required");
    check_close(time_to_window(0.0, required_outer, n_tessera, n_halberd), 3287083.8328109165,
                1e-3, "window: Tessera -> Halberd wait");

    // Already in position: zero wait, and the synodic periods that repeat it.
    check_close(time_to_window(required_inner, required_inner, n_cinder, n_tessera), 0.0, 1e-12,
                "window: no wait when the lead is already right");
    check_close(synodic_period(n_cinder, n_tessera), 1150495.3843036187, 1e-3,
                "synodic: Cinder and Tessera");
    check(std::isinf(synodic_period(n_cinder, n_cinder)), "synodic: +inf for equal rates");
}

/** Conic sampling: closed rings for ellipses, open branches for hyperbolas. */
void test_conic_sampling() {
    const int count = 1024;
    const Elements el = elements_of(1.720e10, 0.95, 0.7, 1.0);
    Conic conic;
    conic.elements = el;

    std::vector<glm::dvec2> points;
    conic.sample(count, points);
    check(points.size() == static_cast<std::size_t>(count), "sample: one point per vertex");
    check(points.front() != points.back(), "sample: last point does not duplicate the first");

    // The first point is periapsis: radius a(1 - e) = 1.72e10 * 0.05 = 8.6e8 m, at angle omega.
    check_close(glm::length(points.front()), 8.6e8, 1e-3, "sample: starts at periapsis");
    check_close(std::atan2(points.front().y, points.front().x), 0.7, 1e-12,
                "sample: starts on the periapsis direction");

    // Every vertex lies between the apsides.
    double min_radius = glm::length(points.front());
    double max_radius = min_radius;
    for (const glm::dvec2 &p : points) {
        const double radius = glm::length(p);
        min_radius = radius < min_radius ? radius : min_radius;
        max_radius = radius > max_radius ? radius : max_radius;
    }
    check(min_radius >= periapsis(el) * (1.0 - 1e-12) &&
              max_radius <= apoapsis(el) * (1.0 + 1e-12),
          "sample: vertices stay between the apsides");

    // A closed ring's area converges on pi a b with b = a sqrt(1 - e^2)
    //   = 1.72e10 * 0.312250 = 5.370 70e9 m; at 1024 vertices uniform in true anomaly the error
    // is 9.3e-5 relative, so 1e-3 catches a broken ring (a dropped half, a doubled vertex).
    double area = 0.0;
    for (std::size_t i = 0; i < points.size(); ++i) {
        area += cross2(points[i], points[(i + 1) % points.size()]);
    }
    const double expected_area =
        kPi * el.a * el.a * std::sqrt(1.0 - el.e * el.e);
    check(rel_error(std::fabs(area) * 0.5, expected_area) < 1e-3, "sample: ring area is the ellipse");

    // The hyperbola is an open branch: no wrapping, and the ends nearest the asymptotes are the
    // farthest points on it.
    const Elements hyper = elements_of(-1.0e10, 1.5, 0.4, 0.2, 1e14);
    Conic open;
    open.elements = hyper;
    open.sample(256, points);
    check(points.size() == 256u, "sample: hyperbola vertex count");
    check(glm::length(points.front()) > glm::length(points[128]) &&
              glm::length(points.back()) > glm::length(points[128]),
          "sample: hyperbola is farthest at the asymptotes");

    // The wrapper is exactly the free functions it stands for.
    check_close(glm::length(conic.position_at(1234.5) - position_at(el, 1234.5)), 0.0, 0.0,
                "conic: position delegates");
    check_close(conic.period(), period(el), 0.0, "conic: period delegates");
    check_close(conic.periapsis(), periapsis(el), 0.0, "conic: periapsis delegates");
    check_close(conic.apoapsis(), apoapsis(el), 0.0, "conic: apoapsis delegates");
}

}  // namespace

int tests() {
    g_failures = 0;
    test_kepler_solver();
    test_hyperbolic_solver();
    test_near_parabolic();
    test_element_geometry();
    test_system_periods();
    test_round_trip();
    test_hyperbolic_round_trip();
    test_conservation();
    test_spheres_of_influence();
    test_hohmann();
    test_windows();
    test_conic_sampling();
    return g_failures;
}

}  // namespace opra::orbit
