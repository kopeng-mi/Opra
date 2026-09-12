#include "game/viewer.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

#include "game/bindings.h"
#include "hud/hud.h"
#include "ui/ui.h"

namespace opra {
namespace {

/** Half the model's longest axis, so the turntable framing follows the model. */
float model_extent(const ModelMeta &meta) {
    const glm::vec3 size = meta.aabb_max - meta.aabb_min;
    return std::max(4.0f, std::max(size.x, std::max(size.y, size.z)) * 0.5f);
}

/** One thin cube: the viewer's only drawing primitive, which keeps it to one mesh and one run. */
void edge(SceneBuilder &scene, int mesh, const glm::vec3 &a, const glm::vec3 &b, float thickness,
          const glm::vec3 &color) {
    const glm::vec3 delta = b - a;
    const float length = glm::length(delta);
    if (length < 1e-4f) return;
    const glm::vec3 mid = (a + b) * 0.5f;
    const glm::vec3 axis = delta / length;
    const glm::vec3 reference = std::abs(axis.z) < 0.9f ? glm::vec3(0.0f, 0.0f, 1.0f)
                                                        : glm::vec3(1.0f, 0.0f, 0.0f);
    const glm::vec3 side = glm::normalize(glm::cross(axis, reference));
    const glm::vec3 up = glm::cross(side, axis);
    const glm::mat3 basis(side, axis, up);
    scene.add(mesh, mid, glm::quat_cast(basis), glm::vec3(thickness, length, thickness), color);
}

/** The twelve edges of an axis-aligned box, centred where the caller says. */
void box_edges(SceneBuilder &scene, int mesh, const glm::vec3 &min, const glm::vec3 &max,
               float thickness, const glm::vec3 &color) {
    const glm::vec3 corners[8] = {{min.x, min.y, min.z}, {max.x, min.y, min.z},
                                  {min.x, max.y, min.z}, {max.x, max.y, min.z},
                                  {min.x, min.y, max.z}, {max.x, min.y, max.z},
                                  {min.x, max.y, max.z}, {max.x, max.y, max.z}};
    const int pairs[12][2] = {{0, 1}, {2, 3}, {4, 5}, {6, 7}, {0, 2}, {1, 3},
                              {4, 6}, {5, 7}, {0, 4}, {1, 5}, {2, 6}, {3, 7}};
    for (const auto &pair : pairs) {
        edge(scene, mesh, corners[pair[0]], corners[pair[1]], thickness, color);
    }
}

/**
 * The collider as the sim will use it, drawn in the model's own frame: each shape's outline on the
 * model's mid-plane, which is where the shapes live. This is the overlay that catches an export
 * whose shapes do not match the geometry.
 *
 * ponytail: flat outlines, no prism. The shapes are 2D; giving them a drawn height would imply a
 * depth the sim does not have. Upgrade path: a per-shape z half-extent in the sidecar.
 */
void shape_edges(SceneBuilder &scene, int mesh, const ModelMeta &meta, float thickness,
                 const glm::vec3 &color) {
    for (const ModelShape &shape : meta.shapes) {
        const glm::vec3 origin(static_cast<float>(shape.pos.x), static_cast<float>(shape.pos.y), 0.0f);
        if (shape.kind == ModelShape::Kind::Circle) {
            constexpr int kSegments = 24;
            const float radius = static_cast<float>(shape.radius);
            glm::vec3 previous(origin.x + radius, origin.y, 0.0f);
            for (int i = 1; i <= kSegments; ++i) {
                const float angle = static_cast<float>(i) * 6.2831853f / kSegments;
                const glm::vec3 at = origin + glm::vec3(std::cos(angle), std::sin(angle), 0.0f) * radius;
                edge(scene, mesh, previous, at, thickness, color);
                previous = at;
            }
            continue;
        }
        const float cos = std::cos(static_cast<float>(shape.angle));
        const float sin = std::sin(static_cast<float>(shape.angle));
        const float hw = static_cast<float>(shape.half.x);
        const float hl = static_cast<float>(shape.half.y);
        glm::vec3 corners[4];
        const float local[4][2] = {{-hw, -hl}, {hw, -hl}, {hw, hl}, {-hw, hl}};
        for (int i = 0; i < 4; ++i) {
            corners[i] = origin + glm::vec3(local[i][0] * cos - local[i][1] * sin,
                                            local[i][0] * sin + local[i][1] * cos, 0.0f);
        }
        for (int i = 0; i < 4; ++i) edge(scene, mesh, corners[i], corners[(i + 1) % 4], thickness, color);
    }
}

}  // namespace

Camera Viewer::camera(const ModelSet &models, Uint32 width, Uint32 height) const {
    Camera out;
    const std::vector<std::string> &names = models.store.names();
    float extent = 40.0f;
    if (!names.empty()) {
        const std::string &name = names[static_cast<size_t>(
            std::max(0, std::min(static_cast<int>(names.size()) - 1, selected)))];
        extent = model_extent(models.store.meta(name));
    }
    out.half_height = extent * 1.6f * distance;
    out.aspect = height > 0 ? static_cast<float>(width) / static_cast<float>(height) : 1.0f;
    const glm::vec3 forward(std::cos(pitch) * std::sin(yaw), std::cos(pitch) * std::cos(yaw),
                            std::sin(pitch));
    out.target = glm::vec3(0.0f);
    // D-11/E5: the eye distance is derived from the framing the wheel asks for, so the zoom is real
    // and the turntable keeps the model the same size in a perspective view.
    out.eye = forward * (out.half_height / std::tan(CAMERA_FOV_Y * 0.5f));
    return out;
}

void update_viewer(Viewer &viewer, const Input &input, const ModelSet &models) {
    const std::vector<std::string> &names = models.store.names();
    if (names.empty()) return;
    const int count = static_cast<int>(names.size());
    // D-15: a reload can shrink the manifest, so the clamp runs every frame, not on a dead flag.
    viewer.selected = std::clamp(viewer.selected, 0, count - 1);

    if (input.wheel != 0.0f) {
        viewer.distance =
            std::clamp(viewer.distance * (1.0f - input.wheel * 0.08f), 0.25f, 4.0f);
    }
    if (input.left_pressed()) viewer.dragging = true;
    if (!input.left) viewer.dragging = false;
    if (viewer.dragging && input.pointer_valid) {
        const glm::vec2 delta = input.pointer - viewer.pointer;
        viewer.yaw -= delta.x * 0.01f;
        viewer.pitch = std::clamp(viewer.pitch + delta.y * 0.008f, -1.2f, 1.35f);
    }
    viewer.pointer = input.pointer;
    if (input.pressed(SDL_SCANCODE_G)) viewer.grid = !viewer.grid;
    if (input.pressed(SDL_SCANCODE_N)) viewer.normals = !viewer.normals;
    if (input.pressed(SDL_SCANCODE_B)) viewer.boxes = !viewer.boxes;
    if (input.pressed(SDL_SCANCODE_E)) viewer.effects = !viewer.effects;
}

void build_viewer_scene(SceneBuilder &scene, const ModelSet &models, const Viewer &viewer) {
    const std::vector<std::string> &names = models.store.names();
    if (names.empty()) return;
    const int index = std::max(0, std::min(static_cast<int>(names.size()) - 1, viewer.selected));
    const std::string &name = names[static_cast<size_t>(index)];
    const Model &model = models.store.model(name);
    const ModelMeta &meta = models.store.meta(name);
    const int cube = models.star_mesh;

    // The model itself, at the origin, unscaled: what the exporter wrote is what you see. The
    // E toggle lights every effect, so all four jets show at full authority here.
    scene.add_model(model, glm::vec3(0.0f), glm::quat(1, 0, 0, 0), 1.0f, viewer.effects, 1.0f,
                    glm::vec3(1.0f), glm::vec4(viewer.effects ? 1.0f : 0.0f));

    if (viewer.grid) {
        const float span = std::max(20.0f, model_extent(meta) * 2.0f);
        const int half = static_cast<int>(span / 10.0f);
        for (int i = -half; i <= half; ++i) {
            const float at = static_cast<float>(i) * 10.0f;
            const bool axis = i == 0;
            const glm::vec3 color = axis ? glm::vec3(0.45f, 0.55f, 0.62f) : glm::vec3(0.22f, 0.27f, 0.31f);
            scene.add(cube, glm::vec3(at, 0.0f, -40.0f), glm::quat(1, 0, 0, 0),
                      glm::vec3(0.12f, 80.0f, 0.12f), color);
            scene.add(cube, glm::vec3(0.0f, at, -40.0f), glm::quat(1, 0, 0, 0),
                      glm::vec3(80.0f, 0.12f, 0.12f), color);
        }
    }

    if (viewer.normals) {
        const glm::vec3 color(0.55f, 0.85f, 0.95f);
        int budget = 4000;  // enough to show the shading, bounded so the viewer stays a viewer
        for (const MeshPart &part : model.parts) {
            const MeshData &mesh = models.library.at(part.mesh);
            const size_t step = std::max<size_t>(1, mesh.vertices.size() / 400);
            for (size_t i = 0; i < mesh.vertices.size() && budget > 0; i += step) {
                const MeshVertex &vertex = mesh.vertices[i];
                const glm::vec3 at = part.pos + part.rot * (vertex.pos * part.scale);
                const glm::vec3 tip = at + (part.rot * vertex.normal) * 2.5f;
                edge(scene, cube, at, tip, 0.08f, color);
                --budget;
            }
        }
    }

    if (viewer.boxes) {
        // The measured AABB, its lit extents, and the shape set the sim actually collides with.
        box_edges(scene, cube, meta.aabb_min, meta.aabb_max, 0.16f, glm::vec3(0.35f, 0.6f, 0.75f));
        box_edges(scene, cube, meta.lit_aabb_min, meta.lit_aabb_max, 0.12f,
                  glm::vec3(0.30f, 0.45f, 0.55f));
        shape_edges(scene, cube, meta, 0.22f, glm::vec3(0.85f, 0.45f, 0.30f));
        for (const auto &hardpoint : meta.hardpoints) {
            scene.add(cube, hardpoint.second, glm::quat(1, 0, 0, 0), glm::vec3(1.2f),
                      glm::vec3(0.9f, 0.8f, 0.35f));
        }
    }
}

void build_viewer_ui(ui::Context &ui, const ui::Rect &screen, const ModelSet &models,
                     Viewer &viewer) {
    const std::vector<std::string> &names = models.store.names();
    ui::push_rect(*ui.batch(), {0.0f, 0.0f}, {screen.w, screen.h},
                 ui::with_alpha(ui::tokens::FIELD, 0.55f));
    ui.push(screen);

    // ---- left: the manifest
    const ui::Rect left = ui.cut_left(320.0f);
    ui.push(ui.inset(left, 18.0f));
    ui.label(ui.cut_top(40.0f), "models", 26.0f, TextAlign::Left, ui::tokens::ETCH);
    ui.label(ui.cut_top(22.0f), "assets/models.json", 12.0f, TextAlign::Left, ui::tokens::ETCH_DIM,
             TextFace::Label);
    ui.cut_top(14.0f);
    std::vector<const char *> items;
    items.reserve(names.size());
    for (const std::string &name : names) items.push_back(name.c_str());
    ui.list("viewer.list", ui.area(), items.data(), static_cast<int>(items.size()),
            viewer.selected);
    ui.pop();

    // ---- right: the sidecar
    const ui::Rect right = ui.cut_right(360.0f);
    ui.push(ui.inset(right, 18.0f));
    if (!names.empty()) {
        const int index = std::max(0, std::min(static_cast<int>(names.size()) - 1, viewer.selected));
        const std::string &name = names[static_cast<size_t>(index)];
        const Model &model = models.store.model(name);
        const ModelMeta &meta = models.store.meta(name);
        std::vector<int> meshes;
        int effects = 0;
        for (const MeshPart &part : model.parts) {
            if (std::find(meshes.begin(), meshes.end(), part.mesh) == meshes.end()) {
                meshes.push_back(part.mesh);
            }
            if (part.effect) ++effects;
        }

        char line[128];
        ui.label(ui.cut_top(40.0f), meta.name.c_str(), 26.0f, TextAlign::Left, ui::tokens::ETCH);
        ui.cut_top(10.0f);
        const auto row = [&](const char *text) {
            ui.label(ui.cut_top(22.0f), text, 15.0f, TextAlign::Left, ui::tokens::ETCH, TextFace::Label);
        };
        std::snprintf(line, sizeof line, "parts %zu   unique meshes %zu", model.parts.size(),
                      meshes.size());
        row(line);
        std::snprintf(line, sizeof line, "triangles %d   vertices %d", meta.triangle_count,
                      meta.vertex_count);
        row(line);
        std::snprintf(line, sizeof line, "effect parts %d  (%zu nodes)", effects,
                      meta.effect_nodes.size());
        row(line);
        ui.cut_top(12.0f);
        std::snprintf(line, sizeof line, "aabb  %.1f %.1f %.1f", static_cast<double>(meta.aabb_min.x),
                      static_cast<double>(meta.aabb_min.y), static_cast<double>(meta.aabb_min.z));
        row(line);
        std::snprintf(line, sizeof line, "      %.1f %.1f %.1f", static_cast<double>(meta.aabb_max.x),
                      static_cast<double>(meta.aabb_max.y), static_cast<double>(meta.aabb_max.z));
        row(line);
        std::snprintf(line, sizeof line, "lit   %.1f .. %.1f  (y)", static_cast<double>(meta.lit_aabb_min.y),
                      static_cast<double>(meta.lit_aabb_max.y));
        row(line);
        std::snprintf(line, sizeof line, "collider %zu shapes  r %.1f", meta.shapes.size(),
                      static_cast<double>(meta.bounds_radius));
        row(line);
        std::snprintf(line, sizeof line, "box      %.1f x %.1f", static_cast<double>(meta.collider.halfLength),
                      static_cast<double>(meta.collider.halfWidth));
        row(line);
        std::snprintf(line, sizeof line, "sim scale x%.2f", static_cast<double>(meta.scale));
        row(line);
        std::snprintf(line, sizeof line, "hardpoints %zu", meta.hardpoints.size());
        row(line);

        ui.cut_top(18.0f);
        ui.section(ui.cut_top(26.0f), "overlays");
        ui.cut_top(6.0f);
        ui.toggle("viewer.grid", ui.cut_top(32.0f), "grid  G", viewer.grid);
        ui.toggle("viewer.normals", ui.cut_top(32.0f), "normals  N", viewer.normals);
        ui.toggle("viewer.boxes", ui.cut_top(32.0f), "bounds  B", viewer.boxes);
        ui.toggle("viewer.effects", ui.cut_top(32.0f), "effect parts  E", viewer.effects);
    }
    ui.pop();

    // ---- footer: the loop this screen exists for
    const ui::Rect bottom = ui.cut_bottom(46.0f);
    ui.label(bottom, "drag to orbit   wheel to zoom   F5 reloads assets/models.json   F2 or Esc "
                     "returns to flight",
             14.0f, TextAlign::Center, ui::tokens::ETCH_DIM, TextFace::Label);
    ui.pop();
}

}  // namespace opra
