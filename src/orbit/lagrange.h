// Lagrange points of a two-body system in the plane (PLAN-03 §3.5): the three collinear points
// from the mass ratio and the two triangular ones a full separation out at sixty degrees. Pure
// math: <cmath> and glm only, so a station placement is reviewable without a world or a device.
//
// A station at one of these is *not* a conic - see the ponytail in lagrange.cpp - so this header
// offers the geometry the placement needs and nothing about time.
#pragma once

#include <glm/glm.hpp>

namespace opra::orbit {

/** mu = m2 / (m1 + m2), the secondary's share of the pair. Zero masses give zero, not NaN. */
double mass_ratio(double m1, double m2);

/**
 * The collinear point's signed place along the primary->secondary line, metres: L1 inside the
 * secondary, L2 beyond it, L3 opposite it (hence negative). L1 and L2 refine the plan's closed-form
 * seed with three Newton steps on the collinear condition; L3 is the plan's own closed form.
 * Returns 0 for a point outside 1..3, a degenerate pair, or a secondary heavier than its primary.
 */
double collinear_point(double mu, double a, int point);

/**
 * The point's offset from the primary in the secondary's rotating frame, metres: +x toward the
 * secondary, +y along its motion. `dtheta` rotates the whole placement about the primary, which is
 * the file's own offset on top of the point's fixed geometry. Returns the zero vector for a bad
 * point or a degenerate pair.
 */
glm::dvec2 lagrange_offset(double mu, double a, int point, double dtheta);

}  // namespace opra::orbit
