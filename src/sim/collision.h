// 2D collision primitives, ported from the DRIFT source (AstraWars/src/collision.ts).
// Real is double so the port reproduces the original numerics exactly.
#pragma once

#include "core/units.h"

namespace opra {

/** A circle in the navigation plane, in world metres. */
struct Circle {
    Real x = 0;
    Real y = 0;
    Real radius = 0;
};

/** An oriented box: halfLength runs along the local forward axis (+y rotated by `angle`). */
struct Box {
    Real x = 0;
    Real y = 0;
    Real halfLength = 0;
    Real halfWidth = 0;
    Real angle = 0;
};

/** Minimum translation that pushes the circle out of the box. False when they are apart. */
bool obb_circle_out(const Box &box, Real cx, Real cy, Real radius, Vec2 &out);

/** Minimum translation that pushes box `b` out of box `a` (separating axis theorem). */
bool obb_obb_out(const Box &a, const Box &b, Vec2 &out);

bool point_in_circle(const Circle &circle, Real x, Real y);

bool point_in_box(const Box &box, Real x, Real y);

/** First normalized hit time for a point swept from (x0,y0) to (x1,y1) against a circle. */
bool segment_circle_hit(Real x0, Real y0, Real x1, Real y1, const Circle &circle, Real radius,
                        Real &out_t);

}  // namespace opra
