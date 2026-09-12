#include "sim/terrain.h"

#include <algorithm>
#include <cmath>

namespace opra {
namespace {

constexpr double TWO_PI = 6.283185307179586;

/**
 * A 32-bit integer hash. shaders/planet.hlsl carries the same constants and the same expression, so
 * the silhouette seen from orbit is the ground the collision code samples. Change one and the other
 * is a lie: the seed comes from the body's terrain in the system file.
 */
double hash01(int i, unsigned int seed) {
    unsigned int h = static_cast<unsigned int>(i) * 747796405u + seed * 2891336453u;
    h ^= h >> 15;
    h *= 2246822519u;
    h ^= h >> 13;
    h *= 3266489917u;
    h ^= h >> 16;
    return static_cast<double>(h) * (1.0 / 4294967296.0);
}

/** Value noise on a unit lattice, smoothstepped. The shader's `value_noise` in one dimension. */
double value_noise(double x, unsigned int seed) {
    const double base = std::floor(x);
    const int i = static_cast<int>(base);
    const double t = x - base;
    const double s = t * t * (3.0 - 2.0 * t);
    const double a = hash01(i, seed);
    const double b = hash01(i + 1, seed);
    return a + (b - a) * s;
}

double wrap(double theta) {
    const double t = std::fmod(theta, TWO_PI);
    return t < 0.0 ? t + TWO_PI : t;
}

int sample_of(const SurfaceProfile &profile, double theta) {
    const double turns = wrap(theta) / TWO_PI * static_cast<double>(TERRAIN_SAMPLES);
    return static_cast<int>(turns) % TERRAIN_SAMPLES;
}

}  // namespace

SurfaceProfile generate_surface(const Body &body) {
    SurfaceProfile profile;
    profile.radius = body.radius;
    profile.height.assign(TERRAIN_SAMPLES, 0.0);
    if (!body.terrain.present) return profile;

    const double amplitude = body.radius * body.terrain.amplitude;
    const unsigned int seed = body.terrain.seed;
    for (int i = 0; i < TERRAIN_SAMPLES; ++i) {
        const double theta = TWO_PI * static_cast<double>(i) / static_cast<double>(TERRAIN_SAMPLES);
        double h = 0.0;
        double octave = amplitude;
        double frequency = TERRAIN_FREQUENCY;
        for (int o = 0; o < TERRAIN_OCTAVES; ++o) {
            const double phase = static_cast<double>(o) * 0.6180339887;
            // Each octave gets its own lattice: a shared seed would stack the octaves into one
            // spike at the origin instead of a ridge line.
            h += octave * value_noise(theta * frequency + phase,
                                      seed + static_cast<unsigned int>(o) * 7919u);
            octave *= TERRAIN_GAIN;
            frequency *= 2.0;
        }
        profile.height[static_cast<size_t>(i)] = h;
    }

    // Pads: forced flat across their span, tapered over one sample into their neighbours, so a pad
    // never sits on a cliff and a landing on one is a landing on a deck.
    for (const Pad &pad : body.terrain.pads) {
        const double half_span = pad.width / (2.0 * body.radius);
        const int centre = sample_of(profile, pad.theta);
        const int span = std::max(1, static_cast<int>(std::ceil(half_span / TWO_PI *
                                                                  TERRAIN_SAMPLES)));
        const double level = profile.height[static_cast<size_t>(centre)];
        for (int k = -span - 1; k <= span + 1; ++k) {
            const double distance = std::fabs(static_cast<double>(k));
            const double mix = distance <= static_cast<double>(span)
                                   ? 1.0
                                   : std::max(0.0, static_cast<double>(span) + 1.0 - distance);
            if (mix <= 0.0) continue;
            const int index = ((centre + k) % TERRAIN_SAMPLES + TERRAIN_SAMPLES) % TERRAIN_SAMPLES;
            double &height = profile.height[static_cast<size_t>(index)];
            height = height * (1.0 - mix) + level * mix;
        }
    }
    return profile;
}

double terrain_height(const SurfaceProfile &profile, double theta) {
    if (profile.height.size() != static_cast<size_t>(TERRAIN_SAMPLES)) return 0.0;
    const double turns = wrap(theta) / TWO_PI * static_cast<double>(TERRAIN_SAMPLES);
    const int i = static_cast<int>(turns) % TERRAIN_SAMPLES;
    const double t = turns - std::floor(turns);
    const double a = profile.height[static_cast<size_t>(i)];
    const double b = profile.height[static_cast<size_t>((i + 1) % TERRAIN_SAMPLES)];
    return a + (b - a) * t;
}

double terrain_slope(const SurfaceProfile &profile, double theta) {
    if (profile.height.size() != static_cast<size_t>(TERRAIN_SAMPLES) || profile.radius <= 0.0) {
        return 0.0;
    }
    const double step = TWO_PI / static_cast<double>(TERRAIN_SAMPLES);
    const double before = terrain_height(profile, theta - step);
    const double after = terrain_height(profile, theta + step);
    const double run = 2.0 * step * profile.radius;
    return std::atan2(after - before, run);
}

double terrain_radius(const SurfaceProfile &profile, double theta) {
    return profile.radius + terrain_height(profile, theta);
}

int pad_index_at(const Body &body, double theta) {
    for (size_t i = 0; i < body.terrain.pads.size(); ++i) {
        const Pad &pad = body.terrain.pads[i];
        if (body.radius <= 0.0) continue;
        const double half_span = pad.width / (2.0 * body.radius);
        double delta = std::fmod(theta - pad.theta, TWO_PI);
        if (delta > 3.141592653589793) delta -= TWO_PI;
        if (delta < -3.141592653589793) delta += TWO_PI;
        if (std::fabs(delta) <= half_span) return static_cast<int>(i);
    }
    return -1;
}

}  // namespace opra
