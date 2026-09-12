#include "orbit/lagrange.h"

#include <cmath>

// ponytail: co-rotating placement, not a conic. CR3BP if station-keeping ever becomes gameplay.

namespace opra::orbit {
namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr int kNewtonSteps = 3;

/**
 * The collinear condition on the primary->secondary line: the pull of both masses balanced against
 * the centrifugal term about the system's barycentre, which sits at mu*a from the primary.
 *
 * The plan prints the balance as (1-mu)/(a-r)^2 - mu/r^2 = (1-mu)(a-r)/a^3, which is the small-mu
 * truncation of this and only reads correctly with `r` measured from the *secondary*, while the
 * plan's own seed line measures `r` from the primary. The exact barycentric term (r - mu*a)/a^3 is
 * used here instead: it costs one subtraction, the plan's seed is still the right starting point,
 * and the root is the libration point rather than a tenth of a percent off it at Earth-Moon.
 * `beyond` selects L2's branch, where the secondary's term adds rather than subtracts.
 */
double collinear_residual(double mu, double a, double r, bool beyond) {
    const double arm = beyond ? r - a : a - r;
    return (1.0 - mu) / (r * r) + (beyond ? 1.0 : -1.0) * mu / (arm * arm) -
           (r - mu * a) / (a * a * a);
}

/** The residual's derivative with respect to `r`; the same sign choice as the residual. */
double collinear_slope(double mu, double a, double r, bool beyond) {
    const double arm = beyond ? r - a : a - r;
    return -2.0 * (1.0 - mu) / (r * r * r) - 2.0 * mu / (arm * arm * arm) - 1.0 / (a * a * a);
}

/** Three Newton steps from the plan's closed-form seed. A step that leaves the domain keeps the
 *  last good value: a hand-edited mass ratio is a data error, not a licence to return NaN. */
double refine(double mu, double a, double r, bool beyond) {
    for (int i = 0; i < kNewtonSteps; ++i) {
        const double step = collinear_residual(mu, a, r, beyond) / collinear_slope(mu, a, r, beyond);
        const double next = r - step;
        if (!std::isfinite(next) || next <= 0.0 || (!beyond && next >= a)) break;
        r = next;
    }
    return r;
}

}  // namespace

double mass_ratio(double m1, double m2) {
    const double total = m1 + m2;
    return total > 0.0 ? m2 / total : 0.0;
}

double collinear_point(double mu, double a, int point) {
    if (!(a > 0.0) || !(mu > 0.0) || mu >= 1.0 || point < 1 || point > 3) return 0.0;
    if (point == 3) return -a * (1.0 + 5.0 * mu / 12.0);

    // The plan's seed, a(1 -/+ (mu/3)^(1/3)): the standard first-order distance, which the Newton
    // steps above take to the exact point.
    const double seed = a * (1.0 + (point == 1 ? -1.0 : 1.0) * std::cbrt(mu / 3.0));
    return refine(mu, a, seed, point == 2);
}

glm::dvec2 lagrange_offset(double mu, double a, int point, double dtheta) {
    if (point < 1 || point > 5) return glm::dvec2(0.0);
    // L4 and L5 are a full separation out at sixty degrees from the secondary, leading and
    // trailing it; the collinear trio carries its sign in the radius itself.
    const double radius = point <= 3 ? collinear_point(mu, a, point) : a;
    if (radius == 0.0 || !(a > 0.0)) return glm::dvec2(0.0);
    const double base = point == 4 ? kPi / 3.0 : point == 5 ? -kPi / 3.0 : 0.0;
    const double angle = base + dtheta;
    return glm::dvec2(radius * std::cos(angle), radius * std::sin(angle));
}

}  // namespace opra::orbit
