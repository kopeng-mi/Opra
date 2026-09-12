#include "render/scene.h"

#include <algorithm>

namespace opra {

void SceneBuilder::add(int mesh, const glm::vec3 &position, const glm::quat &rotation,
                       const glm::vec3 &scale, const glm::vec3 &color, InstanceLayer placement) {
    Instance instance;
    instance.pos = glm::vec4(position, 1.0f);
    instance.rot = glm::vec4(rotation.x, rotation.y, rotation.z, rotation.w);
    instance.scale = glm::vec4(scale, 1.0f);
    instance.color = glm::vec4(color, 1.0f);
    instances.push_back(instance);
    mesh_ids.push_back(mesh);
    layer.push_back(static_cast<uint8_t>(placement));
}

void SceneBuilder::add_model(const Model &model, const glm::vec3 &origin, const glm::quat &rotation,
                             float scale, bool effects, float thrust, const glm::vec3 &tint,
                             const glm::vec4 &rcs_jets) {
    for (const MeshPart &part : model.parts) {
        float stretch = 1.0f;
        if (part.jet >= 0) {
            // The hull's jet vector is in the ship frame and the cone is already posed, so the
            // authority is a scale on the part, applied here rather than in the transform.
            const float authority = rcs_jets[part.jet];
            if (authority <= 0.02f) continue;  // a jet that is not firing is not drawn
            stretch = 0.35f + 0.85f * authority;
        } else if (part.effect) {
            if (!effects) continue;
            stretch = 0.35f + 0.85f * thrust;
        }
        // A flame stretches along its own axis; a jet cone is rotated so that axis is lateral.
        glm::vec3 part_scale = part.scale * scale;
        part_scale.y *= stretch;
        add(part.mesh, origin + rotation * (part.pos * scale), rotation * part.rot, part_scale,
            part.color * tint,
            part.effect ? InstanceLayer::Effect : InstanceLayer::Opaque);
    }
}

void SceneBuilder::clear() {
    instances.clear();
    mesh_ids.clear();
    layer.clear();
    bodies.clear();
}

/**
 * Counting sort by (layer, mesh id): the id space is the library size times three layers, so this
 * is O(n) with no comparisons and no allocation beyond the reusable scratch.
 */
void SceneBuilder::sorted(std::vector<Instance> &out, std::vector<InstanceRun> &runs,
                          std::vector<InstanceRun> &effect_runs,
                          std::vector<InstanceRun> &backdrop_runs, int mesh_count) const {
    const size_t count = instances.size();
    out.resize(count);
    runs.clear();
    effect_runs.clear();
    backdrop_runs.clear();
    if (count == 0 || mesh_count <= 0) return;

    const int layer_count = 3;
    const int keys = mesh_count * layer_count;
    scratch_counts_.assign(static_cast<size_t>(keys) + 1, 0);
    const auto key_of = [&](size_t i) {
        return static_cast<int>(layer[i]) * mesh_count + mesh_ids[i];
    };
    for (size_t i = 0; i < count; ++i) ++scratch_counts_[static_cast<size_t>(key_of(i)) + 1];
    for (int k = 0; k < keys; ++k) {
        scratch_counts_[static_cast<size_t>(k) + 1] += scratch_counts_[static_cast<size_t>(k)];
    }
    scratch_next_.assign(scratch_counts_.begin(), scratch_counts_.end() - 1);
    for (size_t i = 0; i < count; ++i) {
        out[scratch_next_[static_cast<size_t>(key_of(i))]++] = instances[i];
    }

    for (int k = 0; k < keys; ++k) {
        const uint32_t first = scratch_counts_[static_cast<size_t>(k)];
        const uint32_t last = scratch_counts_[static_cast<size_t>(k) + 1];
        if (last == first) continue;
        const int placement = k / mesh_count;
        const InstanceRun run{k - placement * mesh_count, first, last - first};
        if (placement == static_cast<int>(InstanceLayer::Effect)) {
            effect_runs.push_back(run);
        } else if (placement == static_cast<int>(InstanceLayer::Backdrop)) {
            backdrop_runs.push_back(run);
        } else {
            runs.push_back(run);
        }
    }
}

glm::quat spin_about_z(float angle) { return glm::quat(glm::vec3(0.0f, 0.0f, angle)); }

}  // namespace opra
