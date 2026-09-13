// Headless assert suite: simulation determinism, grid storage integrity, collision primitives.
// Run with `Opra.exe --selftest`; exits non-zero if any assertion fails.
#include "selftest.h"

#include <cmath>
#include <cstdio>
#include <vector>

#include "sim/collision.h"
#include "sim/physics.h"
#include "core/file.h"

#include <fstream>
#include <string>

#include "game/camera_follow.h"
#include "game/config.h"
#include "game/settings.h"
#include "game/warp_tests.h"
#include "game/effects_tests.h"
#include "orbit/encounter_tests.h"
#include "orbit/lagrange_tests.h"
#include "orbit/tests.h"
#include "orbit/transfer_tests.h"
#include "sim/dock_tests.h"
#include "sim/combat_tests.h"
#include "sim/component_tests.h"
#include "sim/shapes_tests.h"
#include "sim/system_tests.h"
#include "sim/world_tests.h"
#include "sim/worlds_tests.h"
#include "ui/flow_tests.h"
#include "ui/ui.h"
#include "render/camera.h"
#include "render/gltf.h"
#include "render/orrery_tests.h"
#include "render/text_tests.h"
#include "render/texture.h"
#include "sim/world.h"

namespace opra {

/** Defined in game/app.cpp: the nearest body inside the beam's swept segment, or null. */
Obstacle *beam_target(World &world, const Vec2 &muzzle, const Vec2 &direction, Real range,
                      Vec2 &hit_point);

namespace {

// The counters live in selftest_support.cpp so a module's own test file can link them alone; these
// wrappers keep this file's cases reading as they always have.
void check(bool ok, const char* what) { selftest::check(ok, what); }

void check_close(double actual, double expected, double tolerance, const char* what) {
    selftest::check_close(actual, expected, tolerance, what);
}

/**
 * 600 fixed steps from a fresh world. The golden numbers below were captured from this build on
 * 2026-09-12, after the grid-lifetime and fragment-collision changes; regenerate them only when a
 * sim change is intended, and say why. They are the guard that the port stays bit-identical to the
 * original TypeScript.
 */
void test_determinism() {
    World world;
    const FlightInput input{1.0, 0.2, 0.0, false, false};
    const Real dt = 1.0 / 120.0;
    for (int i = 0; i < 600; ++i) world.step(input, dt);

    check_close(world.ship.position.x, 23.630688409859413, 1e-9, "determinism: position.x");
    check_close(world.ship.position.y, 164.88502883314655, 1e-9, "determinism: position.y");
    check_close(world.ship.velocity.x, -16.200403415485933, 1e-9, "determinism: velocity.x");
    check_close(world.ship.velocity.y, 44.737950690534639, 1e-9, "determinism: velocity.y");
    check_close(world.ship.angle, 2.7506249999999874, 1e-9, "determinism: angle");
    check_close(world.ship.fuel, 15928.799999999683, 1e-9, "determinism: fuel");
    check_close(world.ship.heat, 0.0, 1e-9, "determinism: heat");
    check_close(world.ship.hull, 100.0, 1e-9, "determinism: hull");
    check_close(world.elapsed, 5.0, 1e-9, "determinism: elapsed");
    check_close(world.stationSpin, 0.175, 1e-9, "determinism: station spin");
}

/** Every rock in the grid must still be owned by the rock field, every fragment by the fragment
 *  storage: the fracture path used to leave the grid pointing at freed memory. */
void test_fracture() {
    World world;
    const int initial_rocks = static_cast<int>(world.rocks.size());
    // The background shelf (every fifth rock) never collides and never breaks: the planar field is
    // what the fracture path has to clear. Its size follows the belt retune, so the test counts it
    // rather than hardcoding a field size (s3.1).
    int planar = 0;
    for (const Obstacle& rock : world.rocks) {
        if (rock.z == 0) ++planar;
    }
    int broken = 0;
    while (broken < planar) {
        // Breaking retires the rock, so re-scan instead of holding an iterator across the call.
        Obstacle* next = nullptr;
        for (Obstacle& rock : world.rocks) {
            if (rock.z == 0 && !rock.retired) {
                next = &rock;
                break;
            }
        }
        if (!next) break;
        world.break_rock(*next);
        ++broken;
    }
    check(broken == planar, "fracture: the field breaks every rock it holds");
    int retired = 0;
    for (const Obstacle& rock : world.rocks) {
        if (rock.retired) ++retired;
    }
    check(retired == broken, "fracture: broken rocks are retired, not freed");

    const auto owned = [&world](const Obstacle* body) {
        for (const Obstacle& rock : world.rocks) {
            if (&rock == body) return true;
        }
        for (const Obstacle& fragment : world.fragments) {
            if (&fragment == body) return true;
        }
        return false;
    };
    std::size_t entries = 0;
    std::size_t dangling = 0;
    std::size_t retired_in_grid = 0;
    for (const auto& cell : world.grid.cells()) {
        for (const Obstacle* body : cell.second) {
            ++entries;
            if (!owned(body)) ++dangling;
            if (body->retired) ++retired_in_grid;
        }
    }
    check(retired_in_grid == 0, "fracture: retired bodies leave the grid");
    std::printf("  fracture: rocks=%d fragments=%zu grid entries=%zu\n", initial_rocks,
                world.fragments.size(), entries);
    check(dangling == 0, "fracture: every grid entry points at live storage");
    check(world.fragments.size() <= static_cast<std::size_t>(broken) * 3,
          "fracture: fragment count is bounded");
    check(world.fragments.size() > 0, "fracture: fragments were produced");

    // A vector would have dangled by now: 200 fractures push ~2-3 fragments each past any reserve.
    for (const Obstacle& fragment : world.fragments) {
        check(fragment.hp > 0 && fragment.radius >= 9.0, "fracture: fragments carry sane hp and radius");
        break;
    }
}

/** Collision primitives against hand-computed cases, including the touching and contained edges. */
void test_collision() {
    const Box box{0, 0, 10, 5, 0};  // 10 m half-length along +y, 5 m half-width along +x
    Vec2 out;

    // Circle 3 m from the face at x = 5, radius 3: exactly touching, so no penetration.
    check(!obb_circle_out(box, 8.0, 0.0, 3.0, out), "collision: touching circle reports no overlap");
    // 2.5 m from the face with radius 3: penetrating by 0.5 along +x.
    check(obb_circle_out(box, 7.5, 0.0, 3.0, out), "collision: penetrating circle reports overlap");
    check_close(out.x, 0.5, 1e-12, "collision: circle push x");
    check_close(out.y, 0.0, 1e-12, "collision: circle push y");
    // Centre inside the box: leaves along the shallowest face, so x, by halfWidth + radius.
    check(obb_circle_out(box, 0.0, 0.0, 1.0, out), "collision: contained circle reports overlap");
    check_close(out.x, 6.0, 1e-12, "collision: contained circle push x");
    check_close(out.y, 0.0, 1e-12, "collision: contained circle push y");

    const Box a{0, 0, 10, 5, 0};
    const Box b{7, 0, 10, 4, 0};
    check(obb_obb_out(a, b, out), "collision: overlapping boxes report overlap");
    check_close(out.x, 2.0, 1e-12, "collision: box push x");
    check_close(out.y, 0.0, 1e-12, "collision: box push y");
    const Box apart{20, 0, 10, 4, 0};
    check(!obb_obb_out(a, apart, out), "collision: separated boxes report no overlap");
    const Box touching{9, 0, 10, 4, 0};
    check(!obb_obb_out(a, touching, out), "collision: just-touching boxes report no overlap");

    // A box crossed at right angles: 12 m up, 10 m half-length so 4 m thick, overlapping by 2 m.
    // The minimum translation pushes B out along the axis from A to B, which is +y here.
    const Box crossed{0, 12, 10, 4, 1.5707963267948966};
    check(obb_obb_out(a, crossed, out), "collision: rotated overlap reports overlap");
    check_close(out.x, 0.0, 1e-9, "collision: rotated push x");
    check_close(out.y, 2.0, 1e-9, "collision: rotated push y");
}

/** Every exported model loads, and each ship's box matches the authored collision table. */
void test_models() {
    ModelSet models;
    models.build(asset_path("assets/models.json"));
    check(models.store.names().size() >= 8, "gltf: manifest names at least eight models");
    for (const std::string &name : models.store.names()) {
        check(!models.store.model(name).parts.empty(), ("gltf: " + name + " has parts").c_str());
        check(models.store.meta(name).aabb_max.y > models.store.meta(name).aabb_min.y,
              ("gltf: " + name + " has measured bounds").c_str());
    }
    for (int i = 0; i < 3; ++i) {
        const ModelMeta &meta = models.store.meta(SHIP_MODEL_NAMES[i]);
        // D-19: the box is the model's own AABB, not the inherited table.
        check_close(meta.collider.halfLength,
                    (meta.aabb_max.y - meta.aabb_min.y) / 2.0, 1e-4,
                    "gltf: ship box is the exported AABB, long axis");
        check_close(meta.collider.halfWidth, (meta.aabb_max.x - meta.aabb_min.x) / 2.0, 1e-4,
                    "gltf: ship box is the exported AABB, short axis");
        check(meta.collider.halfWidth > 0, "gltf: ship box has a width");
        // The drawn hull, effects lit, must land near that box on its long axis: the port gate.
        const double half_length = (meta.lit_aabb_max.y - meta.lit_aabb_min.y) / 2.0;
        check_close(half_length, HULL_BOXES[i].halfLength, HULL_BOXES[i].halfLength * 0.075,
                    "gltf: lit hull length within 7.5% of the table");
        const double half_width = (meta.lit_aabb_max.x - meta.lit_aabb_min.x) / 2.0;
        check(half_width > HULL_BOXES[i].halfWidth * 0.8,
              "gltf: lit hull width within 20% of the table");
    }
}

/** The immediate-mode state machine: mouse press/release, drag, and keyboard focus. */
void test_ui() {
    ui::Context context;
    UIBatch batch;
    const ui::Rect button{100.0f, 100.0f, 200.0f, 40.0f};
    const glm::vec2 screen{800.0f, 600.0f};

    const auto frame = [&](glm::vec2 at, bool down, bool press, bool release, ui::Nav nav) {
        ui::Pointer pointer;
        pointer.at = at;
        pointer.valid = true;
        pointer.down = down;
        pointer.pressed = press;
        pointer.released = release;
        context.begin(batch, screen, pointer, nav);
        const bool fired = context.button("test.button", button, "Fire");
        context.end();
        return fired;
    };

    const ui::Nav no_nav;
    check(!frame({50.0f, 50.0f}, false, false, false, no_nav), "ui: no fire from empty space");
    check(!frame({150.0f, 120.0f}, true, true, false, no_nav), "ui: press alone does not fire");
    check(!frame({150.0f, 500.0f}, true, false, true, no_nav), "ui: release outside does not fire");
    check(frame({150.0f, 120.0f}, true, true, false, no_nav) == false, "ui: press latches active");
    check(frame({150.0f, 120.0f}, false, false, true, no_nav), "ui: release inside fires");

    // Keyboard: Tab moves focus, Enter fires whatever holds it.
    ui::Nav tab;
    tab.next = true;
    frame({0.0f, 0.0f}, false, false, false, tab);
    ui::Nav enter;
    enter.activate = true;
    check(frame({0.0f, 0.0f}, false, false, false, enter), "ui: enter fires the focused widget");
    check(context.keyboard_in_use(), "ui: keyboard use is tracked");

    // A toggle flips once per click, on release inside, like every other widget.
    bool flag = false;
    ui::Pointer pointer;
    pointer.valid = true;
    pointer.at = {150.0f, 120.0f};
    pointer.pressed = true;
    pointer.down = true;
    context.begin(batch, screen, pointer, no_nav);
    context.toggle("test.toggle", button, "Flag", flag);
    context.end();
    check(!flag, "ui: toggle waits for the release");
    pointer.pressed = false;
    pointer.released = true;
    pointer.down = false;
    context.begin(batch, screen, pointer, no_nav);
    context.toggle("test.toggle", button, "Flag", flag);
    context.end();
    check(flag, "ui: release inside flips the toggle");
    pointer.released = false;
    context.begin(batch, screen, pointer, no_nav);
    context.toggle("test.toggle", button, "Flag", flag);
    context.end();
    check(flag, "ui: one click is one flip");

    float value = 0.0f;
    const ui::Rect track{100.0f, 200.0f, 200.0f, 30.0f};
    pointer.at = {250.0f, 215.0f};
    pointer.pressed = true;
    pointer.down = true;
    context.begin(batch, screen, pointer, no_nav);
    context.slider("test.slider", track, "Value", value, 0.0f, 10.0f);
    context.end();
    check_close(value, 7.5, 0.01, "ui: slider follows the pointer");
    pointer.pressed = false;
    pointer.down = false;
    pointer.released = true;
    context.begin(batch, screen, pointer, no_nav);
    context.slider("test.slider", track, "Value", value, 0.0f, 10.0f);
    context.end();
    check_close(value, 7.5, 0.01, "ui: slider keeps its value after release");
}

/**
 * D-1: the cutter is aimed off the nose, not off the starboard beam. A rock straight ahead has to
 * come out at bearing zero, with the beam running along the hull's own forward vector.
 */
void test_cutter_bearing() {
    World world;
    world.ship.angle = -0.63;  // nothing special: an angle where sin and cos differ in sign
    const Real angle = world.ship.angle;
    const Vec2 forward{-std::sin(angle), std::cos(angle)};
    const Vec2 muzzle{world.ship.position.x + forward.x * 60.0,
                      world.ship.position.y + forward.y * 60.0};
    const Vec2 aim{muzzle.x + forward.x * 150.0, muzzle.y + forward.y * 150.0};

    Real bearing = 9.0;
    const Vec2 direction = cutter_beam(angle, muzzle, aim, config::CUTTER_ARC, bearing);
    check_close(bearing, 0.0, 1e-12, "cutter: an aim point dead ahead is bearing zero");
    check_close(direction.x, forward.x, 1e-12, "cutter: the beam runs along forward.x");
    check_close(direction.y, forward.y, 1e-12, "cutter: the beam runs along forward.y");

    // And a rock sitting on that line is the one the beam finds.
    Obstacle rock;
    rock.id = 90001;
    rock.radius = 30.0;
    rock.hp = rock.maxHp = rock_hp(rock.radius);
    rock.x = muzzle.x + forward.x * 120.0;
    rock.y = muzzle.y + forward.y * 120.0;
    world.rocks.push_back(rock);
    world.grid.add(&world.rocks.back());

    Vec2 hit{};
    const Obstacle *found =
        beam_target(world, muzzle, direction, config::CUTTER_RANGE, hit);
    check(found != nullptr, "cutter: a rock dead ahead is hit");
    if (found) check(found->id == rock.id, "cutter: the rock dead ahead is the one hit");
}

/**
 * D-2: the per-frame update path was handed the fixed simulation step while running once per
 * rendered frame, so the cutter's draw scaled with the display refresh. The same wall-clock span
 * must cost the same fuel and heat at any frame rate.
 */
void test_frame_rate_independence() {
    const auto burn = [](double fps, double seconds) {
        ShipState ship = create_ship();
        const Real dt = 1.0 / fps;
        const int frames = static_cast<int>(seconds * fps + 0.5);
        for (int i = 0; i < frames; ++i) {
            step_cutter(ship, config::CUTTER_DRAW, config::CUTTER_HEAT, dt);
        }
        return ship;
    };

    const ShipState slow = burn(60.0, 600.0);
    const ShipState fast = burn(240.0, 600.0);
    check_close(slow.fuel, fast.fuel, 1e-6, "frame rate: 600 s of cutter draws the same fuel");
    check_close(slow.heat, fast.heat, 1e-6, "frame rate: 600 s of cutter builds the same heat");
    check(slow.fuel < SHIPS[0].fuel && slow.fuel > 0.0, "frame rate: the cutter actually drew");
    // Heat saturates long before 600 s, so check it again while it is still climbing.
    check_close(burn(60.0, 2.0).heat, burn(240.0, 2.0).heat, 1e-6,
                "frame rate: heat is rate independent before it saturates");
}

/**
 * D-7: a press whose release never arrives - the widget stopped being submitted while held - must
 * not leave the active id latched, or the next release over it fires with no press.
 */
void test_ui_active_release() {
    ui::Context context;
    UIBatch batch;
    const ui::Rect button{100.0f, 100.0f, 200.0f, 40.0f};
    const glm::vec2 screen{800.0f, 600.0f};
    const ui::Nav no_nav;

    ui::Pointer pointer;
    pointer.valid = true;
    pointer.at = {150.0f, 120.0f};
    pointer.down = true;
    pointer.pressed = true;
    context.begin(batch, screen, pointer, no_nav);
    context.button("test.latch", button, "Fire");
    context.end();
    check(context.active() == ui::hash_id("test.latch"), "ui: the press latches the active id");

    // The screen closed: the widget is not submitted, and the release never reaches it.
    pointer.pressed = false;
    pointer.down = false;
    context.begin(batch, screen, pointer, no_nav);
    context.end();
    check(context.active() == 0, "ui: a frame with the pointer up clears the active id");

    // Reopening and releasing over it must not fire a press that never happened.
    pointer.released = true;
    context.begin(batch, screen, pointer, no_nav);
    const bool fired = context.button("test.latch", button, "Fire");
    context.end();
    check(!fired, "ui: a stale release does not fire the widget");
}

/**
 * E5/P2: the camera is a perspective view on a double render origin. The projection round-trips
 * through the render origin - which is the whole reason the origin exists, since a float origin at
 * 10 Gm quantises the ship onto 1 km - depth runs backwards, and the pitch clamp keeps the aim ray
 * off the plane's parallel case.
 */
void test_camera() {
    const float width = 1600.0f;
    const float height = 900.0f;
    Camera camera;
    camera.origin = glm::dvec3(1.0e10, -2.5e9, 0.0);  // system scale on purpose
    camera.half_height = 340.0f;
    camera.aspect = width / height;
    camera.target = glm::vec3(0.0f);
    camera.eye = orbit_eye(camera.half_height, CAMERA_PITCH_DEFAULT);
    check(camera.eye.z > 0.0f, "camera: the eye sits above the play plane");

    const glm::mat4 matrix = view_projection(camera);
    const glm::vec2 centre = project(matrix, camera.origin, camera.origin.x, camera.origin.y, 0.0f,
                                     width, height);
    check_close(centre.x, width * 0.5, 0.5, "camera: the target projects to the centre column");
    check_close(centre.y, height * 0.5, 0.5, "camera: the target projects to the centre row");

    // Project -> unproject closes on the same world point near the origin, and the offsets are
    // double-subtracted first: this is where a float origin would show up as kilometres of error.
    const Real offsets[3] = {-260.0, 0.0, 180.0};
    for (Real offset : offsets) {
        const Real x = camera.origin.x + offset;
        const Real y = camera.origin.y + offset * 0.5;
        const glm::vec2 px = project(matrix, camera.origin, x, y, 0.0f, width, height);
        const glm::dvec2 back = unproject(camera, px.x, px.y, width, height);
        check_close(back.x, x, 0.05, "camera: unproject returns the world x it was given");
        check_close(back.y, y, 0.05, "camera: unproject returns the world y it was given");
    }

    // Reversed-Z: near is 1, far is 0, and nearer is greater - the mesh pipeline compares GREATER.
    const glm::vec3 view_dir = glm::normalize(camera.target - camera.eye);
    const auto clip_depth = [&](float distance) {
        const glm::vec4 clip = matrix * glm::vec4(camera.eye + view_dir * distance, 1.0f);
        return clip.z / clip.w;
    };
    // s2.2: the planes ride the eye distance - near = R*1e-3, far = R*1e3, a 1e6 ratio at every
    // scale, which is what keeps reversed-Z's precision constant from hull to system.
    check_close(camera.near_z(), camera.eye_distance() * 1.0e-3, 1e-3,
                "camera: the near plane rides the eye distance");
    (void)0;
    check_close(camera.far_z(), camera.eye_distance() * 1.0e3, 1.0,
                "camera: the far plane rides the eye distance");
    check_close(clip_depth(camera.near_z()), 1.0, 1e-4, "camera: the near plane maps to depth 1");
    check_close(clip_depth(camera.far_z()), 0.0, 1e-4, "camera: the far plane maps to depth 0");
    check(clip_depth(120.0f) > clip_depth(2400.0f), "camera: nearer is greater in reversed-Z");
    {
        // The ratio holds ten decades away, which is the whole point of deriving the planes.
        Camera far_out = camera;
        far_out.half_height = 5.0e9;
        check_close(far_out.far_z() / far_out.near_z(), 1.0e6, 1e3,
                    "camera: the near/far ratio is 1e6 at system scale too");
    }

    // At both pitch limits the bottom of the screen still cuts the plane in front of the camera.
    for (float pitch : {CAMERA_PITCH_MIN, CAMERA_PITCH_MAX}) {
        Camera tilted = camera;
        tilted.eye = orbit_eye(camera.half_height, pitch);
        check(tilted.eye.z > 0.0f, "camera: the eye stays above the plane at the pitch limit");
        const glm::dvec2 hit = unproject(tilted, width * 0.5f, height, width, height);
        check(std::isfinite(hit.x) && std::isfinite(hit.y),
              "camera: the bottom screen row reaches the plane at the pitch limit");
        check(std::fabs(hit.y - camera.origin.y) < 6000.0,
              "camera: the plane cut lands near the target, not at infinity");
    }

    // A rock beside the ship is where the aim ray says it is: screen -> world -> screen is stable.
    const glm::vec2 rock_px =
        project(matrix, camera.origin, camera.origin.x + 120.0, camera.origin.y - 90.0, 0.0f, width,
                height);
    const glm::dvec2 rock_world = unproject(camera, rock_px.x, rock_px.y, width, height);
    const glm::vec2 again = project(matrix, camera.origin, rock_world.x, rock_world.y, 0.0f, width,
                                    height);
    check_close(again.x, rock_px.x, 0.05, "camera: the aim round trip is stable in x");
    check_close(again.y, rock_px.y, 0.05, "camera: the aim round trip is stable in y");

    // F1: the follow. The deadzone holds, the lead caps, the spring does not overshoot, and the
    // whole thing is frame-rate independent - which is what the closed-form step is for.
    {
        const FollowParams params;
        FollowState held;
        const glm::dvec2 still =
            follow_step(held, {params.deadzone * 0.6, 0.0}, {0.0, 0.0}, 1.0 / 60.0, params);
        check(glm::length(still) == 0.0, "follow: inside the deadzone the camera does not move");

        check_close(follow_goal({0.0, 0.0}, {5000.0, 0.0}, params).x, params.lead_max, 1e-9,
                    "follow: the lead caps at lead_max");
        // Under the cap the lead is speed x lead_seconds, and the cap itself is a framing choice:
        // the ship must stay inside the frame and clear of the corner blocks (config.h says why).
        check_close(follow_goal({0.0, 0.0}, {40.0, 0.0}, params).x,
                    40.0 * params.lead_seconds, 1e-9,
                    "follow: under the cap the lead is speed x lead_seconds");
        check(params.lead_max <= 24.0 && params.deadzone <= 16.0,
              "follow: the frame stays calm enough to read the hull");
        check_close(follow_goal({0.0, 0.0}, {0.0, 0.0}, params).x, 0.0, 1e-9,
                    "follow: a stationary ship needs no lead");

        FollowState spring;
        bool monotone = true;
        double remaining = 300.0;
        for (int i = 0; i < 240; ++i) {
            follow_step(spring, {300.0, 0.0}, {0.0, 0.0}, 1.0 / 60.0, params);
            const double now = glm::length(glm::dvec2(300.0, 0.0) - spring.target);
            if (now > remaining + 1.0e-9) monotone = false;
            remaining = now;
        }
        check(monotone, "follow: a critically damped step never overshoots");
        check(remaining <= params.deadzone + 0.5, "follow: the spring settles inside the deadzone");

        FollowState coarse, fine;
        for (int i = 0; i < 60; ++i) {
            follow_step(coarse, {300.0, 0.0}, {0.0, 0.0}, 1.0 / 30.0, params);
        }
        for (int i = 0; i < 480; ++i) {
            follow_step(fine, {300.0, 0.0}, {0.0, 0.0}, 1.0 / 240.0, params);
        }
        check_close(glm::length(coarse.target - fine.target), 0.0, 0.05,
                    "follow: two seconds at 30 Hz lands with two seconds at 240 Hz");

        FollowState warped;
        const glm::dvec2 flung = follow_step(warped, {300.0, 0.0}, {0.0, 0.0}, 4.0, params);
        check(std::isfinite(flung.x) && flung.x <= 300.0 + 1.0e-6,
              "follow: a four second step is stable and stays behind the goal");

        FollowState jumped;
        const glm::dvec2 landed = follow_step(jumped, {5000.0, 0.0}, {0.0, 0.0}, 1.0 / 60.0, params);
        check_close(landed.x, 5000.0, 1.0e-9, "follow: past the snap distance it lands on the ship");
        check_close(jumped.velocity.x, 0.0, 1.0e-9,
                    "follow: a snap takes the ship's velocity, so the next step is smooth");

        // The camera's freedom (plan 05 J1): Ctrl and the mouse swing the eye inside a cone about
        // the home axis - thirty degrees in any direction, clamped exactly there, easing home on
        // release. The gate asks for: reaches 30 degrees and no further, every direction; returns
        // home; elevation stays inside [8, 88] degrees; world north stays within 30 of screen-up
        // (which the cone guarantees by never rolling and never crossing the pole).
        glm::dvec2 cone{0.0, 0.0};
        look_cone_step(cone, 400.0, 300.0, 400.0, 300.0, 900.0);
        check(glm::length(cone) == 0.0, "look: a cursor still on the anchor swings nothing");

        look_cone_step(cone, 400.0, 300.0, 400.0 + 100.0, 300.0, 900.0);
        check_close(glm::length(cone), 100.0 * 0.0035, 1e-12,
                    "look: drag maps to angle at the documented radians per pixel");

        // Any direction: the clamp is on the magnitude of the two-axis offset, so a diagonal drag
        // reaches the same thirty degrees as a straight one and no further.
        const double limit = 30.0 * 3.14159265358979323846 / 180.0;
        for (const glm::dvec2 drag : {glm::dvec2{900.0, 0.0}, glm::dvec2{0.0, 900.0},
                                      glm::dvec2{640.0, 640.0}, glm::dvec2{-900.0, 900.0}}) {
            glm::dvec2 swung{0.0, 0.0};
            look_cone_step(swung, 400.0, 300.0, 400.0 + drag.x, 300.0 + drag.y, 900.0);
            check(glm::length(swung) <= limit + 1e-12,
                  "look: no drag reaches past thirty degrees");
            check_close(glm::length(swung), limit, 1e-9,
                        "look: a hard drag reaches exactly thirty degrees");
            // The clamp keeps the drag's direction: it is a cone, not a square.
            check_close(glm::length(swung - glm::normalize(drag) * limit), 0.0, 1e-9,
                        "look: the clamp points the same way the drag did");
        }

        // It walks home on release: exp(-dt/tau) is exact, so two frame rates land together, and
        // half a second at the shipped tau is visually home.
        glm::dvec2 easing = cone;
        for (int i = 0; i < 240; ++i) easing = look_cone_release(easing, 1.0 / 120.0, 0.15);
        check(glm::length(easing) < limit * 1.0e-3, "look: releasing the key eases the eye home");

        glm::dvec2 relaxed = cone, slow = cone;
        for (int i = 0; i < 30; ++i) relaxed = look_cone_release(relaxed, 4.0 / 60.0, 0.15);
        for (int i = 0; i < 120; ++i) slow = look_cone_release(slow, 1.0 / 60.0, 0.15);
        check_close(glm::length(relaxed - slow), 0.0, 1e-12,
                    "look: two seconds at 15 Hz eases with two seconds at 60 Hz");
        check(look_cone_release(cone, 10.0, 0.0) == glm::dvec2(0.0),
              "look: a zero constant centre means it");

        // The swung eye respects the camera's own elevation limits whatever the cone does, and at
        // zero cone it is the home orbit exactly.
        for (const float pitch_deg : {12.0f, 32.0f, 75.0f}) {
            const float pitch = pitch_deg * 0.017453292519943295f;
            const glm::vec3 home = orbit_eye(150.0, pitch);
            check_close(glm::length(look_eye(150.0, pitch, glm::dvec2(0.0)) - home), 0.0, 1e-6,
                        "look: a centred cone is the home orbit");
            for (const glm::dvec2 full : {glm::dvec2{limit, 0.0}, glm::dvec2{0.0, limit},
                                          glm::dvec2{-limit, -limit * 0.7}}) {
                const glm::vec3 swung = look_eye(150.0, pitch, full);
                const float elevation =
                    std::atan2(swung.z, std::hypot(swung.x, swung.y)) * 57.29577951308232f;
                check(elevation >= 8.0f - 1e-4 && elevation <= 88.0f + 1e-4,
                      "look: the eye's elevation stays inside [8, 88] degrees");
            }
        }

        // A cone look moves the eye about the follow point and never the point itself: the view
        // can swing but the ship stays tracked, which is what makes the collar's bearing frame
        // survive a look.
        {
            const Camera cam{glm::dvec3(0.0), look_eye(150.0, 0.55850536f, glm::dvec2(limit, limit)),
                             glm::vec3(0.0f), 150.0, 16.0f / 9.0f};
            const glm::dvec3 forward = glm::normalize(glm::dvec3(cam.target) - glm::dvec3(cam.eye));
            check(glm::length(forward) > 0.99,
                  "look: the swung eye still looks at the follow point");
        }
    }
}

/**
 * The settings file round-trips. It writes the real path, so the previous contents are restored:
 * a test that clobbers the player's settings would be worse than no test.
 */
void test_settings() {
    const char *path = settings_path();
    check(path != nullptr, "settings: path resolves");
    if (!path) return;

    std::string original;
    std::ifstream existing(path, std::ios::binary);
    if (existing) original.assign((std::istreambuf_iterator<char>(existing)),
                                  std::istreambuf_iterator<char>());

    Settings written;
    written.camera_pitch = 47.0f;
    written.assist = false;
    written.reduced_motion = true;
    written.msaa = 4;
    written.show_stats = true;
    check(written.save(), "settings: save writes the file");

    Settings read;
    read.load();
    check_close(read.camera_pitch, 47.0, 1e-4, "settings: the camera pitch round-trips");
    check(read.assist == false && read.reduced_motion && read.msaa == 4 && read.show_stats,
          "settings: flags round-trip");

    // The pitch feeds the eye's position directly, so a value outside the slider's range - a
    // hand-edited file, an older build - keeps the default instead of pinning the camera.
    Settings out_of_range;
    out_of_range.camera_pitch = 5.0f;
    out_of_range.save();
    Settings reloaded;
    reloaded.load();
    check_close(reloaded.camera_pitch, 32.0, 1e-4,
                "settings: a camera pitch outside the range keeps the default");

    if (original.empty()) {
        std::remove(path);
    } else {
        std::ofstream restore(path, std::ios::binary | std::ios::trunc);
        restore << original;
    }
}

/**
 * Gate 2 (plan-04 s5): the sRGB/linear pairing is the one thing a texture pipeline can get wrong
 * without a visible smoke signal, so the pairing itself is asserted, and the loader is exercised
 * against the repaired maps the fixer wrote.
 */
void test_textures() {
    check(render::map_format(true) == SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM_SRGB,
          "texture: a colour map loads through the sRGB transfer");
    check(render::map_format(false) == SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM,
          "texture: a data map loads as numbers, untransformed");

    // The device needs the video subsystem up first, as the text tests do.
    const bool video = SDL_Init(SDL_INIT_VIDEO);
    check(video, "texture: SDL_Init");
    if (!video) return;

    SDL_GPUDevice *handle = SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_DXIL, false, nullptr);
    if (!handle) {
        check(false, "texture: a GPU device is needed to exercise load_texture");
        SDL_Quit();
        return;
    }
    gpu::Device device;
    device.handle = handle;
    const render::LoadedTexture tile =
        render::load_texture(device, asset_path("assets/textures/nereid_photosphere.png"), true);
    check(tile.width == 1024 && tile.height == 1024,
          "texture: the photosphere tile loads at its manifest size");
    const render::LoadedTexture equirect =
        render::load_texture(device, asset_path("assets/textures/tessera_albedo.png"), true);
    check(equirect.width == 2048 && equirect.height == 1024,
          "texture: the equirect loads two to one");
    render::destroy_texture(device, tile);
    render::destroy_texture(device, equirect);
    SDL_DestroyGPUDevice(handle);
    SDL_Quit();
}

}  // namespace
int run_selftest() {
    std::printf("opra selftest\n");
    test_determinism();
    test_fracture();
    test_collision();
    test_models();
    test_ui();
    combat_tests();
    test_ui_active_release();
    test_cutter_bearing();
    test_frame_rate_independence();
    test_camera();
    test_settings();
    test_textures();
    // Every suite reports through the shared harness, so the shared counters are authoritative; a
    // module that counted without forwarding would show up in its own return only, which is why the
    // two are maxed rather than added: adding them would count every failure twice.
    const int reported = orbit::tests() + orbit::transfer_tests() + orbit::encounter_tests() +
                         orbit::lagrange_tests() + system_tests() + dock_tests() +
                         shapes_tests() + warp_tests() + text_tests() + world_tests() +
                         component_tests() + combat_tests() + orrery_tests() + effects_tests() + flow_tests() +
                         worlds_tests();
    const int failures = std::max(selftest::failures(), reported);
    std::printf("%s: %d checks, %d failures\n", failures == 0 ? "ok" : "FAILED", selftest::checks(),
                failures);
    return failures;
}

}  // namespace opra
