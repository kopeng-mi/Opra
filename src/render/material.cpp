#include "render/material.h"

namespace opra {

bool Material::operator==(const Material &other) const {
    return base_color_factor == other.base_color_factor &&
           metallic_factor == other.metallic_factor &&
           roughness_factor == other.roughness_factor &&
           base_color_texture == other.base_color_texture &&
           metallic_roughness_texture == other.metallic_roughness_texture &&
           normal_texture == other.normal_texture && unlit == other.unlit;
}

int MaterialTable::add(const Material &material) {
    for (size_t i = 0; i < materials_.size(); ++i) {
        if (materials_[i] == material) return static_cast<int>(i);
    }
    materials_.push_back(material);
    return static_cast<int>(materials_.size()) - 1;
}

}  // namespace opra
