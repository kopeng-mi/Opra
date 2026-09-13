#include "render/backdrop.h"

#include <algorithm>
#include <cmath>

#include <glm/gtc/quaternion.hpp>

#include "core/rng.h"
#include "render/mesh.h"

namespace opra {
namespace {

constexpr int STAR_COUNT = 1200;
constexpr int NEBULA_COUNT = 4;
/** Motes per nebula cloud (plan 05 S-1): many small overlapping glows, so the cloud reads as gas
 *  and no single mote is big enough to read as a circle of its own. */
constexpr int NEBULA_MOTES = 80;
constexpr int DUST_COUNT = 600;
constexpr int MOTE_COUNT = 300;

/** Metres from the eye at the reference sky shell: the sizes are angular, and add_backdrop rides
 *  the shell in and out with the camera's derived far plane (plan 05 s2.2 - a fixed sky distance
 *  falls behind the near plane long before orbital zoom). */
constexpr float SKY_SHELL = 10000.0f;
constexpr float STAR_RADIUS_MIN = 9000.0f;
constexpr float STAR_RADIUS_MAX = 11000.0f;
constexpr float NEBULA_RADIUS = 4600.0f;
/** Mote sizes, metres of full width at NEBULA_RADIUS: 1.5-4.5 degrees of sky each. Small enough
 *  that no mote reads as a circle of its own, big enough that the cloud has structure. */
constexpr float NEBULA_MOTE_MIN = 55.0f;
constexpr float NEBULA_MOTE_SPAN = 115.0f;
/** How far a mote may wander from its cloud's centre direction, radians. */
constexpr float NEBULA_SPREAD = 0.20f;

/** How far out the sky anchors are pinned: far enough that in-system travel barely turns it. */
constexpr double SKY_DISTANCE = 4.0e15;

/** Folds a coordinate into [-box, box): a layer never runs out, it re-enters on the far face. */
double fold(double value, double box) {
    const double span = box * 2.0;
    double wrapped = std::fmod(value + box, span);
    if (wrapped < 0.0) wrapped += span;
    return wrapped - box;
}

/** A uniform direction on the sphere from two unit randoms. */
glm::dvec3 sphere_direction(double u, double v) {
    const double z = u * 2.0 - 1.0;
    const double ring = std::sqrt(std::max(0.0, 1.0 - z * z));
    const double angle = v * 6.283185307179586;
    return {ring * std::cos(angle), ring * std::sin(angle), z};
}

glm::vec3 lerp(const glm::vec3 &a, const glm::vec3 &b, float t) { return a + (b - a) * t; }

/** The rotation that turns the disc's +Z onto `direction` (both unit): the nebula's billboard. */
glm::quat facing(const glm::vec3 &direction) {
    const glm::vec3 from(0.0f, 0.0f, 1.0f);
    const glm::vec3 to = -direction;
    const float alignment = glm::dot(from, to);
    if (alignment > 0.9999f) return glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
    if (alignment < -0.9999f) return glm::quat(0.0f, 1.0f, 0.0f, 0.0f);  // edge on: any axis will do
    const float angle = std::acos(glm::clamp(alignment, -1.0f, 1.0f));
    return glm::angleAxis(angle, glm::normalize(glm::cross(from, to)));
}

/**
 * Star and nebula tints. Starlight is mostly cool white with a few warm ones; the nebula keeps to
 * the almanac's hues so the sky reads as part of the same instrument.
 */
glm::vec3 star_color(Rng &rng) {
    const float warmth = static_cast<float>(rng.next());
    const glm::vec3 cool(0.72f, 0.80f, 0.92f);
    const glm::vec3 warm(0.95f, 0.84f, 0.66f);
    const glm::vec3 tint = warmth > 0.78f ? lerp(cool, warm, (warmth - 0.78f) / 0.22f) : cool;
    const float brightness = 0.35f + static_cast<float>(rng.next()) * 0.65f;
    return tint * brightness;
}

}  // namespace

Backdrop build_backdrop() {
    Backdrop backdrop;
    Rng rng(20260912);
    backdrop.stars.reserve(STAR_COUNT);
    for (int i = 0; i < STAR_COUNT; ++i) {
        Backdrop::Sky star;
        star.anchor = sphere_direction(rng.next(), rng.next()) * SKY_DISTANCE;
        star.radius = STAR_RADIUS_MIN + static_cast<float>(rng.next()) * (STAR_RADIUS_MAX - STAR_RADIUS_MIN);
        // A couple of pixels each at 24 degrees over 900 px, with a few brighter ones.
        star.size = 9.0f + static_cast<float>(rng.next() * rng.next()) * 26.0f;
        star.color = star_color(rng);
        backdrop.stars.push_back(star);
    }

    backdrop.nebula.reserve(NEBULA_COUNT * NEBULA_MOTES);
    const glm::vec3 nebula_tints[NEBULA_COUNT] = {{0.22f, 0.38f, 0.42f},
                                                  {0.34f, 0.26f, 0.40f},
                                                  {0.40f, 0.31f, 0.22f},
                                                  {0.20f, 0.34f, 0.40f}};
    // R-3: the sky is the quietest thing on screen. Same hues at half saturation; the per-mote gain
    // below keeps the brightest stacked point just under 15% of a lit hull's value, so contrast in
    // this frame comes from the star and the drive flames, both HDR and bloomed (G5).
    const auto quiet = [](const glm::vec3 &tint) {
        const float mean = (tint.x + tint.y + tint.z) / 3.0f;
        return glm::mix(glm::vec3(mean), tint, 0.5f);
    };
    // The flight view's yaw is locked to world north, so the sky it can ever look at is a band
    // around the default orbit, not a sphere: patches scattered uniformly would waste five sixths
    // of themselves on a hemisphere the camera never turns to. They are placed across that band
    // instead - still real fixed geometry at a real distance, just aimed where the instrument
    // points, which is what a survey backdrop is for.
    const glm::vec3 band = glm::normalize(glm::vec3(0.0f, 0.848f, -0.53f));
    const glm::vec3 band_right = glm::normalize(glm::cross(band, glm::vec3(0.0f, 0.0f, 1.0f)));
    const glm::vec3 band_up = glm::cross(band_right, band);
    for (int i = 0; i < NEBULA_COUNT; ++i) {
        const float lean = 0.12f + 0.55f * static_cast<float>(rng.next());
        const float roll = (static_cast<float>(i) / static_cast<float>(NEBULA_COUNT) +
                            0.17f * static_cast<float>(rng.next())) *
                           6.283185307179586f;
        const glm::vec3 centre =
            glm::normalize(band * std::cos(lean) +
                           (band_right * std::cos(roll) + band_up * std::sin(roll)) * std::sin(lean));
        const glm::vec3 tint = quiet(nebula_tints[i]);
        // S-1: one soft glow per mote, dozens per cloud. No mote carries an edge the eye can
        // locate - each falls to zero at its own rim, and the cloud's boundary is where the
        // density happens to run out, which is what a nebula is. Density falls toward the rim of
        // the cloud so the whole still fades rather than stopping.
        for (int k = 0; k < NEBULA_MOTES; ++k) {
            const float offset_roll = static_cast<float>(rng.next() * 6.283185307179586);
            // A squared radius concentrates motes at the cloud's centre.
            const float offset = NEBULA_SPREAD * static_cast<float>(std::sqrt(rng.next()));
            const glm::vec3 direction = glm::normalize(
                centre * std::cos(offset) +
                (band_right * std::cos(offset_roll) + band_up * std::sin(offset_roll)) *
                    std::sin(offset));
            Backdrop::Sky mote;
            mote.anchor = glm::dvec3(direction) * SKY_DISTANCE;
            mote.radius = NEBULA_RADIUS + static_cast<float>((rng.next() - 0.5) * 700.0);
            mote.size = NEBULA_MOTE_MIN + static_cast<float>(rng.next()) * NEBULA_MOTE_SPAN;
            // Dimmer out at the cloud's rim: gain falls with the square of the offset.
            const float gain = (1.0f - offset / NEBULA_SPREAD);
            // The gain aims a single mote's centre at a faint-but-visible glow: dimmer than any
            // lit hull by an order of magnitude (R-3), bright enough that the cloud reads as gas
            // rather than as a field of smudges over the stars.
            mote.color = tint * (0.26f * (0.35f + 0.65f * gain * gain));
            backdrop.nebula.push_back(mote);
        }
    }

    backdrop.dust.reserve(DUST_COUNT);
    for (int i = 0; i < DUST_COUNT; ++i) {
        Backdrop::Grit mote;
        mote.base = {rng.next() * 2.0 - 1.0, rng.next() * 2.0 - 1.0, rng.next() * 2.0 - 1.0};
        mote.base *= static_cast<double>(backdrop.dust_box);
        mote.base.z *= 0.35;  // a slab, not a cube: the plane is thin
        mote.drift = {static_cast<float>(rng.next() * 2.0 - 1.0) * 2.4f,
                      static_cast<float>(rng.next() * 2.0 - 1.0) * 2.4f,
                      static_cast<float>(rng.next() * 2.0 - 1.0) * 1.2f};
        mote.size = 0.9f + static_cast<float>(rng.next()) * 1.8f;
        // Half the old tone: at three pixels the grit read as a star, and a star that drifts is
        // the one lie this layer must never tell (it is the speed cue, not a sky).
        const float tone = 0.06f + static_cast<float>(rng.next()) * 0.08f;
        mote.color = glm::vec3(tone * 0.85f, tone * 0.95f, tone * 1.15f);
        backdrop.dust.push_back(mote);
    }

    backdrop.motes.reserve(MOTE_COUNT);
    for (int i = 0; i < MOTE_COUNT; ++i) {
        Backdrop::Grit mote;
        mote.base = {rng.next() * 2.0 - 1.0, rng.next() * 2.0 - 1.0, rng.next() * 2.0 - 1.0};
        mote.base *= static_cast<double>(backdrop.mote_box);
        mote.drift = {static_cast<float>(rng.next() * 2.0 - 1.0) * 6.0f,
                      static_cast<float>(rng.next() * 2.0 - 1.0) * 6.0f,
                      static_cast<float>(rng.next() * 2.0 - 1.0) * 3.0f};
        mote.size = 0.22f + static_cast<float>(rng.next()) * 0.5f;
        const float tone = 0.12f + static_cast<float>(rng.next()) * 0.14f;
        mote.color = glm::vec3(tone * 0.90f, tone * 0.96f, tone * 1.05f);
        backdrop.motes.push_back(mote);
    }
    return backdrop;
}

void add_backdrop(SceneBuilder &scene, const Backdrop &backdrop, const ModelSet &models,
                  const Camera &camera, double t, float margin) {
    const ViewFrustum view = view_frustum(camera, margin);
    const glm::vec3 eye = camera.eye;
    // The sky shell rides the derived far plane at half of it: inside the frustum at every zoom,
    // always behind everything the near pass can draw, and the radii and sizes scale with the
    // shell so every star keeps the angular size it was authored with (plan 05 s2.2 - a fixed sky
    // distance falls behind the near plane long before orbital zoom).
    const float shell = camera.far_z() * 0.5f / SKY_SHELL;

    for (const Backdrop::Sky &star : backdrop.stars) {
        const glm::dvec3 offset = star.anchor - camera.origin;
        const double distance = glm::length(offset);
        if (distance <= 0.0) continue;
        const glm::vec3 direction(offset / distance);
        const glm::vec3 at = eye + direction * (star.radius * shell);
        if (!view.contains(at, star.size * shell)) continue;
        scene.add(models.sky_mesh, at, glm::quat(1.0f, 0.0f, 0.0f, 0.0f),
                  glm::vec3(star.size * shell), star.color, InstanceLayer::Backdrop);
    }

    // Nebula: dozens of soft glows per cloud, additive so the rims fall to nothing and no mote has
    // an edge the eye can locate (S-1). The discs are billboarded about the direction they sit on.
    for (const Backdrop::Sky &mote : backdrop.nebula) {
        const glm::dvec3 offset = mote.anchor - camera.origin;
        const double distance = glm::length(offset);
        if (distance <= 0.0) continue;
        const glm::vec3 direction(offset / distance);
        const glm::vec3 at = eye + direction * (mote.radius * shell);
        if (!view.contains(at, mote.size * shell * 0.5f)) continue;
        const glm::quat billboard = facing(direction);
        scene.add(models.glow_mesh, at, billboard,
                  glm::vec3(mote.size * shell, mote.size * shell, 1.0f), mote.color,
                  InstanceLayer::Backdrop);
    }

    // Grit: folded into a box around the render origin and drifted by the clock, so a mote that
    // leaves one face re-enters on the other and the layer is never flown out of.
    const auto place_grit = [&](const std::vector<Backdrop::Grit> &layer, double box, double slab) {
        for (const Backdrop::Grit &mote : layer) {
            const glm::vec3 at(
                static_cast<float>(fold(mote.base.x + mote.drift.x * t, box)),
                static_cast<float>(fold(mote.base.y + mote.drift.y * t, box)),
                static_cast<float>(fold(mote.base.z + mote.drift.z * t, box * slab)));
            if (!view.contains(at, mote.size)) continue;
            scene.add(models.sky_mesh, at, glm::quat(1.0f, 0.0f, 0.0f, 0.0f),
                      glm::vec3(mote.size), mote.color, InstanceLayer::Backdrop);
        }
    };
    place_grit(backdrop.dust, static_cast<double>(backdrop.dust_box), 0.35);
    place_grit(backdrop.motes, static_cast<double>(backdrop.mote_box), 0.55);
}

}  // namespace opra
