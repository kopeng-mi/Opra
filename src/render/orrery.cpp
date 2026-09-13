#include "render/orrery.h"

#include <algorithm>
#include <cmath>

#include "core/rng.h"
#include "orbit/conic.h"
#include "render/mesh.h"

namespace opra::orrery {
namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr double kTwoPi = 2.0 * kPi;

/** The one place a chart position becomes a float: divided in double, cast once. */
glm::vec3 to_scene(const glm::dvec2 &at, double metres_per_unit) {
    return {static_cast<float>(at.x / metres_per_unit),
            static_cast<float>(at.y / metres_per_unit), 0.0f};
}

glm::quat about_z(double radians) { return spin_about_z(static_cast<float>(radians)); }

const glm::quat kNoRotation{1.0f, 0.0f, 0.0f, 0.0f};

/** True when the body rides a closed conic about a primary of its own: the star rides nothing. */
bool has_ring(const orbit::Elements &elements) {
    return elements.a > 0.0 && orbit::is_elliptic(elements);
}

}  // namespace

Meshes add_meshes(MeshLibrary &library) {
    Meshes out;
    out.box = library.get("orrery.box", []() { return meshes::cube(); });
    out.rock = library.get("orrery.rock", []() { return meshes::icosahedron(1.0f, 0); });
    out.ship = library.get("orrery.ship", []() { return meshes::cone(1.0f, 2.0f, 12); });
    // The bodies draw with a sphere dense enough for a shader to shade: 5120 triangles, which is
    // the LOD the plan's sphere_lod1 asks for and nothing like the chart's glyph. The key matches
    // ModelSet's, so the flight view's deep pass and the chart share one mesh.
    out.planet = library.get("planet", []() { return meshes::icosahedron(1.0f, 4); });
    return out;
}

double field_radius(const Frame &frame) {
    double field = 0.0;
    const auto reach = [&field](double at, double extra) {
        field = std::max(field, at + extra);
    };
    for (const Body &body : frame.bodies) {
        const double apoapsis = has_ring(body.elements) ? orbit::apoapsis(body.elements) : 0.0;
        reach(glm::length(body.primary), apoapsis);
        reach(glm::length(body.position), body.radius);
    }
    reach(glm::length(frame.belt_primary), frame.belt_outer);
    reach(glm::length(frame.ship_position), 0.0);
    return field;
}

double metres_per_unit(double field_radius) {
    return field_radius > 0.0 ? field_radius / FIELD_UNITS : 1.0;
}

double glyph_radius(double body_radius, double field_radius, bool true_scale) {
    if (true_scale) return body_radius;
    return std::max(body_radius, field_radius * GLYPH_FLOOR);
}

double ring_sagitta(double radius, int segments) {
    if (radius <= 0.0 || segments < 3) return 0.0;
    return radius * (1.0 - std::cos(kPi / static_cast<double>(segments)));
}

void sample_ring(const orbit::Elements &elements, int segments, std::vector<glm::dvec2> &out) {
    orbit::Conic conic;
    conic.elements = elements;
    conic.sample(segments, out);
}

void sample_belt(double inner, double outer, const glm::dvec2 &centre, int count, unsigned seed,
                 std::vector<glm::dvec2> &out) {
    out.clear();
    if (count <= 0 || !(outer > inner)) return;
    out.reserve(static_cast<size_t>(count));
    Rng rng(seed);
    for (int i = 0; i < count; ++i) {
        const double angle = rng.next() * kTwoPi;
        const double radius = inner + rng.next() * (outer - inner);
        out.push_back(centre + glm::dvec2(std::cos(angle) * radius, std::sin(angle) * radius));
    }
}

void build(SceneBuilder &scene, const Meshes &meshes, const Frame &frame) {
    const double field = field_radius(frame);
    if (field <= 0.0) return;
    const double scale = metres_per_unit(field);
    // Ink weights are scene units, not metres: a chart's line weight does not shrink with the
    // system it describes.
    const float units = static_cast<float>(field / scale);  // == FIELD_UNITS
    const float hairline = units * 0.002f;
    const float tick_length = units * 0.012f;
    const float rock_size = units * 0.002f;
    const float ship_size = units * 0.012f;

    std::vector<glm::dvec2> points;
    for (const Body &body : frame.bodies) {
        if (has_ring(body.elements)) {
            sample_ring(body.elements, RING_SEGMENTS, points);
            const size_t count = points.size();
            for (size_t i = 0; i < count; ++i) {
                const glm::dvec2 &from = points[i];
                const glm::dvec2 &to = points[(i + 1) % count];
                const glm::dvec2 along = to - from;
                const double chord = glm::length(along);
                if (chord <= 0.0) continue;
                // 6% overshoot: the bars have to overlap at the joint, because the ring *is* this
                // polyline and a gap at every vertex would read as a dashed orbit.
                scene.add(meshes.box, to_scene((from + to) * 0.5 + body.primary, scale),
                          about_z(std::atan2(along.y, along.x)),
                          {static_cast<float>(chord / scale) * 1.06f, hairline, hairline},
                          body.color, InstanceLayer::Backdrop);
            }
            // True-anomaly ticks: sample k * count / RING_TICKS is nu = k * 30 degrees exactly,
            // because the sampling is uniform in true anomaly.
            for (int k = 0; k < RING_TICKS; ++k) {
                const glm::dvec2 rel = points[static_cast<size_t>(k) * count / RING_TICKS];
                const double radius = glm::length(rel);
                if (radius <= 0.0) continue;
                const glm::dvec2 outward = rel / radius;
                const double outward_metres = static_cast<double>(tick_length) * scale * 0.5;
                scene.add(meshes.box,
                          to_scene(rel + body.primary + outward * outward_metres, scale),
                          about_z(std::atan2(outward.y, outward.x)),
                          {tick_length, hairline, hairline}, body.color, InstanceLayer::Backdrop);
            }
        }
        // A body with no orbit of its own is the primary: the star, and the one warm mark. Every
        // body is a sphere plus a shader; the chart's own glyph floor still decides how big it
        // draws, so the map keeps its deliberate oversize and the true-scale chart keeps its honesty.
        const bool star = !has_ring(body.elements);
        const double drawn = glyph_radius(body.radius, field, frame.true_scale);
        SkyBody sky;
        sky.mesh = meshes.planet;
        sky.center = to_scene(body.position, scale);
        sky.radius = static_cast<float>(drawn / scale);
        sky.color = star ? ink::STAR : body.color;
        sky.terrain_seed = body.terrain_seed;
        sky.terrain_amplitude = body.terrain_amplitude;
        sky.scale_height = body.scale_height;
        sky.atmosphere_top = body.atmosphere_top;
        sky.albedo_map = body.albedo_map;
        sky.cloud_map = body.cloud_map;
        sky.night_map = body.night_map;
        sky.photosphere_map = body.photosphere_map;
        sky.kind = star ? BodyKind::Star : BodyKind::Planet;
        scene.bodies.push_back(sky);
    }

    if (frame.belt_count > 0 && frame.belt_outer > frame.belt_inner) {
        const int count = std::min(frame.belt_count, MAX_BELT_ROCKS);
        sample_belt(frame.belt_inner, frame.belt_outer, frame.belt_primary, count, frame.belt_seed,
                    points);
        const glm::vec3 rock_ink = ink::VELLUM * ink::DORMANT;
        for (const glm::dvec2 &at : points) {
            scene.add(meshes.rock, to_scene(at, scale), kNoRotation,
                      {rock_size, rock_size, rock_size}, rock_ink, InstanceLayer::Backdrop);
        }
    }

    scene.add(meshes.ship, to_scene(frame.ship_position, scale), about_z(frame.ship_heading),
              {ship_size, ship_size, ship_size}, ink::ETCH, InstanceLayer::Backdrop);

    // One polyline drawer serves the transfer and every predicted leg: a list of points, the weight
    // of the ink, and whether the line is dashed. A dashed line is an intention or a later leg; the
    // solid one is where the ship is going next.
    const auto draw_polyline = [&](const std::vector<glm::dvec2> &at, bool closed,
                                   const glm::vec3 &ink_color, float weight, bool dashed,
                                   const glm::dvec2 &offset) {
        const size_t count = at.size();
        if (count < 2) return;
        const size_t last = closed ? count : count - 1;
        const size_t step = dashed ? 2 : 1;
        for (size_t i = 0; i < last; i += step) {
            const glm::dvec2 &from = at[i];
            const glm::dvec2 &to = at[(i + 1) % count];
            const glm::dvec2 along = to - from;
            const double chord = glm::length(along);
            if (chord <= 0.0) continue;
            // 6% overshoot, as the rings do: the bars have to overlap at the joint.
            scene.add(meshes.box, to_scene((from + to) * 0.5 + offset, scale),
                      about_z(std::atan2(along.y, along.x)),
                      {static_cast<float>(chord / scale) * 1.06f, weight, weight}, ink_color,
                      InstanceLayer::Backdrop);
        }
    };

    if (frame.has_transfer && frame.transfer.a != 0.0) {
        sample_ring(frame.transfer, RING_SEGMENTS, points);
        // An ellipse closes; a hyperbola is an open branch and must not be joined end to end.
        draw_polyline(points, orbit::is_elliptic(frame.transfer), ink::DRIVE, hairline * 2.0f, true,
                      glm::dvec2(0.0));
    }

    // Predicted legs, each in the frame of the body it is drawn about, plus the body's sphere of
    // influence: a boundary the ship crosses, drawn as a dashed circle and not a solid mark.
    for (const PredictedPath &path : frame.predicted) {
        const bool orbits_body = path.body >= 0 && path.body < static_cast<int>(frame.bodies.size());
        const glm::dvec2 centre =
            orbits_body ? frame.bodies[static_cast<size_t>(path.body)].position : glm::dvec2(0.0);
        draw_polyline(path.points, false, path.post ? ink::DRIVE : ink::VELLUM, hairline * 2.0f,
                      path.dashed, centre);
        if (path.soi <= 0.0) continue;
        points.clear();
        for (int k = 0; k < SOI_SEGMENTS; ++k) {
            const double angle = kTwoPi * static_cast<double>(k) / static_cast<double>(SOI_SEGMENTS);
            points.push_back(centre + glm::dvec2(std::cos(angle), std::sin(angle)) * path.soi);
        }
        draw_polyline(points, true, ink::VELLUM * ink::DORMANT, hairline, true, glm::dvec2(0.0));
    }
}

Camera map_camera(const Frame &frame, float width, float height, float centre_x) {
    Camera camera;
    // The chart's own origin is the render origin: this is the one camera whose world positions are
    // already small, so nothing has to be subtracted before the float cast.
    camera.origin = glm::dvec3(0.0);
    const double field = field_radius(frame);
    camera.half_height = static_cast<float>(field / metres_per_unit(field)) * FIELD_MARGIN;
    camera.aspect = height > 0.0f ? width / height : 1.0f;
    // Move the camera right and the drawing slides left: a positive shift centres the field in a
    // pane left of the screen's middle, which is where the almanac leaves room for it.
    const float shift =
        static_cast<float>(2.0 * (0.5 - centre_x) * camera.half_height) * camera.aspect;
    camera.target = glm::vec3(shift, 0.0f, 0.0f);
    camera.eye = camera.target + orbit_eye(camera.half_height, MAP_PITCH);
    return camera;
}

}  // namespace opra::orrery
