// glTF models: the loader for assets/*.glb and the store that owns every loaded model.
// Built by tools/export.mjs; the sidecar JSON carries the scale, AABB and derived collider.
#pragma once

#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

#include <glm/glm.hpp>

#include "render/model.h"
#include "core/units.h"

namespace opra {

/**
 * One collider primitive as the sidecar wrote it, in model units. Deliberately layer-neutral: the
 * render layer may not include sim/, so this mirrors sim::Shape without depending on it, and the
 * one place that knows both layers (game/app.cpp) does the conversion.
 */
struct ModelShape {
    enum class Kind { Box, Circle };
    Kind kind = Kind::Box;
    /** Centre in the model frame: nose +Y, starboard +X. */
    Vec2 pos;
    /** Radians, about the model's z; boxes only. */
    Real angle = 0;
    /** Box half extents: (starboard, forward). */
    Vec2 half;
    /** Circles only. */
    Real radius = 0;
};

/** A docking port as the sidecar wrote it, in model units. Layer-neutral, like ModelShape. */
struct ModelPort {
    std::string id;
    /** Where the corridor starts, in the model frame (nose +Y, starboard +X). */
    Vec2 pos;
    /** Unit, outward from the hull. */
    Vec2 normal{0.0, 1.0};
    char size_class = 'M';
};

/** What the exporter measured about a model, and what the sim needs from it. */
struct ModelMeta {
    std::string name;
    float scale = 1.0f;
    glm::vec3 aabb_min{0.0f};
    glm::vec3 aabb_max{0.0f};
    /** Extents with the effect meshes lit (flames, jets) - what the collision table measures. */
    glm::vec3 lit_aabb_min{0.0f};
    glm::vec3 lit_aabb_max{0.0f};
    HullBoxes collider{};
    /** The exporter's compound collider, in model units. Empty on a legacy sidecar. */
    std::vector<ModelShape> shapes;
    /** Broad phase bound for `shapes`, about the model origin, in model units. */
    Real bounds_radius = 0;
    /** Named docking ports (E9), in model units: hardpoints in the model, so a modular ship
     *  brings its own and nothing needs a separate authoring step. */
    std::vector<ModelPort> ports;
    std::vector<std::pair<std::string, glm::vec3>> hardpoints;
    /** What the exporter counted: meshes, vertices and triangles in the file. */
    int mesh_count = 0;
    int vertex_count = 0;
    int triangle_count = 0;
    /** The name of the effect nodes the sidecar lists. */
    std::vector<std::string> effect_nodes;
};

/** Loads `<path>` into `library` and returns its parts. A malformed file is fatal. */
bool load_gltf(const std::string &path, MeshLibrary &library, Model &out, ModelMeta &meta);

/** Reads the sidecar written beside the .glb. Missing file is fatal, not a default. */
ModelMeta load_sidecar(const std::string &path);

/** Every model named by assets/models.json. Lookup by name; a missing name is fatal. */
class ModelStore {
public:
    /**
     * Loads the manifest and every model it names, adding their meshes to `library`. The library
     * is shared with the procedural meshes so one upload covers every model.
     */
    void load(const std::string &manifest_path, MeshLibrary &library);

    const Model &model(const std::string &name) const;
    const ModelMeta &meta(const std::string &name) const;

    /** True if any .glb, sidecar or the manifest changed on disk since load. */
    bool stale() const;

    /** Re-reads every model into the attached library. */
    void reload();

    const std::string &manifest_path() const { return manifest_path_; }
    const std::vector<std::string> &names() const { return names_; }
    MeshLibrary &library();

private:
    struct Entry {
        std::string name;
        std::string glb;
        std::string sidecar;
        float scale = 1.0f;
    };

    std::string manifest_path_;
    /** Owned by the caller: procedural and glTF meshes share one library and one upload. */
    MeshLibrary *library_ = nullptr;
    std::unordered_map<std::string, Entry> entries_;
    std::unordered_map<std::string, Model> models_;
    std::unordered_map<std::string, ModelMeta> metas_;
    std::vector<std::string> names_;
    std::unordered_map<std::string, std::filesystem::file_time_type> stamps_;
};

/** Ship classes in manifest order: the scene indexes them by ShipClass. */
extern const char *const SHIP_MODEL_NAMES[3];

/** Every model the scene can draw: the procedural meshes plus everything the manifest names. */
struct ModelSet {
    MeshLibrary library;
    ModelStore store;
    int star_mesh = -1;
    /** Backdrop points: a detail-0 icosahedron, so a mote shades like a point of light, not a box. */
    int sky_mesh = -1;
    /** The nebula wash: one soft disc, billboarded and tinted per instance. */
    int nebula_mesh = -1;
    /** A soft additive glow with no locatable rim: the nebula's mote (plan 05 S-1). */
    int glow_mesh = -1;
    /**
     * The body sphere: every sky body - chart glyph, deep pass, planet filling the frame - draws
     * with this and the planet shader's own LOD (plan 4.4). The orrery registers the same key, so
     * both paths share one mesh id.
     */
    int planet_mesh = -1;
    /** Every rock bucket by seed, resolved at build time so no mesh appears after upload. */
    int rock_mesh[13][8] = {};

    /**
     * Builds the procedural meshes and loads the glTF models into the same library. Re-running it
     * starts from an empty library, which is what the hot-reload path needs: mesh ids never go
     * stale because the whole library is replaced and re-uploaded together.
     */
    void build(const std::string &manifest_path);

    /** The shared rock shape and the scale that brings it back to the rock's own radius. */
    int rock_for(float radius, int seed, float &out_scale) const;
};

}  // namespace opra
