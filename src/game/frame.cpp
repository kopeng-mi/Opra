#include "game/app.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

#include "core/log.h"
#include "game/bindings.h"
#include "game/config.h"
#include "sim/survey.h"
#include "game/effects.h"
#include "gpu/gpu.h"
#include "hud/hud.h"
#include "render/renderer.h"
#include "ui/menus.h"
#include "ui/screens.h"

namespace opra {

using namespace opra::config;  // tunables, bare by design

void models_rock_bounds(const ModelSet &models, const opra::Obstacle &rock, float &out_scale) {
    const int mesh = models.rock_for(static_cast<float>(rock.radius), rock.seed, out_scale);
    glm::vec3 lo(1e9f), hi(-1e9f);
    for (const opra::MeshVertex &vertex : models.library.at(mesh).vertices) {
        const glm::vec3 p = vertex.pos * out_scale;
        lo = glm::min(lo, p);
        hi = glm::max(hi, p);
    }
    SDL_Log("rock id=%d radius=%.1f mesh=%d scale=%.3f  world bounds x %.1f..%.1f y %.1f..%.1f", rock.id,
            static_cast<double>(rock.radius), mesh, out_scale, lo.x * 2.0f, hi.x * 2.0f, lo.y * 2.0f,
            hi.y * 2.0f);
}

/** Finds the rock the cutter is aimed at: the first body in the grid the segment crosses. */
Obstacle *beam_target(World &world, const Vec2 &muzzle, const Vec2 &direction, Real range,
                      Vec2 &hit_point);

/** The docking overlay's inputs, copied out of the world for the HUD layer. */
DockFrame dock_frame_for(const World &world);

/**
 * A body's on-screen pixel radius (s2.5): the honest size the LOD and the icon work read. The
 * divide is guarded - the follow body sits at the camera origin, where d would be zero - and the
 * caller decides what a body at the origin owes the screen instead.
 */
float body_pixel_radius(const Camera &camera, const World &world, int index, float height) {
    const glm::dvec2 offset =
        world.body_zone_position(index) - glm::dvec2(camera.origin.x, camera.origin.y);
    const double d = std::max(glm::length(offset), 1.0e-9);
    const double radius = world.system.bodies[static_cast<size_t>(index)].radius;
    const double pixels =
        radius / d * (static_cast<double>(height) * 0.5) / std::tan(CAMERA_FOV_Y * 0.5);
    return static_cast<float>(pixels);
}

void update_app(App &app, Uint32 width, Uint32 height, Real dt) {
    // Under --debug, every key the game receives and every pause transition: the log a support
    // report is read from. Silent otherwise.
    if (app.debug) {
        static bool paused_before = false;
        const bool paused_now = app.screen != ui::Screen::Flight;
        if (paused_now != paused_before) {
            SDL_Log("[input] paused %d -> %d (viewer %d) at t+%.2f", paused_before ? 1 : 0,
                    paused_now ? 1 : 0, app.screen == ui::Screen::Viewer ? 1 : 0,
                    app.world.elapsed);
            paused_before = paused_now;
        }
        for (int code = 0; code < SDL_SCANCODE_COUNT; ++code) {
            if (app.input.keys[code] && !app.input.previous[code]) {
                SDL_Log("[input] key down scancode %d (%s)", code,
                        SDL_GetScancodeName((SDL_Scancode)code));
            }
        }
    }
    // F5 re-reads the models on demand; --debug also polls once a second so re-exporting a ship
    // shows up without a restart.
    if (app.input.pressed(SDL_SCANCODE_F5)) app.reload_models_if_stale(true);
    const Uint64 ticks = SDL_GetTicks();
    if (app.debug && ticks - app.model_poll_at >= 1000) {
        app.model_poll_at = ticks;
        app.reload_models_if_stale(false);
        // One line a second of flight state: enough to verify bindings and direction by eye.
        const ShipState &ship = app.world.ship;
        SDL_Log("t+%05.1f pos %8.1f %8.1f  vel %7.1f %7.1f  speed %6.1f  heading %6.1f deg  "
                "spin %+6.1f deg/s  thrust %+.2f  hull %5.1f  fuel %6.0f  zoom %.2f  warp %g  "
                "assist %d  paused %d",
                app.world.elapsed, static_cast<double>(ship.position.x),
                static_cast<double>(ship.position.y), static_cast<double>(ship.velocity.x),
                static_cast<double>(ship.velocity.y), length(ship.velocity), heading(ship.angle),
                ship.angularVelocity * 57.29577951308232, static_cast<double>(ship.thrustLevel),
                static_cast<double>(ship.hull), static_cast<double>(ship.fuel),
                static_cast<double>(app.half_height_current), app.warp.rate(), ship.assist ? 1 : 0,
                app.screen != ui::Screen::Flight ? 1 : 0);
    }
    // F1: the follow. It steps with the wall-clock dt, not the sim's fixed step, so the framing is
    // frame-rate independent; a step bigger than the snap distance (a warp jump, a reset) is
    // handled inside. It runs whether or not the flight camera is the one on screen, because the
    // plate would otherwise hand back a camera that had to catch up. The target is the ship unless
    // a double-click put the follow on a body (s2.1); a docked ship inherits the station's motion,
    // and the follow never jumps on undock because it was tracking the ship all along (s2.8).
    const glm::dvec2 ship_at(app.world.ship.position.x, app.world.ship.position.y);
    const glm::dvec2 ship_vel(app.world.ship.velocity.x, app.world.ship.velocity.y);
    if (app.follow_body >= 0 &&
        app.follow_body < static_cast<int>(app.world.bodies.size())) {
        // The follow goal is the body's ZONE-frame state: the camera's origin lives in the zone
        // frame with the ship, and a barycentric goal would put the view a quadrant away.
        const int index = app.follow_body;
        follow_step(app.follow, app.world.body_zone_position(index),
                    app.world.body_zone_velocity(index), dt, FollowParams{});
    } else {
        follow_step(app.follow, ship_at, ship_vel, dt, FollowParams{});
    }

    Input &input = app.input;
    World &world = app.world;

    // The model viewer owns the frame while it is up: its own camera, its own input.
    if (app.screen == ui::Screen::Viewer) {
        update_viewer(app.viewer, input, app.models);
        if (pressed(input, Action::ModelViewer)) {
            app.screen = ui::advance(app.screen, Action::ModelViewer);
        } else if (pressed(input, Action::Pause)) {
            app.screen = ui::advance(app.screen, Action::Pause);
        }
        if (app.screen != ui::Screen::Viewer) app.toast("Flight view");
        if (app.settings_dirty) {
            app.settings.save();
            app.settings_dirty = false;
        }
        return;
    }

    // The title screen's choice: taken on the frame after it was made, so the plate has already
    // drawn the row the pilot pressed.
    if (app.title_action != ui::TitleAction::None) {
        const ui::TitleAction action = app.title_action;
        app.title_action = ui::TitleAction::None;
        switch (action) {
            case ui::TitleAction::Continue:
                app.screen = ui::advance_menu(app.screen, ui::MenuAction::TitleContinue);
                break;
            case ui::TitleAction::NewContract:
                app.reset_run();
                app.screen = ui::advance_menu(app.screen, ui::MenuAction::TitleNewContract);
                break;
            case ui::TitleAction::Settings:
                app.screen = ui::advance_menu(app.screen, ui::MenuAction::TitleSettings);
                break;
            case ui::TitleAction::Manual:
                app.screen = ui::advance_menu(app.screen, ui::MenuAction::TitleManual);
                break;
            case ui::TitleAction::Quit: app.wants_quit = true; break;
            case ui::TitleAction::None: break;
        }
        return;
    }

    // The startup plate owns the frame while it is up: it is the one screen where the flight
    // HUD, the cutter and the camera keys all stand down.
    if (app.screen == ui::Screen::Startup) {
        app.title_reveal = std::min(1.0f, app.title_reveal + static_cast<float>(dt) * 0.85f);
        app.map_frame = orrery_frame_for(app.world, app.map_target);
        app.pointer.at = input.pointer;
        app.pointer.valid = input.pointer_valid;
        app.pointer.down = input.left;
        app.pointer.pressed = input.left_pressed();
        app.pointer.released = input.left_released();
        app.pointer.wheel = input.wheel;
        app.nav.next = input.pressed(SDL_SCANCODE_TAB) || input.pressed(SDL_SCANCODE_DOWN);
        app.nav.previous = input.pressed(SDL_SCANCODE_UP);
        app.nav.activate = input.pressed(SDL_SCANCODE_RETURN) || input.pressed(SDL_SCANCODE_KP_ENTER);
        app.nav.decrease = false;
        app.nav.increase = false;
        return;
    }

    // This frame's UI input: the pointer plus the keyboard navigation edges.
    app.pointer.at = input.pointer;
    app.pointer.valid = input.pointer_valid;
    app.pointer.down = input.left;
    app.pointer.pressed = input.left_pressed();
    app.pointer.released = input.left_released();
    app.pointer.wheel = input.wheel;
    app.nav.next = input.pressed(SDL_SCANCODE_TAB) || input.pressed(SDL_SCANCODE_DOWN);
    app.nav.previous = input.pressed(SDL_SCANCODE_UP);
    app.nav.activate = input.pressed(SDL_SCANCODE_RETURN) || input.pressed(SDL_SCANCODE_KP_ENTER) ||
                       input.pressed(SDL_SCANCODE_SPACE);
    app.nav.decrease = input.pressed(SDL_SCANCODE_LEFT);
    app.nav.increase = input.pressed(SDL_SCANCODE_RIGHT);

    if (app.settings_dirty) {
        app.settings.save();
        app.settings_dirty = false;
    }

    // Camera scale (plan 05 s2.1): the wheel and the zoom keys multiply the half-height, so a
    // notch feels the same at every scale - ten orders of magnitude on one control. Shift makes a
    // notch a whole decade. SDL reports a wheel scrolled away from the user as positive, and that
    // is the zoom-in direction.
    const double notch = input.held(SDL_SCANCODE_LSHIFT) || input.held(SDL_SCANCODE_RSHIFT)
                             ? ZOOM_NOTCH_FAST
                             : ZOOM_NOTCH;
    if (input.wheel != 0.0f) {
        app.half_height = clamp_half_height(
            app.half_height * std::exp(-static_cast<double>(input.wheel) * notch));
    }
    if (held(input, Action::ZoomIn)) {
        app.half_height = clamp_half_height(app.half_height * std::exp(-notch));
    }
    if (held(input, Action::ZoomOut)) {
        app.half_height = clamp_half_height(app.half_height * std::exp(notch));
    }
    // Home (s2.1): the flight framing and the ship, wherever the wheel had got to.
    if (pressed(input, Action::ZoomReset)) {
        app.half_height = HOME_HALF;
        app.follow_body = -1;
        app.camera_look = glm::dvec2(0.0);
    }
    // Scale presets (s2.1): 1..5 jump to hull / flight / local / orbital / system.
    for (int preset = 0; preset < 5; ++preset) {
        if (input.pressed(static_cast<SDL_Scancode>(SDL_SCANCODE_1 + preset))) {
            app.half_height = SCALE_PRESETS[preset];
            app.system_recall_half = 0.0;
        }
    }
    // The system as the map was (J2): M jumps to system scale and back. The map screen itself is
    // gone - this is the same view, the same world, seen ten decades wider.
    if (pressed(input, Action::Map)) {
        if (app.system_recall_half > 0.0) {
            app.half_height = app.system_recall_half;
            app.system_recall_half = 0.0;
        } else {
            app.system_recall_half = app.half_height;
            app.half_height = SCALE_PRESETS[4];
            app.follow_body = -1;
        }
        app.toast(app.system_recall_half > 0.0 ? "System scale" : "Flight scale");
    }
    // Time warp: the rail is data (game/warp.*), so the keys, the manual and the HUD cannot drift
    // apart. Raising it is a request, not a promise: any burn or contact drops it straight back.
    if (pressed(input, Action::WarpUp)) app.warp.request(1);
    if (pressed(input, Action::WarpDown)) app.warp.request(-1);
    // F10: one key, three levels. The toast names the level: a HUD that hides a block has to say
    // which one it hid, and the manual prints the same three names.
    // G15: the deployment is checked against the orbit it was released on, one period later, so a
    // marginal release fails honestly rather than at the instant of release. The contract id is the
    // world's own count: there is no contract desk to ask (plan 3.7).
    if (pressed(input, Action::Deploy)) {
        const int body = app.world.primary;
        const std::string id = "SAT-" + std::to_string(app.world.satellites.size() + 1);
        if (deploy_satellite(app.world, body, id)) {
            app.toast(id + " away - checked in one period");
        } else {
            app.toast("no orbit to deploy on");
        }
    }
    // Deployed satellites come due on their own clock, and the pilot is told which contract closed.
    for (const std::string &checked : update_satellites(app.world)) {
        app.toast(checked + (app.world.satellites.empty() ? "" : " check complete"));
    }
    if (pressed(input, Action::Density)) {
        app.density = next_density(app.density);
        app.toast(density_name(app.density));
    }
    // F5: the minimap's mode. The context sets it until the key is pressed, then the pilot owns it
    // - except on approach, where the corridor is forced (minimap.h says why).
    if (pressed(input, Action::Minimap)) {
        MinimapChoice &choice = minimap_choice();
        choice.mode = next_minimap_mode(choice.mode);
        choice.manual = true;
        app.toast(minimap_mode_name(choice.mode));
    }
    // The camera's freedom (J1): Ctrl and the mouse swing the eye inside a 30-degree cone about
    // the home axis. On press the cursor position is held; while held, the drag from that anchor
    // is the cone angle, clamped; release and the angle walks back home, so a look is never a new
    // home. The follow point does not move during a look - at system zoom an unbounded pan would
    // lose the ship, which is why the plan-04 plane grab is gone.
    const bool looking = input.pointer_valid &&
                         (input.held(SDL_SCANCODE_LCTRL) || input.held(SDL_SCANCODE_RCTRL));
    if (looking) {
        if (!app.look_anchor.has_value()) {
            app.look_anchor = input.pointer;
        } else {
            look_cone_step(app.camera_look, app.look_anchor->x, app.look_anchor->y, input.pointer.x,
                           input.pointer.y, static_cast<double>(height));
        }
    } else {
        app.look_anchor.reset();
        // A look is a look: let go and the eye eases back to the home axis.
        app.camera_look = look_cone_release(app.camera_look, dt, config::LOOK_RELEASE_TAU);
    }

    if (app.settings.reduced_motion) {
        app.half_height_current = app.half_height;  // no camera animation
    } else {
        // Log-space easing: multiplicative in feel, so one notch is one notch at hull or system.
        const double rate = 1.0 - std::exp(-static_cast<float>(dt) * ZOOM_SMOOTHING);
        const double from = std::log(app.half_height_current);
        const double to = std::log(app.half_height);
        app.half_height_current = std::exp(from + (to - from) * rate);
    }

    // s2.7's hard rule, from the zoom side: below a 200 m half-height close quarters is real time.
    if (app.half_height_current < ZOOM_REAL_TIME_BELOW &&
        app.warp.drop_to_real_time(Warp::Drop::Zoom)) {
        app.toast("Warp dropped to 1x - close quarters");
    }

    auto overworld = [&](const glm::vec2 &screen) {
        return unproject(app.camera, screen.x, screen.y, static_cast<float>(width),
                         static_cast<float>(height));
    };

    if (pressed(input, Action::Pause)) {
        const ui::Screen before = app.screen;
        app.screen = ui::advance(app.screen, Action::Pause);
        if (before == ui::Screen::Flight && app.screen == ui::Screen::Pause) {
            app.toast("Paused");
        } else if (before == ui::Screen::Pause && app.screen == ui::Screen::Flight) {
            app.toast("Flight resumed");
        }
    }
    app.mining = false;
    app.beam_active = false;

    // These keys are edges of Flight and of the instrument screens themselves: M opens the chart
    // from flight and closes it from the chart, and FLOW says which is which. They are read before
    // the return below, or an instrument could only ever be left with Esc.
    if (pressed(input, Action::Manual)) app.screen = ui::advance(app.screen, Action::Manual);
    if (pressed(input, Action::Chart)) app.screen = ui::advance(app.screen, Action::Chart);
    if (pressed(input, Action::ModelViewer)) {
        app.screen = ui::advance(app.screen, Action::ModelViewer);
        if (app.screen == ui::Screen::Viewer) app.toast("Model viewer");
    }
    // Close quarters (plan 05 s5): the PDCs' release, then a torpedo at whatever is tracked.
    if (pressed(input, Action::WeaponsFree)) {
        app.world.weapons_free = !app.world.weapons_free;
        app.toast(app.world.weapons_free ? "Weapons free" : "Weapons hold");
    }
    if (pressed(input, Action::FireTorpedo)) {
        if (app.world.fire_torpedo()) {
            app.toast("Torpedo away");
        } else {
            app.toast("No torpedo - nothing tracked, or docked");
        }
    }
    // The planner, live in the flight view (J2 replaced the map screen): N inserts the selected
    // body's transfer burns, B advances to the first of them exactly.
    if (pressed(input, Action::PlanNode)) app.plan_transfer();
    if (pressed(input, Action::WarpToNode)) {
        if (app.world.warp_to_next_node()) app.toast("Burn complete");
    }
    if (app.screen != ui::Screen::Flight) return;

    if (pressed(input, Action::Cinematic)) {
        app.cinematic = !app.cinematic;
        app.toast(app.cinematic ? "Cinematic view" : "Flight view");
    }
    if (pressed(input, Action::Assist)) {
        world.ship.assist = !world.ship.assist;
        app.settings.assist = world.ship.assist;  // the launch default follows the key
        app.settings_dirty = true;
        app.toast(world.ship.assist ? "Attitude assist engaged" : "Attitude assist off");
    }
    if (pressed(input, Action::CycleContact)) {
        std::vector<Contact> contacts = world.contacts();
        if (!contacts.empty()) {
            const int at = contact_index(contacts, world.target);
            const size_t next = static_cast<size_t>((at + 1) % static_cast<int>(contacts.size()));
            world.target = contacts[next].id;
            app.toast("Track: " + contacts[next].name);
        }
    }

    // Left button selects the contact under the cursor: a real target, tracked by the collar.
    if (input.left_pressed() && input.pointer_valid) {
        std::vector<Contact> contacts = world.contacts();
        const glm::mat4 matrix = view_projection(app.camera);
        int best = -1;
        float best_distance = 34.0f;
        for (size_t i = 0; i < contacts.size(); ++i) {
            const glm::vec2 point = project(matrix, app.camera.origin, contacts[i].position.x,
                                            contacts[i].position.y, 0.0f,
                                            static_cast<float>(width), static_cast<float>(height));
            if (point.x < -80.0f || point.y < -80.0f || point.x > static_cast<float>(width) + 80.0f ||
                point.y > static_cast<float>(height) + 80.0f) {
                continue;
            }
            const float distance = glm::length(point - input.pointer);
            if (distance < best_distance) {
                best_distance = distance;
                best = static_cast<int>(i);
            }
        }
        if (best >= 0) {
            world.target = contacts[static_cast<size_t>(best)].id;
            app.toast("Track: " + contacts[static_cast<size_t>(best)].name);
        } else if (!app.guns_warned) {
            app.guns_warned = true;
            app.toast("No guns fitted - the cutter is on C or the right button");
        }

        // Frame a body (s2.1): the second click of a double-click puts the follow on the body
        // under the cursor and sizes the view to two and a half of its radii. The same gesture
        // works at every scale, which is what makes one continuous zoom navigable.
        const double click_seconds = static_cast<double>(ticks) / 1000.0;
        const bool double_click =
            (click_seconds - app.last_click_at) < 0.35 &&
            glm::length(input.pointer - app.last_click_at_px) < 14.0f;
        app.last_click_at = click_seconds;
        app.last_click_at_px = input.pointer;
        if (double_click) {
            const float w = static_cast<float>(width), h = static_cast<float>(height);
            const glm::mat4 click_matrix = view_projection(app.camera);
            int body = -1;
            float best_body = 28.0f;
            for (size_t i = 0; i < world.bodies.size(); ++i) {
                const glm::dvec2 at_zone = world.body_zone_position(static_cast<int>(i));
                const glm::vec2 at = project(click_matrix, app.camera.origin, at_zone.x, at_zone.y,
                                             0.0f, w, h);
                const bool clipped = at.x < 0.0f || at.y < 0.0f || at.x > w || at.y > h;
                if (clipped || !std::isfinite(at.x) || !std::isfinite(at.y)) continue;
                const float hit =
                    std::max(14.0f, 2.5f * body_pixel_radius(app.camera, world,
                                                             static_cast<int>(i), h));
                const float distance = glm::length(at - input.pointer);
                if (distance < std::min(best_body, hit)) {
                    best_body = distance;
                    body = static_cast<int>(i);
                }
            }
            if (body >= 0) {
                app.follow_body = body;
                app.map_target = body;
                app.system_recall_half = 0.0;
                app.half_height =
                    clamp_half_height(2.5 * static_cast<double>(world.system.bodies[static_cast<size_t>(body)].radius));
                app.toast("Frame: " + world.system.bodies[static_cast<size_t>(body)].name);
            }
        }
    }

    if (pressed(input, Action::Interact)) {
        std::vector<Contact> contacts = world.contacts();
        bool handled = false;
        for (size_t i = 0; i < contacts.size() && !handled; ++i) {
            if (!contacts[i].is_cargo) continue;
            const opra::Cargo &cargo = world.cargos[static_cast<size_t>(contacts[i].cargo_index)];
            if (opra::can_recover(world.ship, cargo)) {
                world.cargos[static_cast<size_t>(contacts[i].cargo_index)].collected = true;
                app.toast("Recovered: " + contacts[i].name);
                handled = true;
            }
        }
        if (!handled) {
            if (opra::can_dock(world.ship)) {
                app.toast("Docking clearance open");
            } else {
                app.toast("Nothing in recovery range");
            }
        }
    }

    // The mining cutter: right button or C, aimed by the pointer inside the mount's arc.
    app.mining = (input.right || input.held(SDL_SCANCODE_C)) &&
                 app.screen != ui::Screen::Manual && app.screen != ui::Screen::Chart;
    if (app.mining) {
        const Real heading_angle = world.ship.angle;
        const glm::dvec2 forward(-std::sin(heading_angle), std::cos(heading_angle));
        glm::dvec2 aim_world(world.ship.position.x + forward.x * 100.0,
                             world.ship.position.y + forward.y * 100.0);
        if (input.pointer_valid) aim_world = overworld(input.pointer);

        // `bounds` is already in sim metres, at the scale the hull is drawn with.
        const Real muzzle_offset = world.ship.bounds.halfLength + 9.0;
        const Vec2 muzzle{world.ship.position.x + forward.x * muzzle_offset,
                          world.ship.position.y + forward.y * muzzle_offset};
        Real bearing = 0;
        const Vec2 direction = cutter_beam(heading_angle, muzzle,
                                           Vec2{aim_world.x, aim_world.y}, CUTTER_ARC, bearing);

        Vec2 hit_point{};
        opra::Obstacle *hit =
            beam_target(world, muzzle, direction, CUTTER_RANGE, hit_point);
        const bool can_cut = world.ship.fuel > 0 && world.ship.hull > 0 && world.ship.heat < 0.99;
        if (can_cut) {
            step_cutter(world.ship, CUTTER_DRAW, CUTTER_HEAT, dt);
            if (hit) {
                hit->hp -= CUTTER_DAMAGE * dt;
                if (hit->hp <= 0) {
                    world.break_rock(*hit);
                    hit = nullptr;  // the body is gone: retiring it moved it out of the field
                    app.toast("Rock broken - ore in the drift");
                }
            }
        }
        app.beam_active = true;
        app.beam_from = {muzzle.x, muzzle.y};
        app.beam_to = hit ? glm::dvec2(hit_point.x, hit_point.y)
                          : glm::dvec2(muzzle.x + direction.x * CUTTER_RANGE,
                                       muzzle.y + direction.y * CUTTER_RANGE);
    }

    if (world.ship.hull <= 0 && !app.wrecked) {
        app.wrecked = true;
        app.toast("Hull lost - R restarts the run");
    }
    if (app.wrecked && pressed(input, Action::Interact)) {
        app.reset_run();
    }

    // This frame's edges, read last on purpose: the cutter breaks a rock inside this same call, and
    // an update that ran before the mining block would never see that body retire - so a fracture
    // would only ever reach the screen on the *next* frame, or never, if the frame after it is the
    // one that gets photographed. Contact sparks, fracture dust and trails and the dock pulse all
    // read their time from the sim clock, so a paused or warped frame shows what the sim is doing.
    world_effects().update(world);
}


void render_app(App &app, SDL_GPUCommandBuffer *cmd, SDL_GPUTexture *color,
                SDL_GPUTextureFormat format, Uint32 width, Uint32 height) {
    Renderer &renderer = app.renderer;

    app.scene.clear();
    UIBatch ui;
    if (app.screen == ui::Screen::Startup) {
        // The orrery turns behind the title plate: the one screen the flight camera does not own
        // (E12). The map screen it shared a camera with is one continuous zoom now (plan 05 J2).
        orrery::build(app.scene, app.orrery_meshes, app.map_frame);
        const ui::Rect screen{0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height)};
        app.ui.begin(ui, {static_cast<float>(width), static_cast<float>(height)}, app.pointer,
                     app.nav);
        ui::TitleFrame title;
        title.sessionSeconds = app.world.elapsed;
        title.docked = app.world.docked;
        title.dockName = "Wayfarer";
        title.shipName = app.world.ship.spec ? app.world.ship.spec->name : "";
        title.reveal = app.title_reveal;
        const ui::TitleAction action =
            ui::build_title(app.ui, screen, title, app.title_selected);
        if (action != ui::TitleAction::None) app.title_action = action;
        app.ui.end();
        draw_frame(renderer, app.text, cmd, color, format, width, height, app.camera, app.scene, ui);
        return;
    }
    if (app.screen == ui::Screen::Viewer) {
        const ui::Rect screen{0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height)};
        app.ui.begin(ui, {static_cast<float>(width), static_cast<float>(height)}, app.pointer,
                     app.nav);
        build_viewer_ui(app.ui, screen, app.models, app.viewer);
        app.ui.end();
        build_viewer_scene(app.scene, app.models, app.viewer);
        draw_frame(renderer, app.text, cmd, color, format, width, height, app.camera, app.scene, ui);
        return;
    }
    // The scene builds with the camera's own render origin: the backdrop, the field and the ship
    // are all placed relative to it, inside a margin for the biggest rock.
    build_scene(app.scene, app.models, app.world, app.backdrop, app.camera, static_cast<float>(width),
                static_cast<float>(height), app.lod_memory);

    HudFrame frame = make_hud_frame(app, width, height);
    app.ui.begin(ui, {static_cast<float>(width), static_cast<float>(height)}, app.pointer, app.nav);
    // The menus take the frame: the flight HUD stays out of the way. This is the two menu screens,
    // not every screen that holds the sim - the chart and the manual still draw the HUD under them.
    if (app.screen == ui::Screen::Pause || app.screen == ui::Screen::Settings) {
        const ui::Rect screen{0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height)};
        if (app.screen == ui::Screen::Settings) {
            const ui::SettingsResult result = ui::build_settings(app.ui, screen, app.settings);
            if (result.back) {
                app.screen = ui::advance_menu(app.screen, ui::MenuAction::SettingsBack);
            }
            if (result.changed) app.apply_settings();
        } else {
            const ui::PauseResult result = ui::build_pause(app.ui, screen);
            if (result.resume) {
                app.screen = ui::advance_menu(app.screen, ui::MenuAction::PauseResume);
            }
            if (result.open_settings) {
                app.screen = ui::advance_menu(app.screen, ui::MenuAction::PauseSettings);
            }
            if (result.quit) app.wants_quit = true;
        }
    } else if (!app.hud_hidden) {
        // A port in range replaces the collar with the corridor ladder: the approach is the one
        // time the pilot needs numbers rather than bearings (plan §4.4).
        const DockFrame dock = dock_frame_for(app.world);
        if (dock.active) {
            build_dock_overlay(ui, frame, dock);
        } else {
            build_flight_hud(ui, frame);
        }
        if (app.settings.show_stats) {
            ui::build_stats(app.ui, {0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height)},
                            app.fps, renderer.instance_count, renderer.run_count,
                            renderer.triangle_count, gpu::submit_count());
        }
    }
    app.ui.end();

    SceneBuilder &scene = app.scene;
    const float w = static_cast<float>(width);
    const float h = static_cast<float>(height);
    const glm::mat4 matrix = view_projection(app.camera);
    // The zoom overlay (s2.4, s2.6): orbit lines, predicted legs, node marks and icons, drawn into
    // the UI batch so they land after tonemap with the depth test always off. Flight only: the
    // chart and the manual own the whole glass when they are up.
    if (app.screen == ui::Screen::Flight) {
        build_zoom_overlay(ui, app.world, app.camera, w, h);
    }
    if (app.beam_active) {
        const glm::vec2 from =
            project(matrix, app.camera.origin, app.beam_from.x, app.beam_from.y, 0.0f, w, h);
        const glm::vec2 to =
            project(matrix, app.camera.origin, app.beam_to.x, app.beam_to.y, 0.0f, w, h);
        ui::push_line(ui, from, to, 2.0f, ui::with_alpha(ui::tokens::DRIVE, 0.9f));
        ui::push_disc(ui, to, 4.0f, ui::with_alpha(ui::tokens::THREAT, 0.9f));
    }
    if (app.input.pointer_valid && app.screen != ui::Screen::Chart &&
        app.screen != ui::Screen::Manual) {
        ui::push_arc(ui, app.input.pointer, 9.0f, 0.0f, 6.2831853f, 1.0f,
                     ui::with_alpha(ui::tokens::ETCH_DIM, 0.5f));
    }
    if (app.screen == ui::Screen::Chart) ui::build_chart(ui, chart_frame_for(app.world), w, h);
    if (app.screen == ui::Screen::Manual) ui::build_help(ui, w, h);
    ui::draw_toasts(ui, app.toasts, app.now, 34.0f, 104.0f);

    draw_frame(renderer, app.text, cmd, color, format, width, height, app.camera, scene, ui);
}

/** The docking overlay's inputs, copied out of the world so the HUD layer never sees a World. */
DockFrame dock_frame_for(const World &world) {
    DockFrame dock;
    dock.active = world.target_port >= 0 && !world.station_ports.empty();
    if (!dock.active) return dock;
    const ApproachGate &gate = world.gate;
    dock.axial = static_cast<float>(gate.axial);
    dock.lateral = static_cast<float>(gate.lateral);
    dock.closing = static_cast<float>(gate.closing);
    dock.alignment = static_cast<float>(gate.alignment * 57.29577951308232);
    dock.rate = static_cast<float>(gate.rate * 57.29577951308232);
    dock.axial_ok = gate.axial_ok;
    dock.lateral_ok = gate.lateral_ok;
    dock.closing_ok = gate.closing_ok;
    dock.alignment_ok = gate.alignment_ok;
    dock.rate_ok = gate.rate_ok;
    dock.held = gate.held;
    dock.hold = static_cast<float>(gate.held_for);
    dock.hold_required = 0.4f;
    dock.port = "Wayfarer";
    if (world.target_port < static_cast<int>(world.station_ports.size())) {
        dock.port = world.station_ports[static_cast<size_t>(world.target_port)].id.c_str();
    }
    dock.range = static_cast<float>(distance(world.ship_port_state.position, world.target_state.position));
    dock.docked = world.docked;
    dock.dockedSeconds = static_cast<float>(std::max(0.0, world.docked_for));
    return dock;
}



/** --dump-models: every model's parts, effects and bounds, plus the collider table check. */
void dump_models(const ModelSet &models, const World &world) {
    for (const std::string &name : models.store.names()) {
        const Model &model = models.store.model(name);
        glm::vec3 lo(1e9f), hi(-1e9f);
        int effects = 0;
        for (const MeshPart &part : model.parts) {
            if (part.effect) ++effects;
            const MeshData &mesh = models.library.at(part.mesh);
            for (const MeshVertex &vertex : mesh.vertices) {
                const glm::vec3 p = part.pos + part.rot * (vertex.pos * part.scale);
                lo = glm::min(lo, p);
                hi = glm::max(hi, p);
            }
        }
        SDL_Log("model %-9s parts=%3zu effects=%d  bounds x %.1f..%.1f  y %.1f..%.1f  z %.1f..%.1f",
                name.c_str(), model.parts.size(), effects, static_cast<double>(lo.x),
                static_cast<double>(hi.x), static_cast<double>(lo.y), static_cast<double>(hi.y),
                static_cast<double>(lo.z), static_cast<double>(hi.z));
    }
    // The port gate: the model's own box against the authored collision table.
    for (int i = 0; i < 3; ++i) {
        const ModelMeta &meta = models.store.meta(SHIP_MODEL_NAMES[i]);
        const HullBoxes &table = HULL_BOXES[i];
        const double lit_half_length = (meta.lit_aabb_max.y - meta.lit_aabb_min.y) / 2.0;
        SDL_Log("collider %-9s sidecar %.1f x %.1f | table %.1f x %.1f | lit hull %.2f "
                "(length delta %+.1f%%, width delta %+.1f%%)",
                SHIP_MODEL_NAMES[i], static_cast<double>(meta.collider.halfLength),
                static_cast<double>(meta.collider.halfWidth), static_cast<double>(table.halfLength),
                static_cast<double>(table.halfWidth), lit_half_length,
                (lit_half_length / table.halfLength - 1.0) * 100.0,
                ((meta.lit_aabb_max.x - meta.lit_aabb_min.x) / 2.0 / table.halfWidth - 1.0) * 100.0);
    }
    for (const Obstacle &rock : world.rocks) {
        if (rock.z != 0) continue;
        float scale = 1.0f;
        models_rock_bounds(models, rock, scale);
        break;
    }
}

}  // namespace opra
