// Primitive and procedural geometry builders. The model vocabulary (MeshVertex, MeshData,
// MeshPart, Model, MeshLibrary) lives in render/model.h; this header owns the shapes.
//
// Everything is authored the way the original is: parts in local metres, nose +Y, dorsal +Z.
#pragma once

#include <string>
#include <vector>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include "render/model.h"

namespace opra {

namespace palette {

inline const glm::vec3 ARMOR{0.729f, 0.769f, 0.765f};        // #bac4c3
inline const glm::vec3 LIGHT_ARMOR{0.886f, 0.890f, 0.847f};  // #e2e3d8
inline const glm::vec3 DARK{0.125f, 0.180f, 0.220f};         // #202e38
inline const glm::vec3 METAL{0.400f, 0.463f, 0.506f};        // #667681
inline const glm::vec3 COPPER{0.784f, 0.529f, 0.333f};       // #c88755
inline const glm::vec3 BLACK{0.047f, 0.078f, 0.102f};        // #0c141a
inline const glm::vec3 GLASS{0.216f, 0.424f, 0.486f};        // #376c7c
inline const glm::vec3 TEAL{0.196f, 0.420f, 0.439f};         // #326b70
inline const glm::vec3 OCHRE{0.737f, 0.471f, 0.235f};        // #bc783c
inline const glm::vec3 CERAMIC{0.510f, 0.600f, 0.663f};      // #8299a9
inline const glm::vec3 GLOW{0.639f, 0.914f, 1.000f};         // #a3e9ff
inline const glm::vec3 HOSTILE_PLATE{0.173f, 0.165f, 0.200f};  // #2c2a33
inline const glm::vec3 HOSTILE_TRIM{0.427f, 0.231f, 0.220f};   // #6d3b38
inline const glm::vec3 RUST{0.373f, 0.255f, 0.196f};         // #5f4132
inline const glm::vec3 SCORCH{0.094f, 0.082f, 0.071f};       // #181512
inline const glm::vec3 ORE_SHELL{0.416f, 0.384f, 0.345f};    // #6a6258
inline const glm::vec3 ORE_VEIN{0.937f, 0.722f, 0.475f};     // #efb879
inline const glm::vec3 PANEL{0.125f, 0.239f, 0.333f};        // #203d55
inline const glm::vec3 BEACON_LAMP{0.863f, 0.902f, 0.910f};  // #dce6e8
inline const glm::vec3 BEACON_HALO{0.514f, 0.725f, 0.710f};  // #83b9b5
inline const glm::vec3 WARM_LAMP{0.937f, 0.722f, 0.475f};    // #efb879

}  // namespace palette

namespace meshes {

MeshData cube();
MeshData cylinder(float radius_top, float radius_bottom, float height, int segments);
MeshData cone(float radius, float height, int segments);
MeshData icosahedron(float radius, int detail);
MeshData torus(float major_radius, float tube_radius, int radial_segments, int tubular_segments);
MeshData annulus(float inner_radius, float outer_radius, int segments);
/** A planform outline in the local XY plane, extruded symmetrically along Z. */
MeshData prism(const std::vector<glm::vec2> &outline, float depth);
/** The DRIFT asteroid: icosahedron, sine noise, eight craters, per-vertex tone. */
MeshData asteroid(float radius, int seed);
MeshData ore_chunk();
/** Unit disc in the local XY plane: bright at the centre, black at the rim. Nebula wash. */
MeshData nebula_disc(int segments);

}  // namespace meshes

/** Rock geometry from shared buckets, exactly like the original's cachedAsteroid. */
int asteroid_mesh(float radius, int seed, MeshLibrary &library);

/**
 * Gives every triangle a tangent basis in place: the UV deltas of its corners decide the tangent,
 * and the vertex normal it is Gram-Schmidt orthonormalised against is whatever the caller already
 * has. Writes (1, 0, 0, 1) on a mesh whose UVs are degenerate, which is every procedural mesh and
 * every export that carries no normal map (plan 4.2).
 */
void generate_tangents(MeshData &mesh);

}  // namespace opra
