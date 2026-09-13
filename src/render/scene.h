// Scene assembly: the instance list and its runs. Placing the world's objects is game/scene.h's
// job: this layer only knows models, instances and the camera.
#pragma once

#include <cstdint>
#include <vector>

#include <glm/glm.hpp>

#include "render/gltf.h"
#include "render/model.h"

namespace opra {

/** Which pass an instance belongs to. The backdrop is light, not matter: additive, no depth write. */
enum class InstanceLayer : uint8_t { Opaque = 0, Effect = 1, Backdrop = 2 };

/**
 * The frame's key light: the star the system orbits. The game layer fills it from the ship's real
 * position in the system (plan 4.3) - the render layer only ever sees a direction and a colour.
 * `direction_to_star` is a unit direction in the render frame, which is also the world's: a
 * direction does not need the render origin subtracted.
 */
struct SceneLight {
    glm::vec3 direction_to_star{0.0f, 0.0f, 1.0f};
    glm::vec3 color{1.0f};
    float intensity = 1.0f;
};

/** Which shader a sky body is drawn with. A body is a sphere plus a shader, never a textured mesh. */
enum class BodyKind : uint8_t { Planet = 0, Star = 1 };

/**
 * One celestial body as the scene declares it. `radius` is the *drawn* radius in render-frame
 * units: the chart draws a glyph floor, a true-scale chart and the flight view draw the real size.
 * The renderer picks the LOD from the angular size that comes out of it (plan 4.4).
 */
struct SkyBody {
    int mesh = -1;
    glm::vec3 center{0.0f};
    float radius = 1.0f;
    glm::vec3 color{1.0f};
    /** The terrain seed and its amplitude over the radius: the same numbers sim/terrain.cpp uses,
     *  so the silhouette seen from orbit is the ground the collider samples (plan 4.4). */
    float terrain_seed = 0.0f;
    float terrain_amplitude = 0.0f;
    /** Atmosphere scale height over the radius, and the top of the air; 0 is an airless body. */
    float scale_height = 0.0f;
    float atmosphere_top = 0.0f;
    /**
     * The map names this body draws with, from the system file through the orrery frame
     * (plan-04 s3.4). The renderer resolves them against assets/textures' manifest; empty keeps
     * the procedural shading.
     */
    std::string albedo_map;
    std::string cloud_map;
    std::string night_map;
    std::string photosphere_map;
    BodyKind kind = BodyKind::Planet;
    /**
     * Beyond the far plane (plan 05 s2.3): the scene builder has already re-projected this body
     * onto a shell just inside it, exactly, and the renderer draws the deep set in its own pass
     * before the near pass clears depth.
     */
    bool deep = false;
};

/** Collects instances, remembering which mesh each one belongs to. */
struct SceneBuilder {
    std::vector<Instance> instances;
    std::vector<int> mesh_ids;
    /** Per instance: the pass it is drawn in, which is also the draw order. */
    std::vector<uint8_t> layer;
    /** Objects the visible volume rejected this frame; diagnostics only. */
    int culled = 0;

    /** The star, as the game layer sets it. Defaulted so a scene with no light still draws. */
    SceneLight light;
    /** Celestial bodies, drawn by the planet and star shaders before the mesh runs. */
    std::vector<SkyBody> bodies;

    void add(int mesh, const glm::vec3 &position, const glm::quat &rotation,
             const glm::vec3 &scale, const glm::vec3 &color,
             InstanceLayer placement = InstanceLayer::Opaque);

    /**
     * Places a whole model: every part is transformed by the model's pose and tinted. `rcs_jets` is
     * the four per-jet authorities in MeshPart::jet order, so a cone is drawn only while its own jet
     * fires; the default draws none. (starboard-fore, port-fore, starboard-aft, port-aft)
     */
    void add_model(const Model &model, const glm::vec3 &origin, const glm::quat &rotation,
                   float scale, bool effects, float thrust, const glm::vec3 &tint = glm::vec3(1.0f),
                   const glm::vec4 &rcs_jets = glm::vec4(0.0f));

    /**
     * Drops this frame's instances, keeping the capacity for the next one. The LOD hysteresis
     * memory lives with the app - it is view state that outlives a frame.
     */
    void clear();

    /** True when no instance was placed this frame - the empty scene the renderer still draws. */
    bool empty() const { return instances.empty() && bodies.empty(); }

    /**
     * Groups the instance list by (layer, mesh) so each mesh is one indexed draw, in pass order:
     * opaque, then effects, then the backdrop. Counting sort over the library's id space: O(n), no
     * comparisons, no allocation.
     */
    void sorted(std::vector<Instance> &out, std::vector<InstanceRun> &runs,
                std::vector<InstanceRun> &effect_runs, std::vector<InstanceRun> &backdrop_runs,
                int mesh_count) const;

private:
    /** Reused by the counting sort so a frame never allocates. */
    mutable std::vector<uint32_t> scratch_counts_;
    mutable std::vector<uint32_t> scratch_next_;
};

glm::quat spin_about_z(float angle);

}  // namespace opra
