// Two-body Kepler propagation in the ecliptic plane (z = 0). Elements are the classical in-plane
// set; every orbit in the game is analytic, which is what makes warp exact (E2) and transfer
// planning closed-form. Pure math: <cmath> and glm only, no SDL, no world types.
#pragma once

#include <glm/glm.hpp>

namespace opra::orbit {

/** Classical in-plane elements about `mu`. `a` is negative for a hyperbolic orbit. */
struct Elements {
    double a = 0;      // semi-major axis, metres
    double e = 0;      // eccentricity
    double omega = 0;  // argument of periapsis, radians
    double M0 = 0;     // mean anomaly at `t0`, radians
    double t0 = 0;     // epoch, seconds
    double mu = 0;     // gravitational parameter of the primary, m^3/s^2
};

/** Cartesian state in the primary's frame. */
struct State {
    glm::dvec2 r;
    glm::dvec2 v;
};

/** sqrt(mu / |a|^3), always positive — the sign of `a` never enters the rate. */
double mean_motion(const Elements &elements);

/** E from `E - e sin E = M`; `e` is clamped into [0, 1) when a caller steps outside it. */
double solve_kepler(double M, double e);

/** H from `e sinh H - H = M`; `e` is clamped into (1, inf) when a caller steps outside it. */
double solve_hyperbolic(double M, double e);

glm::dvec2 position_at(const Elements &elements, double t);
glm::dvec2 velocity_at(const Elements &elements, double t);
State state_at(const Elements &elements, double t);

/**
 * Inverse of the above, for a state at `t0`. A circular orbit has no periapsis direction, so
 * `omega` is set to 0 and the phase carries the whole geometry. The element set has no direction
 * flag: a retrograde state (`r x v` < 0) is converted to the prograde conic through the same
 * point, because rot(omega) of a prograde conic cannot sweep the other way.
 */
Elements from_state(const glm::dvec2 &r, const glm::dvec2 &v, double mu, double t0);

/** True when the conic is closed. `e >= 1` escapes and has no period or apoapsis. */
bool is_elliptic(const Elements &elements);

/** Seconds per revolution; +inf when the orbit is not elliptic. */
double period(const Elements &elements);

/** Closest approach, a(1 - e) — positive for both branches. */
double periapsis(const Elements &elements);

/** Farthest point, a(1 + e); +inf when the orbit is not elliptic. */
double apoapsis(const Elements &elements);

/**
 * |e - 1| < 1e-4. The measure-zero case no player can hold: the two branches meet there and
 * Newton's derivative vanishes at periapsis, so both solvers clamp across the band.
 */
bool near_parabolic(double e);

}  // namespace opra::orbit
