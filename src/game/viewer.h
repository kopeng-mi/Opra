// The model viewer: the whole point of the export pipeline. An author writes a model, runs the
// exporter, presses F5, and looks at it here - turntable, grid, normals, collision box and the
// sidecar's numbers, without a rebuild.
#pragma once

#include <vector>

#include <glm/glm.hpp>

#include "game/input.h"
#include "game/scene.h"
#include "render/camera.h"
#include "render/gltf.h"
#include "render/scene.h"
#include "ui/ui.h"

namespace opra {

struct Viewer {
    int selected = 0;
    /** Turntable: yaw and pitch in radians, distance as a multiple of the framed extent. */
    float yaw = 0.75f;
    float pitch = 0.42f;
    float distance = 1.0f;
    bool grid = true;
    bool normals = false;
    bool boxes = false;
    bool effects = true;
    /** Last pointer position, so a drag can orbit without a delta in the input struct. */
    glm::vec2 pointer{0.0f};
    bool dragging = false;

    /** The camera this frame, framing the selected model in a `width` x `height` drawable. */
    Camera camera(const ModelSet &models, Uint32 width, Uint32 height) const;
};

/** Mouse and keys: orbit with a drag, zoom with the wheel, G/N/B/E for the overlays, F5 reloads. */
void update_viewer(Viewer &viewer, const Input &input, const ModelSet &models);

/** The selected model at the origin, with whichever overlays are switched on. */
void build_viewer_scene(SceneBuilder &scene, const ModelSet &models, const Viewer &viewer);

/** Left: the model list. Right: the sidecar as text. */
void build_viewer_ui(ui::Context &ui, const ui::Rect &screen, const ModelSet &models,
                     Viewer &viewer);

}  // namespace opra
