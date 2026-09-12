// PBR materials: the glTF metallic-roughness factors and the texture slots that carry them.
//
// One material per library mesh, and the library's mesh keys include the material, so a run of
// instances of one mesh is also a run of one material: the renderer pushes a material when the
// run's mesh changes and never splits a draw. A material that names no texture is not a special
// case - the shader branches on the factor so the map can stay unbound (plan 4.3).
#pragma once

#include <vector>

#include <glm/glm.hpp>

namespace opra {

/** A glTF 2.0 metallic-roughness material. Absent textures are -1. */
struct Material {
    glm::vec4 base_color_factor{1.0f};
    float metallic_factor = 0.0f;
    float roughness_factor = 1.0f;
    int base_color_texture = -1;
    int metallic_roughness_texture = -1;
    int normal_texture = -1;
    /** KHR_materials_unlit: the exporter marks glows and flames with it, and those are drawn by the
     *  additive pipelines, which never read the material at all. */
    bool unlit = false;

    bool operator==(const Material &other) const;
};

/** Deduplicated store of this session's materials, indexed by GpuMesh::material. */
class MaterialTable {
public:
    /** Returns the index of `material`, adding it when it is new. */
    int add(const Material &material);

    int size() const { return static_cast<int>(materials_.size()); }
    const Material &at(int index) const { return materials_[static_cast<size_t>(index)]; }
    const std::vector<Material> &all() const { return materials_; }
    void clear() { materials_.clear(); }

private:
    std::vector<Material> materials_;
};

}  // namespace opra
