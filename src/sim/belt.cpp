#include "sim/belt.h"

#include <cmath>

#include "core/rng.h"

namespace opra {
namespace {

constexpr double TAU = 6.283185307179586;

}  // namespace

std::vector<BeltBody> generate_belt(const SystemDef &system) {
    const Belt &belt = system.belt;
    const Body &primary = system.bodies[static_cast<size_t>(belt.parent)];
    std::vector<BeltBody> ring;
    if (belt.count <= 0) return ring;
    ring.reserve(static_cast<size_t>(belt.count));
    Rng rng(belt.seed);
    for (int i = 0; i < belt.count; ++i) {
        BeltBody rock;
        // Semi-major axes spread across the ring, eccentricities small: this is rubble, not planets.
        const double radial = rng.next();
        rock.elements.a = belt.inner + (belt.outer - belt.inner) * radial;
        rock.elements.e = 0.0004 + 0.006 * rng.next();
        rock.elements.omega = rng.next() * TAU;
        rock.elements.M0 = rng.next() * TAU;
        rock.elements.t0 = system.epoch;
        rock.elements.mu = primary.mu;
        // A few big ones and a long tail of small ones, which is what a real belt's size
        // distribution looks like and keeps the ring legible when it is drawn.
        const double bias = rng.next() * rng.next();
        rock.radius = 6.0 + 900.0 * bias;
        rock.seed = static_cast<int>(rng.next() * 1.0e6);
        ring.push_back(rock);
    }
    return ring;
}

std::vector<ClusterRock> generate_cluster(const Belt &belt, int count, double span) {
    std::vector<ClusterRock> cluster;
    if (count <= 0) return cluster;
    cluster.reserve(static_cast<size_t>(count));
    // The cluster's own stream of the belt seed: the ring and the field are the same distribution,
    // sampled twice, so a system file describes both.
    Rng rng(belt.seed ^ 0x9e3779b9u);
    for (int i = 0; i < count; ++i) {
        ClusterRock rock;
        const double radial = (rng.next() * 2.0 - 1.0) * span * 0.5;
        const double along = (rng.next() * 2.0 - 1.0) * span * 0.5;
        rock.offset = {radial, along};
        const double bias = rng.next();
        // R-2: the same cubic tail the live field uses, so a cluster ring and the zone it sits in
        // read as one population instead of two.
        rock.radius = 5.0 + bias * bias * bias * 44.0;
        rock.seed = static_cast<int>(rng.next() * 1.0e6);
        cluster.push_back(rock);
    }
    return cluster;
}

}  // namespace opra
