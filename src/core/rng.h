// mulberry32, bit-for-bit with the original so the rock field and the meshes match it exactly.
#pragma once

#include "core/units.h"

namespace opra {

struct Rng {
    unsigned int seed;
    explicit Rng(unsigned int initial) : seed(initial) {}
    Real next();
};

}  // namespace opra
