#include "core/rng.h"

namespace opra {

Real Rng::next() {
    seed = seed + 0x6D2B79F5u;
    unsigned int t = seed;
    t = (t ^ (t >> 15)) * (1u | t);
    t = (t + (t ^ (t >> 7)) * (61u | t)) ^ t;
    return static_cast<Real>(t ^ (t >> 14)) / 4294967296.0;
}

}  // namespace opra
