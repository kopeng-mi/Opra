// The shared vocabulary of measured quantities, so the render and simulation layers can talk
// without either including the other.
#pragma once

#include <cmath>

namespace opra {

/** Every length in the game is this: metres at 1:1, no fixed point, no rounding. */
using Real = double;

/** A point or vector on the play plane. */
struct Vec2 {
    Real x = 0;
    Real y = 0;
};

/** A hull's collision box: the drawn model's half-length and half-width, in model units. */
struct HullBoxes {
    Real halfLength;
    Real halfWidth;
};

}  // namespace opra
