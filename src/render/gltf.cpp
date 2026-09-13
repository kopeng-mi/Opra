// The CRT's own headers, before cgltf.h pulls them in and marks fopen/strncpy deprecated inside the
// single-header library: the /W4 build has no business reporting a vendored file's calls. GLM and
// nlohmann are clean; cgltf is not.
#define _CRT_SECURE_NO_WARNINGS

#include "render/gltf.h"

#include <algorithm>
#include <cstring>
#include <fstream>

#define CGLTF_IMPLEMENTATION  // header-only library: exactly one TU provides the symbols
#include <cgltf.h>
#define GLM_ENABLE_EXPERIMENTAL  // glm::decompose lives in gtx/
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/matrix_decompose.hpp>
#include <nlohmann/json.hpp>

#include "core/file.h"
#include "core/log.h"
#include "render/mesh.h"

namespace opra {
namespace {

using json = nlohmann::json;

/** FNV-1a over the geometry bytes: identical primitives across models share one library slot. */
uint64_t hash_bytes(const void *data, size_t bytes, uint64_t seed = 1469598103934665603ull) {
    const auto *p = static_cast<const unsigned char *>(data);
    uint64_t hash = seed;
    for (size_t i = 0; i < bytes; ++i) {
        hash ^= p[i];
        hash *= 1099511628211ull;
    }
    return hash;
}

/**
 * The surface is part of the mesh's identity: a run of instances is bound to one material, so two
 * primitives with the same geometry and different materials have to be two library meshes. Hashed
 * as the values themselves, not the struct's bytes, which carry padding.
 */
uint64_t hash_material(const Material &material, uint64_t seed) {
    const float values[9] = {material.base_color_factor.x, material.base_color_factor.y,
                             material.base_color_factor.z, material.base_color_factor.w,
                             material.metallic_factor,        material.roughness_factor,
                             static_cast<float>(material.base_color_texture),
                             static_cast<float>(material.metallic_roughness_texture),
                             static_cast<float>(material.normal_texture)};
    uint64_t hash = hash_bytes(values, sizeof values, seed);
    const unsigned char flags = static_cast<unsigned char>(material.unlit ? 1 : 0);
    return hash_bytes(&flags, 1, hash);
}

std::string mesh_key(const MeshData &mesh, uint64_t &hash) {
    hash = hash_bytes(mesh.vertices.data(), mesh.vertices.size() * sizeof(MeshVertex));
    hash = hash_bytes(mesh.indices.data(), mesh.indices.size() * sizeof(uint32_t), hash);
    hash = hash_material(mesh.material, hash);
    char key[32];
    std::snprintf(key, sizeof key, "gltf:%016llx", static_cast<unsigned long long>(hash));
    return key;
}

glm::mat4 node_matrix(const cgltf_node *node) {
    float raw[16];
    cgltf_node_transform_local(node, raw);
    // cgltf writes column-major, which is what glm expects.
    return glm::make_mat4(raw);
}

/** The material a primitive is drawn with, in the render layer's own terms. */
Material read_material(const cgltf_data *data, const cgltf_material *material) {
    Material out;
    if (!material) return out;
    const cgltf_pbr_metallic_roughness &pbr = material->pbr_metallic_roughness;
    out.base_color_factor = glm::vec4(pbr.base_color_factor[0], pbr.base_color_factor[1],
                                      pbr.base_color_factor[2], pbr.base_color_factor[3]);
    out.metallic_factor = pbr.metallic_factor;
    out.roughness_factor = pbr.roughness_factor;
    const auto texture_index = [data](const cgltf_texture_view &view) {
        if (!view.texture || !data->textures) return -1;
        return static_cast<int>(view.texture - data->textures);
    };
    out.base_color_texture = texture_index(pbr.base_color_texture);
    out.metallic_roughness_texture = texture_index(pbr.metallic_roughness_texture);
    out.normal_texture = texture_index(material->normal_texture);
    out.unlit = material->unlit != 0;
    return out;
}

/**
 * Reads POSITION / NORMAL / TANGENT / TEXCOORD_0 / COLOR_0 and the indices. Returns false if the
 * primitive is unusable.
 */
bool read_primitive(const cgltf_primitive &primitive, MeshData &out) {
    const cgltf_accessor *position = nullptr, *normal = nullptr, *color = nullptr;
    const cgltf_accessor *tangent = nullptr, *uv = nullptr;
    for (cgltf_size i = 0; i < primitive.attributes_count; ++i) {
        const cgltf_attribute &attribute = primitive.attributes[i];
        if (attribute.type == cgltf_attribute_type_position) position = attribute.data;
        if (attribute.type == cgltf_attribute_type_normal) normal = attribute.data;
        if (attribute.type == cgltf_attribute_type_tangent) tangent = attribute.data;
        if (attribute.type == cgltf_attribute_type_texcoord && uv == nullptr) uv = attribute.data;
        if (attribute.type == cgltf_attribute_type_color && color == nullptr) color = attribute.data;
    }
    if (!position || position->count == 0) return false;

    const size_t count = position->count;
    out.vertices.resize(count);
    for (size_t i = 0; i < count; ++i) {
        float p[3] = {0, 0, 0};
        cgltf_accessor_read_float(position, i, p, 3);
        MeshVertex &vertex = out.vertices[i];
        vertex.pos = glm::vec3(p[0], p[1], p[2]);
        vertex.color = glm::vec3(1.0f);
        if (normal) {
            float n[3] = {0, 0, 0};
            cgltf_accessor_read_float(normal, i, n, 3);
            vertex.normal = glm::vec3(n[0], n[1], n[2]);
        } else {
            vertex.normal = glm::vec3(0.0f);
        }
        if (tangent) {
            float t[4] = {1, 0, 0, 1};
            cgltf_accessor_read_float(tangent, i, t, 4);
            vertex.tangent = glm::vec4(t[0], t[1], t[2], t[3]);
        }
        if (uv) {
            float t[2] = {0, 0};
            cgltf_accessor_read_float(uv, i, t, 2);
            vertex.uv = glm::vec2(t[0], t[1]);
        }
        if (color) {
            float c[4] = {1, 1, 1, 1};
            cgltf_accessor_read_float(color, i, c, 4);
            vertex.color = glm::vec3(c[0], c[1], c[2]);
        }
    }

    if (primitive.indices) {
        out.indices.resize(primitive.indices->count);
        for (size_t i = 0; i < primitive.indices->count; ++i) {
            out.indices[i] = static_cast<uint32_t>(cgltf_accessor_read_index(primitive.indices, i));
        }
    } else {
        out.indices.resize(count);
        for (size_t i = 0; i < count; ++i) out.indices[i] = static_cast<uint32_t>(i);
    }

    // No normals in the file: derive flat face normals rather than leaving them zero.
    if (!normal) {
        for (size_t i = 0; i + 2 < out.indices.size(); i += 3) {
            MeshVertex &a = out.vertices[out.indices[i]];
            MeshVertex &b = out.vertices[out.indices[i + 1]];
            MeshVertex &c = out.vertices[out.indices[i + 2]];
            const glm::vec3 face = glm::normalize(glm::cross(b.pos - a.pos, c.pos - a.pos));
            a.normal = b.normal = c.normal = face;
        }
    }
    // glTF only guarantees tangents when a normal map is already assigned, so a file that carries
    // UVs but no TANGENT gets its basis from the UV deltas.
    if (!tangent) generate_tangents(out);
    return true;
}

/** True when the node or its mesh asks to be treated as an effect (flames, jets, glow). */
bool node_is_effect(const cgltf_node *node) {
    const char *texts[2] = {node->extras.data, node->mesh ? node->mesh->extras.data : nullptr};
    for (const char *text : texts) {
        if (!text) continue;
        const char *found = std::strstr(text, "\"effect\"");
        if (found && std::strstr(found, "true")) return true;
    }
    return false;
}

/** Manifest paths are asset-root relative; absolute paths are taken as they come. */
std::string resolve(const std::string &path) {
    return std::filesystem::path(path).is_absolute() ? path : asset_path(path);
}

bool matrix_is_trs(const glm::mat4 &matrix) {
    glm::vec3 scale(1.0f), translation(1.0f), skew(0.0f);
    glm::vec4 perspective(0.0f);
    glm::quat rotation(1.0f, 0.0f, 0.0f, 0.0f);
    if (!glm::decompose(matrix, scale, rotation, translation, skew, perspective)) return false;
    const glm::mat4 recomposed =
        glm::translate(glm::mat4(1.0f), translation) * glm::mat4_cast(rotation) *
        glm::scale(glm::mat4(1.0f), scale);
    for (int column = 0; column < 4; ++column) {
        for (int row = 0; row < 4; ++row) {
            if (std::abs(recomposed[column][row] - matrix[column][row]) > 1e-4f) return false;
        }
    }
    return true;
}

void walk(const cgltf_data *data, const cgltf_node *node, const glm::mat4 &parent,
          MeshLibrary &library, Model &out, int &skipped) {
    const glm::mat4 matrix = parent * node_matrix(node);
    if (node->mesh) {
        const bool effect = node_is_effect(node);
        for (cgltf_size i = 0; i < node->mesh->primitives_count; ++i) {
            const cgltf_primitive &primitive = node->mesh->primitives[i];
            if (primitive.type != cgltf_primitive_type_triangles) {
                SDL_Log("gltf: skipping non-triangle primitive in node %s",
                        node->name ? node->name : "(unnamed)");
                ++skipped;
                continue;
            }
            MeshData mesh;
            if (!read_primitive(primitive, mesh)) {
                ++skipped;
                continue;
            }
            mesh.material = read_material(data, primitive.material);

            MeshPart part;
            part.effect = effect;
            // Effect meshes are authored with a three.js ShaderMaterial, which glTF cannot carry, so
            // they arrive unlit white. The additive pass expects the legacy flame tint instead, and
            // the mesh's own material is never read there: the flame pass is emissive - genuinely
            // HDR, ~1.6 at the core, so the bloom chain's tightest mip carries it and the widest
            // mips do not (plan 05 S-1: a halo, not a wash).
            part.color = effect ? glm::vec3(0.99f, 1.42f, 1.55f) : glm::vec3(1.0f);

            if (matrix_is_trs(matrix)) {
                glm::vec3 scale(1.0f), translation(1.0f), skew(0.0f);
                glm::vec4 perspective(0.0f);
                glm::quat rotation(1.0f, 0.0f, 0.0f, 0.0f);
                glm::decompose(matrix, scale, rotation, translation, skew, perspective);
                part.pos = translation;
                part.rot = rotation;
                part.scale = scale;
            } else {
                // pos+quat+scale in the shader cannot represent shear, so bake it into the mesh.
                const glm::mat3 linear(matrix);
                const bool mirrored = glm::determinant(linear) < 0.0f;
                for (MeshVertex &vertex : mesh.vertices) {
                    const glm::vec4 world = matrix * glm::vec4(vertex.pos, 1.0f);
                    vertex.pos = glm::vec3(world);
                    vertex.normal = glm::normalize(linear * vertex.normal);
                    const glm::vec3 tangent = linear * glm::vec3(vertex.tangent);
                    const float length = glm::length(tangent);
                    if (length > 1e-6f) {
                        vertex.tangent =
                            glm::vec4(tangent / length,
                                      mirrored ? -vertex.tangent.w : vertex.tangent.w);
                    }
                }
            }

            uint64_t hash = 0;
            const std::string key = mesh_key(mesh, hash);
            part.mesh = library.get(key, [&mesh]() { return mesh; });
            // The exporter names all four RCS cones `rcs-jet` and mounts them at the hull's four
            // corners; the flames keep the centreline. The corner is the part's local position.
            if (node->name && std::strncmp(node->name, "rcs-jet", 7) == 0) {
                part.jet = (part.pos.x < 0.0f ? 1 : 0) + (part.pos.y < 0.0f ? 2 : 0);
            }
            out.parts.push_back(part);
        }
    }
    for (cgltf_size i = 0; i < node->children_count; ++i) {
        walk(data, node->children[i], matrix, library, out, skipped);
    }
}

/** Pulls the hardpoints (nodes named hp.<id>) out of the scene graph, in model space. */
void collect_hardpoints(const cgltf_node *node, const glm::mat4 &parent, ModelMeta &meta) {
    const glm::mat4 matrix = parent * node_matrix(node);
    if (node->name && std::strncmp(node->name, "hp.", 3) == 0) {
        glm::vec3 scale(1.0f), translation(1.0f), skew(0.0f);
        glm::vec4 perspective(0.0f);
        glm::quat rotation(1.0f, 0.0f, 0.0f, 0.0f);
        glm::decompose(matrix, scale, rotation, translation, skew, perspective);
        meta.hardpoints.emplace_back(node->name + 3, translation);
    }
    for (cgltf_size i = 0; i < node->children_count; ++i) {
        collect_hardpoints(node->children[i], matrix, meta);
    }
}

}  // namespace

bool load_gltf(const std::string &path, MeshLibrary &library, Model &out, ModelMeta &meta) {
    cgltf_options options{};
    cgltf_data *data = nullptr;
    if (cgltf_parse_file(&options, path.c_str(), &data) != cgltf_result_success || !data) {
        fatal(("cgltf_parse_file " + path).c_str());
    }
    if (cgltf_load_buffers(&options, data, path.c_str()) != cgltf_result_success) {
        cgltf_free(data);
        fatal(("cgltf_load_buffers " + path).c_str());
    }
    if (cgltf_validate(data) != cgltf_result_success) {
        cgltf_free(data);
        fatal(("cgltf_validate " + path).c_str());
    }

    int skipped = 0;
    const glm::mat4 identity(1.0f);
    for (cgltf_size i = 0; i < data->scenes_count; ++i) {
        const cgltf_scene &scene = data->scenes[i];
        for (cgltf_size n = 0; n < scene.nodes_count; ++n) {
            walk(data, scene.nodes[n], identity, library, out, skipped);
            collect_hardpoints(scene.nodes[n], identity, meta);
        }
    }
    cgltf_free(data);
    if (skipped > 0) SDL_Log("gltf: %s skipped %d primitive(s)", path.c_str(), skipped);
    return !out.parts.empty();
}

ModelMeta load_sidecar(const std::string &path) {
    ModelMeta meta;
    std::ifstream file(path);
    if (!file) fatal(("sidecar missing: " + path).c_str());
    json sidecar;
    try {
        file >> sidecar;
    } catch (const std::exception &error) {
        fatal(("sidecar parse " + path + ": " + error.what()).c_str());
    }

    meta.name = sidecar.value("name", std::string{});
    meta.scale = sidecar.value("scale", 1.0f);
    if (sidecar.contains("aabb")) {
        const json &aabb = sidecar["aabb"];
        meta.aabb_min = glm::vec3(aabb["min"][0].get<float>(), aabb["min"][1].get<float>(),
                                  aabb["min"][2].get<float>());
        meta.aabb_max = glm::vec3(aabb["max"][0].get<float>(), aabb["max"][1].get<float>(),
                                  aabb["max"][2].get<float>());
    }
    // The lit extents: what the authored collision table was measured against.
    if (sidecar.contains("aabbWithEffects")) {
        const json &aabb = sidecar["aabbWithEffects"];
        meta.lit_aabb_min = glm::vec3(aabb["min"][0].get<float>(), aabb["min"][1].get<float>(),
                                      aabb["min"][2].get<float>());
        meta.lit_aabb_max = glm::vec3(aabb["max"][0].get<float>(), aabb["max"][1].get<float>(),
                                      aabb["max"][2].get<float>());
    } else {
        meta.lit_aabb_min = meta.aabb_min;
        meta.lit_aabb_max = meta.aabb_max;
    }
    // The exported AABB, not the sidecar's `collider` block: that number is an inherited constant
    // measured with the effect meshes lit, and it overhangs the drawn hull by ~11% (D-19).
    meta.collider.halfLength = (meta.aabb_max.y - meta.aabb_min.y) * 0.5;
    meta.collider.halfWidth = (meta.aabb_max.x - meta.aabb_min.x) * 0.5;
    // P4: the fitted shape set, straight from the sidecar and in model units. No shapes means a
    // legacy sidecar, and the caller falls back to `collider`.
    if (sidecar.contains("collider")) {
        const json &collider = sidecar["collider"];
        meta.bounds_radius = collider.value("bounds_radius", 0.0);
        if (collider.contains("shapes")) {
            for (const json &entry : collider["shapes"]) {
                ModelShape shape;
                shape.kind = entry.value("kind", std::string{"box"}) == "circle"
                                 ? ModelShape::Kind::Circle
                                 : ModelShape::Kind::Box;
                const json &pos = entry["pos"];
                shape.pos = {pos[0].get<Real>(), pos[1].get<Real>()};
                shape.angle = entry.value("angle", 0.0);
                if (shape.kind == ModelShape::Kind::Box) {
                    const json &half = entry["half"];
                    shape.half = {half[0].get<Real>(), half[1].get<Real>()};
                } else {
                    shape.radius = entry.value("radius", 0.0);
                }
                meta.shapes.push_back(shape);
            }
        }
    }
    if (sidecar.contains("ports")) {
        for (const json &entry : sidecar["ports"]) {
            ModelPort port;
            port.id = entry.value("id", std::string{});
            const json &pos = entry["pos"];
            port.pos = {pos[0].get<Real>(), pos[1].get<Real>()};
            if (entry.contains("normal")) {
                const json &normal = entry["normal"];
                port.normal = {normal[0].get<Real>(), normal[1].get<Real>()};
            }
            const std::string size = entry.value("class", std::string{"M"});
            port.size_class = size.empty() ? 'M' : size[0];
            if (!port.id.empty()) meta.ports.push_back(port);
        }
    }
    if (sidecar.contains("counts")) {
        const json &counts = sidecar["counts"];
        meta.mesh_count = counts.value("meshes", 0);
        meta.vertex_count = counts.value("vertices", 0);
        meta.triangle_count = counts.value("triangles", 0);
    }
    if (sidecar.contains("effects")) {
        for (const json &entry : sidecar["effects"]) {
            meta.effect_nodes.push_back(entry.value("node", std::string{}));
        }
    }
    if (sidecar.contains("hardpoints")) {
        for (auto &[id, entry] : sidecar["hardpoints"].items()) {
            const json &pos = entry["pos"];
            meta.hardpoints.emplace_back(id, glm::vec3(pos[0].get<float>(), pos[1].get<float>(),
                                                       pos[2].get<float>()));
        }
    }
    return meta;
}

void ModelStore::load(const std::string &manifest_path, MeshLibrary &library) {
    manifest_path_ = manifest_path;
    library_ = &library;
    std::ifstream file(manifest_path);
    if (!file) fatal(("manifest missing: " + manifest_path).c_str());
    json manifest;
    try {
        file >> manifest;
    } catch (const std::exception &error) {
        fatal(("manifest parse " + manifest_path + ": " + error.what()).c_str());
    }

    for (const json &entry : manifest["models"]) {
        Entry item;
        item.name = entry.value("name", std::string{});
        item.glb = resolve(entry.value("glb", std::string{}));
        item.sidecar = resolve(entry.value("sidecar", std::string{}));
        item.scale = entry.value("scale", 1.0f);
        if (item.name.empty() || item.glb.empty()) fatal("manifest entry without name or glb");
        entries_[item.name] = item;
        names_.push_back(item.name);
    }
    reload();
}

MeshLibrary &ModelStore::library() {
    if (!library_) fatal("ModelStore used before load()");
    return *library_;
}

void ModelStore::reload() {
    models_.clear();
    metas_.clear();
    for (const std::string &name : names_) {
        const Entry &entry = entries_.at(name);
        Model model;
        ModelMeta meta = load_sidecar(entry.sidecar);
        if (meta.name.empty()) meta.name = name;
        meta.scale = entry.scale;
        if (!load_gltf(entry.glb, library(), model, meta)) {
            fatal(("gltf has no drawable parts: " + entry.glb).c_str());
        }
        models_[name] = std::move(model);
        metas_[name] = std::move(meta);
    }
    stamps_.clear();
    for (const std::string &name : names_) {
        const Entry &entry = entries_.at(name);
        for (const std::string *path : {&entry.glb, &entry.sidecar}) {
            std::error_code error;
            const auto stamp = std::filesystem::last_write_time(*path, error);
            if (!error) stamps_[*path] = stamp;
        }
    }
    const auto manifest_stamp = std::filesystem::last_write_time(manifest_path_);
    stamps_[manifest_path_] = manifest_stamp;
}

bool ModelStore::stale() const {
    for (const auto &[path, stamp] : stamps_) {
        std::error_code error;
        const auto current = std::filesystem::last_write_time(path, error);
        if (error || current != stamp) return true;
    }
    return false;
}

const Model &ModelStore::model(const std::string &name) const {
    auto it = models_.find(name);
    if (it == models_.end()) fatal(("model not loaded: " + name).c_str());
    return it->second;
}

const ModelMeta &ModelStore::meta(const std::string &name) const {
    auto it = metas_.find(name);
    if (it == metas_.end()) fatal(("model meta not loaded: " + name).c_str());
    return it->second;
}

}  // namespace opra
