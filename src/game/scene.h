// Places the world's objects as instances: the one module that knows both the simulation and the
// renderer, so neither of them has to know the other.
#pragma once

#include <vector>

#include <glm/glm.hpp>

#include "render/backdrop.h"
#include "render/camera.h"
#include "render/gltf.h"
#include "render/orrery.h"
#include "render/scene.h"
#include "sim/world.h"

namespace opra {

/** Places every instance for one frame: backdrop, rock field, structures, cargo, ore, and the ship. */
void build_scene(SceneBuilder &scene, const ModelSet &models, const World &world,
                 const Backdrop &backdrop, const Camera &camera, float screen_width,
                 float screen_height);

/**
 * The orrery's neutral frame, built from the world: the one place that knows both the system and
 * the chart's own vocabulary (render/orrery.h). Metres, double, about the barycentre.
 */
orrery::Frame orrery_frame_for(const World &world, bool true_scale, int target_body = -1);

}  // namespace opra
