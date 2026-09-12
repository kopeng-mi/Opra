// Model vocabulary: the vertex format, a primitive mesh, a model built from placed parts, and
// the library that owns them. Primitives and the procedural builders live in render/mesh.h.
#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include "render/material.h"

namespace opra {

/**
 * 60 bytes: position, normal, tangent with the handedness glTF puts in w, one UV and the baked
 * tone. The tangent is what a normal map needs; a mesh with no normal map carries the default
 * (1, 0, 0, 1) and the shader never reads it (plan 4.2).
 */
struct MeshVertex {
    glm::vec3 pos{0.0f};
    glm::vec3 normal{0.0f, 0.0f, 1.0f};
    glm::vec4 tangent{1.0f, 0.0f, 0.0f, 1.0f};
    glm::vec2 uv{0.0f};
    glm::vec3 color{1.0f};

    MeshVertex() = default;
    /** The procedural builders' form: a surface point, its normal and its tone. */
    MeshVertex(const glm::vec3 &at, const glm::vec3 &up, const glm::vec3 &tone)
        : pos(at), normal(up), color(tone) {}
};

struct MeshData {
    std::vector<MeshVertex> vertices;
    std::vector<uint32_t> indices;
    /** The surface the geometry is made of. One per mesh, and part of the library key. */
    Material material;
};

/** One instance of a library mesh in a model's local space. */
struct MeshPart {
    int mesh = -1;
    glm::vec3 pos{0.0f};
    glm::quat rot{1.0f, 0.0f, 0.0f, 0.0f};
    glm::vec3 scale{1.0f};
    glm::vec3 color{1.0f};
    /** Effect geometry (engine flame): drawn only while the system is firing. */
    bool effect = false;
    /**
     * Which RCS corner jet this effect cone belongs to, as (starboard bit) | (fore bit), or -1 for
     * anything else. The four cones differ only in where the exporter mounted them, so the loader
     * resolves the corner once and the scene never has to guess from geometry.
     */
    int jet = -1;
};

struct Model {
    std::vector<MeshPart> parts;
};

/** Named mesh store; identical keys are built once. */
class MeshLibrary {
public:
    /** Returns the id of `key`, building it with `build` on first use. */
    int get(const std::string &key, const std::function<MeshData()> &build);

    int size() const { return static_cast<int>(meshes_.size()); }
    const MeshData &at(int id) const { return meshes_[static_cast<size_t>(id)]; }

private:
    std::vector<MeshData> meshes_;
    std::vector<std::string> keys_;
};

/** Per-instance transform, shared by every mesh in the library. */
struct Instance {
    glm::vec4 pos;    // xyz world position
    glm::vec4 rot;    // quaternion xyzw
    glm::vec4 scale;  // xyz half extents
    glm::vec4 color;
};

/** Where one library mesh lives inside the merged vertex/index buffers. */
struct GpuMesh {
    uint32_t first_index = 0;
    uint32_t index_count = 0;
    int32_t vertex_offset = 0;
    /** Index into Renderer::materials. The library keys on the material, so this is fixed. */
    int material = 0;
};

/** Every instance of one mesh, contiguous in the instance buffer. */
struct InstanceRun {
    int mesh = -1;
    uint32_t first_instance = 0;
    uint32_t count = 0;
};

}  // namespace opra
