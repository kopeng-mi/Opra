// The belt: a real ring of orbiting bodies, generated from the system file's seed so the same
// system always produces the same field. Nothing here is drawn or stepped by hand - the ring is
// data, and the pilot's mineable field is the co-orbiting cluster inside it.
#pragma once

#include <vector>

#include "sim/system.h"

namespace opra {

/** One member of the ring: an orbiting body, not a rock in a box. */
struct BeltBody {
    /** Radius of the body itself, metres. */
    double radius = 0.0;
    /** Shape seed, stable for a given belt seed. */
    int seed = 0;
    orbit::Elements elements{};
};

/** The belt: `belt.count` rocks on seeded near-circular orbits about the belt's parent. */
std::vector<BeltBody> generate_belt(const SystemDef &system);

/**
 * The field a pilot works in: `count` rocks in a `span`-metre clump that shares the primary's
 * orbit, so their relative velocities are below the simulation's resolution and the clump stays a
 * clump. Offsets are in the co-orbiting frame (x radial, y along-track), which is the frame the
 * flight box is measured in.
 */
struct ClusterRock {
    glm::dvec2 offset;
    double radius = 0.0;
    int seed = 0;
};
std::vector<ClusterRock> generate_cluster(const Belt &belt, int count, double span);

}  // namespace opra
