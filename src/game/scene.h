// Places the world's objects as instances: the one module that knows both the simulation and the
// renderer, so neither of them has to know the other.
#pragma once

#include <unordered_map>
#include <vector>

#include <glm/glm.hpp>

#include "render/backdrop.h"
#include "render/camera.h"
#include "render/gltf.h"
#include "render/orrery.h"
#include "render/scene.h"
#include "sim/world.h"
#include "ui/draw.h"

namespace opra {

/**
 * Places every instance for one frame: backdrop, rock field, structures, cargo, ore, and the ship.
 * `lod_memory` is the hysteresis state (plan 05 s2.5): keyed per object, it survives frames so an
 * object sitting on a threshold does not pop-flicker between representations.
 */
void build_scene(SceneBuilder &scene, const ModelSet &models, const World &world,
                 const Backdrop &backdrop, const Camera &camera, float screen_width,
                 float screen_height, std::unordered_map<unsigned long long, int> &lod_memory);

/** Places every live placement of a design. `lod` is chosen once for the whole ship (§7.4). */
void add_design(SceneBuilder &scene, const ModelSet &models, const ShipDesign &design,
                const glm::vec3 &origin, const glm::quat &rotation, float scale, int lod,
                float thrust, const std::vector<Real> &jets);

/**
 * s2.5's selection: the level for an object `px` pixels across, given the level it already drew
 * at. Thresholds 8 / 60 / 250 px with a ten percent deadband - a model at the bold LOD stays
 * there until 275 px or 54 px, which costs nothing and removes the boundary pop-flicker.
 * 0 icon, 1 blocky, 2 bold, 3 detailed.
 */
int lod_level(float px, int previous);

/**
 * The orrery's neutral frame, built from the world: the one place that knows both the system and
 * the chart's own vocabulary (render/orrery.h). Metres, double, about the barycentre. The plate is
 * the orrery's last home - the map screen it served is one continuous zoom now (plan 05 J2).
 */
orrery::Frame orrery_frame_for(const World &world, int target_body = -1);

/**
 * The zoom overlay (s2.4, s2.6): orbit lines, transfer arcs and the predicted trajectory as
 * projected screen-space polylines, plus the vector icons for anything too small to draw as
 * geometry. Drawn in the UI batch, after tonemap, with the depth test always off.
 */
void build_zoom_overlay(UIBatch &ui, const World &world, const Camera &camera, float width,
                        float height);

}  // namespace opra
