#include "render/backdrop.h"

#include <algorithm>
#include <cmath>

#include "core/rng.h"
#include "render/mesh.h"

namespace opra {
namespace {

// Stars retuned per plan 06 §5 (L7, fixes T-11, T-12): 700 stars, size 1.3-3.2 px, max value 0.60
// (under the 0.65 bloom knee). Nebula is deleted.
constexpr int STAR_COUNT = 700;
constexpr int DUST_COUNT = 600;
constexpr int MOTE_COUNT = 300;

/** Metres from the eye at the reference sky shell: the sizes are angular, and add_backdrop rides
 *  the shell in and out with the camera's derived far plane (plan 05 s2.2 - a fixed sky distance
 *  falls behind the near plane long before orbital zoom). */
constexpr float SKY_SHELL = 10000.0f;
constexpr float STAR_RADIUS_MIN = 10000.0f;
constexpr float STAR_RADIUS_MAX = 11000.0f;

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

/**
 * Star tints. Starlight is mostly cool white with a few warm ones.
 * Values capped at 0.60 (under the 0.65 bloom knee) to prevent glare (plan 06 §5).
 */
glm::vec3 star_color(Rng &rng) {
    const float warmth = static_cast<float>(rng.next());
    const glm::vec3 cool(0.72f, 0.80f, 0.92f);
    const glm::vec3 warm(0.95f, 0.84f, 0.66f);
    const glm::vec3 tint = warmth > 0.78f ? lerp(cool, warm, (warmth - 0.78f) / 0.22f) : cool;
    const double r = rng.next();
    const float brightness = 0.18f + static_cast<float>(r * r) * 0.42f;
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
        // Star size: 6 + rng^2 * 9 -> 1.3 - 3.2 px, a point source (plan 06 §5).
        const double r = rng.next();
        star.size = 6.0f + static_cast<float>(r * r) * 9.0f;
        star.color = star_color(rng);
        backdrop.stars.push_back(star);
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
    // shell so every star keeps the angular size it was authored with (plan 05 s2.2).
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
