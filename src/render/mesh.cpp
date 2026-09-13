#include "render/mesh.h"

#include "core/rng.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <map>


namespace opra {
namespace {

constexpr float PI = 3.14159265358979f;

void push_triangle(MeshData &mesh, const glm::vec3 &a, const glm::vec3 &b, const glm::vec3 &c,
                   const glm::vec3 &color) {
    const glm::vec3 normal = glm::normalize(glm::cross(b - a, c - a));
    const uint32_t base = static_cast<uint32_t>(mesh.vertices.size());
    mesh.vertices.push_back({a, normal, color});
    mesh.vertices.push_back({b, normal, color});
    mesh.vertices.push_back({c, normal, color});
    mesh.indices.push_back(base);
    mesh.indices.push_back(base + 1);
    mesh.indices.push_back(base + 2);
}

void push_quad(MeshData &mesh, const glm::vec3 &a, const glm::vec3 &b, const glm::vec3 &c,
               const glm::vec3 &d, const glm::vec3 &color) {
    push_triangle(mesh, a, b, c, color);
    push_triangle(mesh, a, c, d, color);
}

/** Fan of vertices around `centre` on the XY plane, used by the annulus and caps. */
glm::vec3 ring_point(const glm::vec3 &centre, float radius, float angle) {
    return {centre.x + std::cos(angle) * radius, centre.y + std::sin(angle) * radius, centre.z};
}

}  // namespace

namespace meshes {

MeshData cube() {
    MeshData mesh;
    const glm::vec3 h(0.5f);
    const glm::vec3 corners[8] = {{-h.x, -h.y, -h.z}, {h.x, -h.y, -h.z}, {h.x, h.y, -h.z},
                                  {-h.x, h.y, -h.z},  {-h.x, -h.y, h.z},  {h.x, -h.y, h.z},
                                  {h.x, h.y, h.z},    {-h.x, h.y, h.z}};
    const glm::vec3 white(1.0f);
    push_quad(mesh, corners[4], corners[5], corners[6], corners[7], white);  // +Z
    push_quad(mesh, corners[1], corners[0], corners[3], corners[2], white);  // -Z
    push_quad(mesh, corners[0], corners[4], corners[7], corners[3], white);  // -X
    push_quad(mesh, corners[5], corners[1], corners[2], corners[6], white);  // +X
    push_quad(mesh, corners[3], corners[7], corners[6], corners[2], white);  // +Y
    push_quad(mesh, corners[0], corners[1], corners[5], corners[4], white);  // -Y
    return mesh;
}

MeshData cylinder(float radius_top, float radius_bottom, float height, int segments) {
    MeshData mesh;
    const glm::vec3 white(1.0f);
    const float half = height * 0.5f;
    for (int i = 0; i < segments; ++i) {
        const float a0 = 2.0f * PI * static_cast<float>(i) / static_cast<float>(segments);
        const float a1 = 2.0f * PI * static_cast<float>(i + 1) / static_cast<float>(segments);
        const glm::vec3 t0(std::cos(a0) * radius_top, half, std::sin(a0) * radius_top);
        const glm::vec3 t1(std::cos(a1) * radius_top, half, std::sin(a1) * radius_top);
        const glm::vec3 b0(std::cos(a0) * radius_bottom, -half, std::sin(a0) * radius_bottom);
        const glm::vec3 b1(std::cos(a1) * radius_bottom, -half, std::sin(a1) * radius_bottom);
        push_quad(mesh, b0, t0, t1, b1, white);
        // Caps: wound so the derived normal points along the axis, out of the solid.
        push_triangle(mesh, {0.0f, half, 0.0f}, t1, t0, white);
        push_triangle(mesh, {0.0f, -half, 0.0f}, b0, b1, white);
    }
    return mesh;
}

MeshData cone(float radius, float height, int segments) {
    return cylinder(0.0f, radius, height, segments);
}

MeshData icosahedron(float radius, int detail) {
    MeshData mesh;
    const glm::vec3 white(1.0f);
    const float t = (1.0f + std::sqrt(5.0f)) * 0.5f;
    std::vector<glm::vec3> points = {{-1, t, 0}, {1, t, 0},  {-1, -t, 0}, {1, -t, 0},
                                     {0, -1, t}, {0, 1, t},  {0, -1, -t}, {0, 1, -t},
                                     {t, 0, -1}, {t, 0, 1},  {-t, 0, -1}, {-t, 0, 1}};
    for (glm::vec3 &point : points) point = glm::normalize(point);

    std::vector<uint32_t> faces = {0, 11, 5, 0, 5, 1,  0, 1, 7,  0, 7, 10, 0, 10, 11,
                                   1, 5, 9,  5, 11, 4, 11, 10, 2, 10, 7, 6, 7, 1, 8,
                                   3, 9, 4,  3, 4, 2,  3, 2, 6,  3, 6, 8,  3, 8, 9,
                                   4, 9, 5,  2, 4, 11, 6, 2, 10, 8, 1, 9, 6, 10, 7, 8, 6, 3};

    for (int step = 0; step < detail; ++step) {
        std::vector<uint32_t> refined;
        std::map<std::pair<uint32_t, uint32_t>, uint32_t> midpoints;
        auto midpoint = [&](uint32_t a, uint32_t b) {
            const std::pair<uint32_t, uint32_t> key = std::minmax(a, b);
            auto found = midpoints.find(key);
            if (found != midpoints.end()) return found->second;
            const glm::vec3 mid = glm::normalize((points[a] + points[b]) * 0.5f);
            points.push_back(mid);
            const uint32_t index = static_cast<uint32_t>(points.size() - 1);
            midpoints[key] = index;
            return index;
        };
        for (size_t i = 0; i < faces.size(); i += 3) {
            const uint32_t a = faces[i], b = faces[i + 1], c = faces[i + 2];
            const uint32_t ab = midpoint(a, b), bc = midpoint(b, c), ca = midpoint(c, a);
            refined.insert(refined.end(), {a, ab, ca, b, bc, ab, c, ca, bc, ab, bc, ca});
        }
        faces = std::move(refined);
    }

    for (size_t i = 0; i < faces.size(); i += 3) {
        push_triangle(mesh, points[faces[i]] * radius, points[faces[i + 1]] * radius,
                      points[faces[i + 2]] * radius, white);
    }
    return mesh;
}

MeshData torus(float major_radius, float tube_radius, int radial_segments, int tubular_segments) {
    MeshData mesh;
    const glm::vec3 white(1.0f);
    auto point = [&](int i, int j) {
        const float u = 2.0f * PI * static_cast<float>(i) / static_cast<float>(radial_segments);
        const float v = 2.0f * PI * static_cast<float>(j) / static_cast<float>(tubular_segments);
        const float ring = major_radius + tube_radius * std::cos(v);
        return glm::vec3(ring * std::cos(u), ring * std::sin(u), tube_radius * std::sin(v));
    };
    for (int i = 0; i < radial_segments; ++i) {
        for (int j = 0; j < tubular_segments; ++j) {
            push_quad(mesh, point(i, j), point(i + 1, j), point(i + 1, j + 1), point(i, j + 1), white);
        }
    }
    return mesh;
}

MeshData annulus(float inner_radius, float outer_radius, int segments) {
    MeshData mesh;
    const glm::vec3 white(1.0f);
    for (int i = 0; i < segments; ++i) {
        const float a0 = 2.0f * PI * static_cast<float>(i) / static_cast<float>(segments);
        const float a1 = 2.0f * PI * static_cast<float>(i + 1) / static_cast<float>(segments);
        push_quad(mesh, ring_point(glm::vec3(0.0f), inner_radius, a0),
                  ring_point(glm::vec3(0.0f), outer_radius, a0),
                  ring_point(glm::vec3(0.0f), outer_radius, a1),
                  ring_point(glm::vec3(0.0f), inner_radius, a1), white);
    }
    return mesh;
}

MeshData prism(const std::vector<glm::vec2> &outline_in, float depth) {
    MeshData mesh;
    const glm::vec3 white(1.0f);
    const float half = depth * 0.5f;
    if (outline_in.size() < 3) return mesh;

    // three.js Shape outlines come in either winding; normalise to counter-clockwise so the
    // derived face normals point out of the solid.
    float signed_area = 0.0f;
    for (size_t i = 0; i < outline_in.size(); ++i) {
        const glm::vec2 &a = outline_in[i];
        const glm::vec2 &b = outline_in[(i + 1) % outline_in.size()];
        signed_area += a.x * b.y - b.x * a.y;
    }
    std::vector<glm::vec2> outline = outline_in;
    if (signed_area < 0.0f) std::reverse(outline.begin(), outline.end());
    const size_t count = outline.size();

    // Extruded walls.
    for (size_t i = 0; i < count; ++i) {
        const glm::vec2 a = outline[i];
        const glm::vec2 b = outline[(i + 1) % count];
        push_quad(mesh, {a.x, a.y, -half}, {b.x, b.y, -half}, {b.x, b.y, half}, {a.x, a.y, half},
                  white);
    }
    // Caps, fan-triangulated from the centroid: the outlines are all convex enough for it.
    glm::vec2 centre(0.0f);
    for (const glm::vec2 &point : outline) centre += point;
    centre /= static_cast<float>(count);
    for (size_t i = 0; i < count; ++i) {
        const glm::vec2 a = outline[i];
        const glm::vec2 b = outline[(i + 1) % count];
        push_triangle(mesh, {centre.x, centre.y, half}, {a.x, a.y, half}, {b.x, b.y, half}, white);
        push_triangle(mesh, {centre.x, centre.y, -half}, {b.x, b.y, -half}, {a.x, a.y, -half}, white);
    }
    return mesh;
}

MeshData asteroid(float radius, int seed, int tile_index) {
    const MeshData base = icosahedron(radius, radius > 46.0f ? 3 : 2);
    MeshData mesh = base;

    // The original deletes normals and re-derives them after displacing: colours carry the tone,
    // so the light does not have to.
    Rng rand(static_cast<unsigned int>(seed));
    const float scale_x = 0.8f + static_cast<float>(rand.next()) * 0.4f;
    const float scale_y = 0.75f + static_cast<float>(rand.next()) * 0.35f;

    struct Crater {
        glm::vec3 dir;
        float size;
    };
    Crater craters[8];
    for (Crater &crater : craters) {
        glm::vec3 dir(static_cast<float>(rand.next() - 0.5), static_cast<float>(rand.next() - 0.5),
                      static_cast<float>(rand.next() - 0.5));
        crater.dir = glm::normalize(dir);
        crater.size = 0.16f + static_cast<float>(rand.next()) * 0.3f;
    }

    for (MeshVertex &vertex : mesh.vertices) {
        const glm::vec3 p = glm::normalize(vertex.pos);
        const float s = static_cast<float>(seed);
        const float noise = std::sin(p.x * 11.0f + s) * std::sin(p.y * 13.0f - s) *
                                std::sin(p.z * 12.0f) * 0.07f +
                            std::sin(p.x * 4.0f + 2.0f) * std::sin(p.y * 5.0f) *
                                std::cos(p.z * 3.0f + s) * 0.13f;
        float crater_depth = 0.0f;
        for (const Crater &crater : craters) {
            const float d = glm::distance(p, crater.dir) / crater.size;
            if (d < 1.0f) crater_depth -= (1.0f - d * d) * 0.11f;
            if (d > 0.85f && d < 1.2f) {
                crater_depth += std::sin((d - 0.85f) / 0.35f * PI) * 0.035f;
            }
        }
        const float r = radius * (1.0f + noise + crater_depth);
        const float tone = 0.52f + noise * 0.7f + crater_depth * 0.8f;
        vertex.pos = glm::vec3(p.x * r * scale_x, p.y * r * scale_y, p.z * r * 0.78f);
        vertex.color = glm::vec3(tone * 0.97f, tone * 0.97f, tone * 0.99f);
    }
    // Faces were emitted as flat triangles, so the normals follow the displaced surface directly.
    for (size_t i = 0; i < mesh.indices.size(); i += 3) {
        MeshVertex &a = mesh.vertices[mesh.indices[i]];
        MeshVertex &b = mesh.vertices[mesh.indices[i + 1]];
        MeshVertex &c = mesh.vertices[mesh.indices[i + 2]];
        const glm::vec3 normal = glm::normalize(glm::cross(b.pos - a.pos, c.pos - a.pos));
        a.normal = normal;
        b.normal = normal;
        c.normal = normal;
    }
    // The rock tiles (plan-04 H9): one of the four variants per seed, wrapped triplanar over the
    // displaced surface. The vertex tone stays, so the craters keep their shading under the tile.
    mesh.material.base_color_texture = tile_index;
    mesh.material.triplanar = true;
    return mesh;
}

MeshData ore_chunk() {
    MeshData mesh = icosahedron(7.0f, 0);
    for (size_t i = 0; i < mesh.vertices.size(); ++i) {
        // The original's hash-per-vertex jitter, which is what makes the chunk read as ore.
        const float hash = std::sin(static_cast<float>(i) * 12.9898f) * 43758.5453f;
        const float unit = (hash - std::floor(hash)) * 0.5f + 0.72f;
        mesh.vertices[i].pos *= unit;
        mesh.vertices[i].pos.z *= 0.7f;
    }
    for (size_t i = 0; i < mesh.indices.size(); i += 3) {
        MeshVertex &a = mesh.vertices[mesh.indices[i]];
        MeshVertex &b = mesh.vertices[mesh.indices[i + 1]];
        MeshVertex &c = mesh.vertices[mesh.indices[i + 2]];
        const glm::vec3 normal = glm::normalize(glm::cross(b.pos - a.pos, c.pos - a.pos));
        a.normal = normal;
        b.normal = normal;
        c.normal = normal;
    }
    return mesh;
}

MeshData nebula_disc(int segments) {
    MeshData mesh;
    const glm::vec3 normal(0.0f, 0.0f, 1.0f);
    // The whole falloff lives in the vertex colours: the wash needs no texture and no blend state,
    // it just goes dark at the edge over a near-black field.
    mesh.vertices.push_back({{0.0f, 0.0f, 0.0f}, normal, glm::vec3(1.0f)});
    const float ring_radius[2] = {0.42f, 1.0f};
    const float ring_tone[2] = {0.78f, 0.0f};
    for (int ring = 0; ring < 2; ++ring) {
        for (int i = 0; i <= segments; ++i) {
            const float angle = 2.0f * PI * static_cast<float>(i) / static_cast<float>(segments);
            // Radial noise on the rim so the cloud has an edge rather than a compass circle.
            const float noise = 0.86f + 0.14f * std::sin(static_cast<float>(i) * 12.9898f);
            const float radius = ring_radius[ring] * (ring == 1 ? noise : 1.0f);
            mesh.vertices.push_back({{std::cos(angle) * radius, std::sin(angle) * radius, 0.0f},
                                     normal, glm::vec3(ring_tone[ring])});
        }
    }
    const uint32_t inner = 1;
    const uint32_t outer = inner + static_cast<uint32_t>(segments) + 1;
    for (int i = 0; i < segments; ++i) {
        const uint32_t a = static_cast<uint32_t>(i);
        const uint32_t b = a + 1;
        mesh.indices.insert(mesh.indices.end(),
                            {0, inner + a, inner + b, inner + a, outer + a, outer + b, inner + a,
                             outer + b, inner + b});
    }
    return mesh;
}

MeshData glow_disc(int segments) {
    // A soft additive light: vertex colour falls from one at the centre to zero at the rim across
    // three rings, so over a near-black field an additive draw of this mesh has no locatable edge
    // at all - plan 05 S-1, the property the single big nebula billboard lost. Half extent 0.5, so
    // an instance scale is the mote's full width.
    MeshData mesh;
    const glm::vec3 normal(0.0f, 0.0f, 1.0f);
    mesh.vertices.push_back({{0.0f, 0.0f, 0.0f}, normal, glm::vec3(1.0f)});
    const float ring_radius[3] = {0.18f, 0.46f, 1.0f};
    const float ring_tone[3] = {0.86f, 0.38f, 0.0f};
    uint32_t previous = 0;
    for (int ring = 0; ring < 3; ++ring) {
        const uint32_t first = static_cast<uint32_t>(mesh.vertices.size());
        for (int i = 0; i <= segments; ++i) {
            const float angle = 2.0f * PI * static_cast<float>(i) / static_cast<float>(segments);
            mesh.vertices.push_back({{std::cos(angle) * ring_radius[ring] * 0.5f,
                                      std::sin(angle) * ring_radius[ring] * 0.5f, 0.0f},
                                     normal, glm::vec3(ring_tone[ring])});
        }
        if (ring == 0) {
            // The innermost ring fans from the centre vertex: chord triangles between consecutive
            // ring points would tile only the boundary and leave the centre open - the donut the
            // first export of this mesh wore.
            for (int i = 0; i < segments; ++i) {
                const uint32_t c = previous;
                const uint32_t p0 = first + static_cast<uint32_t>(i);
                const uint32_t p1 = first + static_cast<uint32_t>(i) + 1;
                mesh.indices.insert(mesh.indices.end(), {c, p0, p1, c, p1, p0});
            }
        } else {
            for (int i = 0; i < segments; ++i) {
                const uint32_t a0 = previous + static_cast<uint32_t>(i);
                const uint32_t a1 = previous + static_cast<uint32_t>(i) + 1;
                const uint32_t b0 = first + static_cast<uint32_t>(i);
                const uint32_t b1 = first + static_cast<uint32_t>(i) + 1;
                mesh.indices.insert(mesh.indices.end(), {a0, b0, b1, a0, b1, a1});
            }
        }
        previous = first;
    }
    return mesh;
}

}  // namespace meshes

void generate_tangents(MeshData &mesh) {
    const glm::vec4 fallback(1.0f, 0.0f, 0.0f, 1.0f);
    for (MeshVertex &vertex : mesh.vertices) vertex.tangent = glm::vec4(0.0f);
    bool any = false;
    const auto accumulate = [&](uint32_t ia, uint32_t ib, uint32_t ic) {
        const MeshVertex &a = mesh.vertices[ia];
        const MeshVertex &b = mesh.vertices[ib];
        const MeshVertex &c = mesh.vertices[ic];
        const glm::vec3 edge1 = b.pos - a.pos;
        const glm::vec3 edge2 = c.pos - a.pos;
        const glm::vec2 duv1 = b.uv - a.uv;
        const glm::vec2 duv2 = c.uv - a.uv;
        const float det = duv1.x * duv2.y - duv2.x * duv1.y;
        if (std::abs(det) < 1e-12f) return;  // a degenerate UV triangle has no basis to give
        const float r = 1.0f / det;
        const glm::vec3 tangent = (edge1 * duv2.y - edge2 * duv1.y) * r;
        const glm::vec3 bitangent = (edge2 * duv1.x - edge1 * duv2.x) * r;
        for (uint32_t index : {ia, ib, ic}) {
            mesh.vertices[index].tangent += glm::vec4(tangent, 0.0f);
        }
        // The handedness is per triangle; the last one wins, and a mesh whose UVs flip on one face
        // is a mesh that needs a mirrored UV channel, not a guess here.
        mesh.vertices[ic].tangent.w = glm::dot(glm::cross(a.normal, tangent), bitangent) < 0.0f
                                          ? -1.0f
                                          : 1.0f;
        any = true;
    };
    for (size_t i = 0; i + 2 < mesh.indices.size(); i += 3) {
        accumulate(mesh.indices[i], mesh.indices[i + 1], mesh.indices[i + 2]);
    }
    if (!any) {
        for (MeshVertex &vertex : mesh.vertices) vertex.tangent = fallback;
        return;
    }
    for (MeshVertex &vertex : mesh.vertices) {
        const glm::vec3 normal = vertex.normal;
        glm::vec3 tangent(vertex.tangent);
        // Gram-Schmidt against the normal, then normalise: the tangent the shader builds its frame
        // from has to be exactly perpendicular, or the normal map tilts the surface.
        tangent -= normal * glm::dot(normal, tangent);
        const float length = glm::length(tangent);
        if (length < 1e-6f) {
            vertex.tangent = fallback;
            continue;
        }
        const float sign = vertex.tangent.w < 0.0f ? -1.0f : 1.0f;
        vertex.tangent = glm::vec4(tangent / length, sign);
    }
}

/** Rock geometry from shared buckets, exactly like the original's cachedAsteroid. */
int asteroid_mesh(float radius, int seed, MeshLibrary &library) {
    // Twelve radius buckets by eight seeds: at most 96 shapes, all reused.
    const int bucket = std::max(1, static_cast<int>(std::lround(radius / 8.0f)));
    const int rock_seed = seed % 8;
    // The tile variant rides the seed: carbonaceous, ore-vein, regolith and silicate rotate
    // through the field (plan-04 H9), so a mining run reads four rock kinds, not one.
    const int tile = rock_seed % 4;
    const float bucket_radius = static_cast<float>(bucket) * 8.0f;
    char key[48];
    std::snprintf(key, sizeof key, "rock:%d:%d", bucket, rock_seed);
    return library.get(key, [bucket_radius, rock_seed, tile]() {
        return meshes::asteroid(bucket_radius, rock_seed, tile);
    });
}

}  // namespace opra
