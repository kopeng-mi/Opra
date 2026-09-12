// Compound colliders (PLAN-02 §3.7): a hull is a handful of boxes and circles, not one box around
// the whole silhouette. Broad phase on the bounding circle, narrow phase by reusing the primitives
// in sim/collision.h, so an irregular hull stops registering hits in the gap between its parts.
#pragma once

#include <vector>

#include "core/units.h"
#include "sim/collision.h"

namespace opra {

/** One collider primitive in the hull's own frame: metres, nose +Y, starboard +X. */
struct Shape {
    enum class Kind { Box, Circle };
    Kind kind = Kind::Box;
    /** Centre, in the hull's frame. */
    Vec2 local_pos;
    /** Radians about the hull's z; boxes only. */
    Real local_angle = 0;
    /** Box half extents. `half_length` runs forward (+Y), `half_width` starboard (+X). */
    Real half_length = 0;
    Real half_width = 0;
    /** Circles only. */
    Real radius = 0;
};

/** A hull's compound collider plus the bounding circle the broad phase rejects against. */
struct Collider {
    std::vector<Shape> shapes;
    Real bounds_radius = 0;
};

/** The single-box collider: the stock hull fallback, and what a legacy sidecar degrades to. */
Collider box_collider(Real half_length, Real half_width);

/** Axis-aligned half extents of the shape set about the hull origin (width +X, length +Y). The
 *  collar radius and the cutter muzzle want extents, not shapes. */
HullBoxes collider_bounds(const Collider &collider);

/** Broad phase: true when no shape can reach a circle of `radius` centred at (cx, cy). It may say
 *  "not clear" for a miss, never "clear" for a contact - the cheap reject has to be conservative. */
bool collider_clear(const Collider &collider, const Vec2 &position, Real cx, Real cy, Real radius);

/**
 * The deepest MTV separating a posed collider from a world circle. The push moves the collider, so
 * the caller adds it to the hull. False when nothing overlaps.
 *
 * One MTV, not the sum of every shape's: two shapes both seeing the same rock would otherwise apply
 * the contact impulse twice, and a hull can leave a rock faster than restitution allows. The
 * deepest shape is the one that resolves the contact; the shallower overlaps resolve next step.
 */
bool collider_circle_out(const Collider &collider, const Vec2 &position, Real angle, Real cx, Real cy,
                         Real radius, Vec2 &out);

/** The same against a world box, for the solid bodies. The push moves the collider. */
bool collider_box_out(const Collider &collider, const Vec2 &position, Real angle, const Box &box,
                      Vec2 &out);

}  // namespace opra
