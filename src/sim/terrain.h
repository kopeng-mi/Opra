// A body's surface as the sim sees it: N height samples around one circumference (plan 03 3.2).
//
// The samples are the whole surface model. Sampling is linear between the two bracketing samples and
// the derivative between them gives the local slope, which is what the touchdown gate reads. A pad
// is a forced-flat span, not a sampled feature: at 4096 samples a 6.3e6 m body is a 9.7 km spacing,
// so a pad has to be imposed, not discovered.
#pragma once

#include <vector>

#include "sim/system.h"

namespace opra {

/** One body's terrain profile: N height samples around the circumference (plan 3.2). */
struct SurfaceProfile {
    int body = -1;
    double radius = 0.0;         // the body's mean radius, metres
    std::vector<double> height;  // h(theta), N even samples, metres above `radius`
};

/** Samples around the circumference. Fixed: the shader's displacement and the sim share the count. */
inline constexpr int TERRAIN_SAMPLES = 4096;

/** Octaves in the summed noise, and the gain and frequency the plan's formula names. */
inline constexpr int TERRAIN_OCTAVES = 6;
inline constexpr double TERRAIN_GAIN = 0.55;
inline constexpr double TERRAIN_FREQUENCY = 1.0;  // lattice cells per radian at octave 0

/** h(theta) at any angle, metres above the body's mean radius. Wraps. */
double terrain_height(const SurfaceProfile &profile, double theta);

/** The local slope in radians, from the two bracketing samples. */
double terrain_slope(const SurfaceProfile &profile, double theta);

/** The body's radius at an angle: mean radius plus the heightfield. */
double terrain_radius(const SurfaceProfile &profile, double theta);

/** True when `theta` is inside one of the body's pads, and which one; -1 for open ground. */
int pad_index_at(const Body &body, double theta);

/**
 * Generates a profile: six octaves of value noise, then the pads forced flat with a one-sample
 * taper into their neighbours, so no pad sits on a cliff. A body with no terrain declared gets an
 * empty profile and a caller that treats it as a smooth sphere.
 */
SurfaceProfile generate_surface(const Body &body);

}  // namespace opra
