// The backdrop: stars, dust and near motes, as real instanced geometry at real distances,
// so perspective produces the parallax instead of a per-layer offset factor (plan 06 §5, L7).
//
// Stars are placed by direction: their anchors are pinned far out in double metres, so
// crossing the system turns the sky by the real angle and nothing has to be animated. Dust and
// motes live in the local volume instead - they fold into a box around the render origin as the
// ship travels, and they are the only layers that read as speed.
#pragma once

#include <vector>

#include <glm/glm.hpp>

#include "render/camera.h"
#include "render/gltf.h"
#include "render/scene.h"

namespace opra {

struct Backdrop {
    /** Anything placed by direction at a fixed distance from the eye. */
    struct Sky {
        glm::dvec3 anchor;       // world, double metres: the direction comes from anchor - origin
        float radius = 9000.0f;  // metres from the eye: the sky is placed, not simulated
        float size = 1.0f;       // instance scale, metres (quads: half extent)
        glm::vec3 color{1.0f};
    };
    /** Anything that drifts through the local volume. */
    struct Grit {
        glm::dvec3 base;     // metres inside the layer's box
        glm::vec3 drift;     // metres per second
        float size = 1.0f;   // instance scale, metres
        glm::vec3 color{1.0f};
    };

    std::vector<Sky> stars;   // 700 points (plan 06 §5)
    std::vector<Grit> dust;   // 600 motes in a wide box
    std::vector<Grit> motes;  // 300 motes in a box around the hull
    float dust_box = 900.0f;
    float mote_box = 70.0f;
};

/** Generated once from a fixed seed: the same run draws the same sky, so a capture repeats. */
Backdrop build_backdrop();

/**
 * Places every layer for one frame. `t` is the simulation clock and every grit position is a pure
 * function of it, so the field drifts without carrying state and a screenshot is reproducible.
 */
void add_backdrop(SceneBuilder &scene, const Backdrop &backdrop, const ModelSet &models,
                  const Camera &camera, double t, float margin);

}  // namespace opra
