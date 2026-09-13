// Assertions for the orrery: the ring's sampling rule and the error it actually produces, the glyph
// floor, true scale, the belt annulus, and the scene the builder emits. Run through
// `Opra.exe --selftest`.
//
// Every expected number below is derived on paper from the rules in PLAN-02 3/4.3/4.4/5.1 and
// written into the comment beside it - nothing here is captured from the run it tests.
#include "render/orrery_tests.h"

#include "selftest.h"

#include <algorithm>
#include <cmath>
#include <vector>

#include "render/orrery.h"

namespace opra {
namespace {

int g_failures = 0;

/** Forwards to the game's harness and counts here: orrery_tests() returns its own failure count. */
void check(bool ok, const char *what) {
    selftest::check(ok, what);
    if (!ok) ++g_failures;
}

void check_close(double actual, double expected, double tolerance, const char *what) {
    selftest::check_close(actual, expected, tolerance, what);
    if (!(std::fabs(actual - expected) <= tolerance)) ++g_failures;
}

constexpr double kPi = 3.14159265358979323846;
constexpr double kTwoPi = 2.0 * kPi;

// PLAN-02 5.1, the system the map draws.
constexpr double kMuNereid = 2.07e19;
constexpr double kStarRadius = 3.4e8;  // Nereid's own disc
constexpr double kTesseraA = 1.720e10;
constexpr double kTesseraRadius = 6.3e6;
constexpr double kHalberdApoapsis = 4.338e10 * (1.0 + 0.091);  // 4.732 758e10 m

double wrap_pi(double angle) { return angle - kTwoPi * std::floor((angle + kPi) / kTwoPi); }

/** An instance position is a float of a scene unit, and at 13..24 units that is about a kilometre:
 *  the only error the emitted position carries, since the division happens in double. */
constexpr double kPlacementSlack = 1.0e4;  // metres

/** A circular body of the chart, in the frame's own coordinates. */
orrery::Body circular_body(const char *name, double a, double radius) {
    orrery::Body body;
    body.name = name;
    body.position = {a, 0.0};
    body.elements.a = a;
    body.elements.e = 0.0;
    body.elements.mu = kMuNereid;
    body.radius = radius;
    return body;
}

/** An instance's position back in metres: the builder divides by one factor, this multiplies it out. */
glm::dvec2 metres_of(const Instance &instance, double scale) {
    return {static_cast<double>(instance.pos.x) * scale,
            static_cast<double>(instance.pos.y) * scale};
}

// ---------------------------------------------------------------------------- rings
void test_ring_sampling() {
    orbit::Elements circle;
    circle.a = kTesseraA;
    circle.e = 0.0;
    circle.mu = kMuNereid;

    std::vector<glm::dvec2> points;
    orrery::sample_ring(circle, orrery::RING_SEGMENTS, points);
    check(points.size() == static_cast<size_t>(orrery::RING_SEGMENTS),
          "ring: one vertex per segment");

    double worst_radius = 0.0;
    double worst_gap = 0.0;
    double worst_sagitta = 0.0;
    for (size_t i = 0; i < points.size(); ++i) {
        const glm::dvec2 &at = points[i];
        const glm::dvec2 &next = points[(i + 1) % points.size()];
        worst_radius = std::max(worst_radius, std::fabs(glm::length(at) - circle.a) / circle.a);
        // The join at the end is the same size as every other step: the ring closes.
        worst_gap = std::max(worst_gap,
                             std::fabs(wrap_pi(std::atan2(next.y, next.x) -
                                               std::atan2(at.y, at.x))));
        // The drawn ring is the chord polyline, so its real error is the sagitta at the midpoints.
        worst_sagitta = std::max(worst_sagitta, circle.a - glm::length((at + next) * 0.5));
    }
    check_close(worst_gap, kTwoPi / orrery::RING_SEGMENTS, 1e-12,
                "ring: the closing step is one segment");
    check(glm::length(points.front() - points.back()) > 0.0, "ring: no duplicate vertex at the join");
    check(worst_radius < 1e-3, "ring: vertex radius error under 0.1%");

    // The sagitta of a chord of a circle: r (1 - cos(pi/192)) = 1.34e-4 r, an order under the 0.1%
    // the plan holds the rings to, and the same at every chord because the sampling is uniform.
    const double expected = circle.a * (1.0 - std::cos(kPi / orrery::RING_SEGMENTS));
    check_close(worst_sagitta, expected, circle.a * 1e-9, "ring: sagitta is r (1 - cos(pi/192))");
    check_close(orrery::ring_sagitta(circle.a, orrery::RING_SEGMENTS), expected, 1e-6,
                "ring: the sagitta rule is the one the samples obey");
    check(worst_sagitta < circle.a * 1e-3, "ring: drawn radius error under 0.1%");

    // An eccentric ring still closes on its own conic: Halberd, a = 4.338e10, e = 0.091.
    orbit::Elements ellipse;
    ellipse.a = 4.338e10;
    ellipse.e = 0.091;
    ellipse.omega = 3.31;
    ellipse.M0 = 3.51;
    ellipse.mu = kMuNereid;
    orrery::sample_ring(ellipse, orrery::RING_SEGMENTS, points);
    double far = 0.0;
    double near_ = 1e30;
    for (const glm::dvec2 &at : points) {
        const double radius = glm::length(at);
        far = std::max(far, radius);
        near_ = std::min(near_, radius);
    }
    check_close(near_, 4.338e10 * (1.0 - 0.091), 4.338e10 * 1e-3, "ring: periapsis is the closest vertex");
    check_close(far, kHalberdApoapsis, 4.338e10 * 1e-3, "ring: apoapsis is the farthest vertex");
}

// ---------------------------------------------------------------------------- glyphs
void test_glyph_floor() {
    // The plan's own example: a 6 300 km planet on a 1.7e10 m orbit. 2.5% of 1.7e10 is 4.25e8 m.
    check_close(orrery::glyph_radius(kTesseraRadius, 1.7e10, false), 4.25e8, 1e-3,
                "glyph: a 6 300 km planet takes the floor");
    // The star's own disc is 3.4e8 m, still inside that floor, so it takes the floor too.
    check_close(orrery::glyph_radius(kStarRadius, 1.7e10, false), 4.25e8, 1e-3,
                "glyph: the star takes the floor");
    // Tessera's own orbit is 1.720e10 m, so the same planet on its real field floors at 4.3e8 m.
    check_close(orrery::glyph_radius(kTesseraRadius, kTesseraA, false), 4.3e8, 1e-3,
                "glyph: the floor follows the field it is drawn on");
    // A body already larger than the floor keeps its own radius: the floor only ever raises.
    check_close(orrery::glyph_radius(2.0e9, kTesseraA, false), 2.0e9, 0.0,
                "glyph: the floor never shrinks a body");
    // True scale draws the honest sizes, however empty that makes the chart.
    check_close(orrery::glyph_radius(kTesseraRadius, kTesseraA, true), kTesseraRadius, 0.0,
                "glyph: true scale draws the real radius");
    check_close(orrery::glyph_radius(kStarRadius, kTesseraA, true), kStarRadius, 0.0,
                "glyph: true scale draws the star's real radius");

    // Every body of Nereid on the map's own field, Halberd's apoapsis 4.732 758e10 m: the floor is
    // 1.183 189 5e9 m and the largest true radius is the star's 3.4e8, so all five take the floor.
    const double field = kHalberdApoapsis;
    const double floor = field * 0.025;
    check_close(floor, 1.1831895e9, 1.0, "glyph: Nereid's floor is 2.5% of the field");
    const double radii[5] = {kStarRadius, 4.1e6, kTesseraRadius, 1.4e6, 8.9e6};
    bool floored = true;
    for (double radius : radii) floored = floored && orrery::glyph_radius(radius, field, false) == floor;
    check(floored, "glyph: every body of Nereid draws at the floor");
}

// ---------------------------------------------------------------------------- belt
void test_belt_annulus() {
    // PLAN-02 5.1: the Drift, 2.46e10 .. 2.89e10 m about Nereid, seed 4712.
    const double inner = 2.46e10;
    const double outer = 2.89e10;
    std::vector<glm::dvec2> rocks;
    std::vector<glm::dvec2> again;
    orrery::sample_belt(inner, outer, glm::dvec2(0.0), 900, 4712u, rocks);
    check(rocks.size() == 900u, "belt: one rock per sample");
    bool inside = true;
    for (const glm::dvec2 &rock : rocks) {
        const double radius = glm::length(rock);
        inside = inside && radius >= inner && radius <= outer;
    }
    check(inside, "belt: every rock is inside inner..outer");

    orrery::sample_belt(inner, outer, glm::dvec2(0.0), 900, 4712u, again);
    bool same = rocks.size() == again.size();
    for (size_t i = 0; same && i < rocks.size(); ++i) same = rocks[i] == again[i];
    check(same, "belt: the seed decides the field, bit for bit");

    orrery::sample_belt(inner, outer, glm::dvec2(0.0), 900, 4713u, again);
    bool differs = false;
    for (size_t i = 0; i < rocks.size(); ++i) differs = differs || rocks[i] != again[i];
    check(differs, "belt: a different seed is a different field");

    orrery::sample_belt(outer, inner, glm::dvec2(0.0), 900, 4712u, again);
    check(again.empty(), "belt: an inverted annulus samples nothing");
}

// ---------------------------------------------------------------------------- the meshes
void test_meshes() {
    MeshLibrary library;
    const orrery::Meshes first = orrery::add_meshes(library);
    const orrery::Meshes second = orrery::add_meshes(library);
    // A second call is a lookup, not a build: the renderer's draw table is built from the library
    // once, so a mesh that appeared after the upload would never be drawn.
    check(library.size() == 4, "meshes: four shapes, registered once");
    check(first.box == second.box && first.planet == second.planet && first.rock == second.rock &&
              first.ship == second.ship,
          "meshes: the same keys resolve to the same ids");
    check(first.box >= 0 && first.planet >= 0 && first.rock >= 0 && first.ship >= 0 &&
              first.box != first.planet && first.box != first.rock && first.box != first.ship &&
              first.planet != first.rock && first.planet != first.ship && first.rock != first.ship,
          "meshes: every mark has its own shape");
}

// ---------------------------------------------------------------------------- the scene
void test_build_marks() {
    orrery::Frame frame;
    orrery::Body star;
    star.name = "Nereid";
    star.radius = kStarRadius;
    frame.bodies.push_back(star);
    frame.bodies.push_back(circular_body("Tessera", kTesseraA, kTesseraRadius));
    frame.belt_inner = 2.46e10;
    frame.belt_outer = 2.89e10;
    frame.belt_count = 300;
    frame.belt_seed = 4712;
    frame.ship_position = {1.0e10, 0.0};
    frame.ship_heading = 1.0;

    // The belt's outer edge is the farthest mark, so it is the field: 2.89e10 m over 40 units.
    const double field = orrery::field_radius(frame);
    check_close(field, 2.89e10, 1.0, "build: the belt is part of the drawn field");
    const double scale = orrery::metres_per_unit(field);
    check_close(scale, 2.89e10 / 40.0, 1.0, "build: the field spans 40 scene units");

    const orrery::Meshes meshes{10, 11, 12, 13};
    SceneBuilder scene;
    orrery::build(scene, meshes, frame);
    check(!scene.instances.empty(), "build: the frame draws something");
    check(scene.instances.size() == scene.layer.size(), "build: one layer per instance");

    bool ink = true;
    int boxes = 0;
    int rocks = 0;
    int ships = 0;
    double ring_near = 1e30;
    double ring_far = 0.0;
    double belt_near = 1e30;
    double belt_far = 0.0;
    bool ship_placed = false;
    bool ship_heading = false;
    for (size_t i = 0; i < scene.instances.size(); ++i) {
        const Instance &instance = scene.instances[i];
        const glm::dvec2 at = metres_of(instance, scale);
        ink = ink && scene.layer[i] == static_cast<uint8_t>(InstanceLayer::Backdrop);
        if (scene.mesh_ids[i] == meshes.box) {
            ++boxes;
            const double radius = glm::length(at);
            ring_near = std::min(ring_near, radius);
            ring_far = std::max(ring_far, radius);
        } else if (scene.mesh_ids[i] == meshes.rock) {
            ++rocks;
            const double radius = glm::length(at);
            belt_near = std::min(belt_near, radius);
            belt_far = std::max(belt_far, radius);
        } else if (scene.mesh_ids[i] == meshes.ship) {
            ++ships;
            ship_placed = glm::length(at - glm::dvec2(1.0e10, 0.0)) < kPlacementSlack;
            const glm::quat rotation(instance.rot.w, instance.rot.x, instance.rot.y, instance.rot.z);
            const glm::vec3 nose = rotation * glm::vec3(0.0f, 1.0f, 0.0f);
            // The flight model's forward is (-sin h, cos h): a mark that points anywhere else is a lie.
            ship_heading = std::fabs(static_cast<double>(nose.x) + std::sin(1.0)) < 1e-5 &&
                           std::fabs(static_cast<double>(nose.y) - std::cos(1.0)) < 1e-5;
        }
    }
    check(ink, "build: the orrery is ink, drawn in the additive pass");
    check(boxes == orrery::RING_SEGMENTS + orrery::RING_TICKS,
          "build: one bar per chord and one tick per 30 degrees");
    // The ring hugs its conic: no mark inside it, and nothing past a tick's own half-length
    // (0.012 of the field, drawn outward from the ring).
    check(ring_near >= kTesseraA * 0.999, "build: no ring mark inside the conic");
    check(ring_far <= kTesseraA * 1.001 + field * 0.012, "build: no ring mark beyond a tick's reach");
    // Bodies are not instances any more: a body is a sphere plus a shader, declared in `bodies`, and
    // the renderer picks the LOD from the angular size that comes out of it (plan 4.4).
    check(scene.bodies.size() == 2, "build: a body for every body, the star included");
    bool body_at_body = false;
    double body_drawn = 0.0;
    for (const SkyBody &sky : scene.bodies) {
        const glm::dvec2 at(static_cast<double>(sky.center.x) * scale,
                            static_cast<double>(sky.center.y) * scale);
        if (glm::length(at - glm::dvec2(kTesseraA, 0.0)) < kPlacementSlack) {
            body_at_body = true;
            body_drawn = static_cast<double>(sky.radius) * scale;
        }
    }
    check(body_at_body, "build: a body sits at the body's own position");
    check_close(body_drawn, field * orrery::GLYPH_FLOOR, 1.0, "build: the chart draws a body at the floor");
    check(rocks == 300, "build: one rock per belt sample");
    check(belt_near >= frame.belt_inner && belt_far <= frame.belt_outer,
          "build: the drawn belt is the annulus it was given");
    check(ships == 1 && ship_placed, "build: one ship mark, at the ship");
    check(ship_heading, "build: the ship mark points along the heading");

    // True scale: the same body is drawn at its real radius, 6.3e6 m, not the floor.
    orrery::Frame honest = frame;
    honest.true_scale = true;
    SceneBuilder true_scene;
    orrery::build(true_scene, meshes, honest);
    double drawn = 0.0;
    bool found = false;
    for (const SkyBody &sky : true_scene.bodies) {
        const glm::dvec2 at(static_cast<double>(sky.center.x) * scale,
                            static_cast<double>(sky.center.y) * scale);
        if (glm::length(at - glm::dvec2(kTesseraA, 0.0)) < kPlacementSlack) {
            found = true;
            drawn = static_cast<double>(sky.radius) * scale;
        }
    }
    check(found, "build: true scale still draws the body");
    check_close(drawn, kTesseraRadius, 1.0, "build: true scale draws the real radius");
}

// ---------------------------------------------------------------------------- camera
void test_map_camera() {
    orrery::Frame frame;
    frame.bodies.push_back(circular_body("Tessera", kTesseraA, kTesseraRadius));
    const double field = orrery::field_radius(frame);
    const Camera camera = orrery::map_camera(frame, 1600.0f, 900.0f, 0.5f);
    const double units = field / orrery::metres_per_unit(field);  // the field in scene units
    check_close(camera.half_height, static_cast<float>(units) * orrery::FIELD_MARGIN, 1e-3,
                "camera: the field fills the frame with a little air");
    check_close(static_cast<double>(camera.aspect), 1600.0 / 900.0, 1e-6, "camera: aspect is the drawable's");
    // A plan view: the eye sits one R above the target, R = half_height / tan(12 degrees), and the
    // half degree off the pole only tips it sideways by 0.0087 R.
    const glm::vec3 offset = camera.eye - camera.target;
    const float radius = static_cast<float>(static_cast<double>(camera.half_height) /
                                             std::tan(static_cast<double>(CAMERA_FOV_Y) * 0.5));
    check(static_cast<double>(offset.z) > static_cast<double>(radius) * 0.9999,
          "camera: the eye is one R above the plane");
    check(std::fabs(offset.y) < radius * 0.01f, "camera: the eye is within a degree of the pole");
    // A pane left of the middle: the camera moves right, so the drawing slides left.
    const Camera shifted = orrery::map_camera(frame, 1600.0f, 900.0f, 0.31f);
    check(shifted.target.x > camera.target.x, "camera: a left-hand pane shifts the camera right");
    check(shifted.eye.x == shifted.target.x, "camera: the pitch offset never moves the eye in x");
}

}  // namespace

int orrery_tests() {
    g_failures = 0;
    test_ring_sampling();
    test_glyph_floor();
    test_belt_annulus();
    test_meshes();
    test_build_marks();
    test_map_camera();
    return g_failures;
}

}  // namespace opra
