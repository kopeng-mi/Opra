#include "render/gltf.h"

#include "render/mesh.h"

#include <algorithm>
#include <cmath>

namespace opra {

int MeshLibrary::get(const std::string &key, const std::function<MeshData()> &build) {
    for (size_t i = 0; i < keys_.size(); ++i) {
        if (keys_[i] == key) return static_cast<int>(i);
    }
    meshes_.push_back(build());
    keys_.push_back(key);
    return static_cast<int>(meshes_.size() - 1);
}

const char *const SHIP_MODEL_NAMES[3] = {"kestrel", "mule", "needle"};

void ModelSet::build(const std::string &manifest_path) {
    // From scratch: the hot-reload path calls this again with the same library object.
    library = MeshLibrary{};
    store = ModelStore{};
    star_mesh = library.get("star", []() { return meshes::cube(); });
    sky_mesh = library.get("sky", []() { return meshes::icosahedron(0.5f, 0); });
    nebula_mesh = library.get("nebula", []() { return meshes::nebula_disc(48); });
    glow_mesh = library.get("glow", []() { return meshes::glow_disc(24); });
    // The flight view's sky bodies draw with the same sphere the orrery's chart does: one key, so
    // whichever builder runs first owns the mesh and the other reuses it.
    planet_mesh = library.get("planet", []() { return meshes::icosahedron(1.0f, 4); });
    // Every rock bucket the field can ask for, resolved before the upload so no mesh is created
    // after the GPU buffers exist.
    for (int bucket = 1; bucket <= 12; ++bucket) {
        for (int seed = 0; seed < 8; ++seed) {
            rock_mesh[bucket][seed] = asteroid_mesh(static_cast<float>(bucket) * 8.0f, seed, library);
        }
    }
    store.load(manifest_path, library);
}

int ModelSet::rock_for(float radius, int seed, float &out_scale) const {
    const int bucket = std::max(1, std::min(12, static_cast<int>(std::lround(radius / 8.0f))));
    const int rock_seed = ((seed % 8) + 8) % 8;
    out_scale = radius / (static_cast<float>(bucket) * 8.0f);
    return rock_mesh[bucket][rock_seed];
}

}  // namespace opra
