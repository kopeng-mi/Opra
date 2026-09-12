#include "game/scene.h"

#include <algorithm>
#include <cmath>

#include "core/log.h"
#include "game/config.h"
#include "game/effects.h"
#include "orbit/encounter.h"
#include "orbit/transfer.h"
#include "sim/atmosphere.h"
#include "sim/program.h"
#include "sim/survey.h"
#include "game/app.h"
#include "hud/hud.h"
#include "ui/draw.h"
#include "ui/screens.h"

namespace opra {

namespace {

/**
 * The one place a world position becomes a float: subtract the render origin in double first. At
 * 26 Gm a float holds no metres at all, so `float(world) - float(origin)` would quantise the ship
 * onto the same grid as the sun.
 */
glm::vec3 relative(const glm::dvec3 &origin, Real x, Real y, Real z) {
    return glm::vec3(static_cast<float>(x - origin.x), static_cast<float>(y - origin.y),
                     static_cast<float>(z - origin.z));
}

/**
 * This frame's particles as additive instances: one star cube each, so the whole effect layer is
 * one mesh and one draw. A particle is a function of (emission time, now) - nothing here smooths,
 * integrates or remembers - and a slot whose life is over is simply skipped, which is what makes
 * the drawn count bounded by the pool's own capacity.
 */
void add_effects(SceneBuilder &scene, const ModelSet &models, const Effects &effects,
                 const glm::dvec3 &origin, Real now) {
    for (int slot = 0; slot < effects.size(); ++slot) {
        const Effect &effect = effects.at(slot);
        if (effect_fade(effect, now) <= 0.0) continue;
        const float size = static_cast<float>(effect_size(effect, now));
        if (size <= 0.0f) continue;
        const Vec2 at = effect_position(effect, now);
        scene.add(models.star_mesh, relative(origin, at.x, at.y, 0.0),
                  spin_about_z(static_cast<float>(effect_angle(effect, now))), glm::vec3(size),
                  effect_color(effect, now), InstanceLayer::Effect);
    }
}

}  // namespace

void build_scene(SceneBuilder &scene, const ModelSet &models, const World &world,
                 const Backdrop &backdrop, const Camera &camera, float screen_width,
                 float screen_height) {
    int culled = 0;
    const glm::dvec3 origin = camera.origin;
    const ViewFrustum view = view_frustum(camera, 260.0f);
    const glm::mat4 view_proj = view_projection(camera);

    // Cross-check: anything the rect rejected that still projects inside the frame is a culling bug.
    const auto cull_miss = [&](Real x, Real y, float z, float radius) {
        const glm::vec2 at =
            project(view_proj, origin, x, y, z, screen_width, screen_height);
        if (at.x + radius < 0 || at.y + radius < 0 || at.x - radius > screen_width ||
            at.y - radius > screen_height) {
            return;
        }
        SDL_Log("cull missed: (%.0f, %.0f) r=%.0f z=%.0f projects to %.0f %.0f", x, y, radius, z,
                at.x, at.y);
    };

    // Stars, nebula, dust and motes. Placed from real depths, so parallax is whatever perspective
    // makes of them, and the grit folds around the ship instead of being left behind.
    add_backdrop(scene, backdrop, models, camera, world.elapsed, 260.0f);

    const auto add_rock = [&](const Obstacle &rock) {
        const float radius = static_cast<float>(rock.radius);
        float scale = 1.0f;
        const int mesh = models.rock_for(radius, rock.seed, scale);
        scene.add(mesh, relative(origin, rock.x, rock.y, rock.z),
                  spin_about_z(static_cast<float>(rock.seed) * 0.7f), glm::vec3(scale),
                  rock.z == 0 ? glm::vec3(1.0f) : glm::vec3(0.45f));
    };
    for (const Obstacle &rock : world.rocks) {
        if (rock.retired) continue;
        const glm::vec3 at = relative(origin, rock.x, rock.y, rock.z);
        if (!view.contains(at, static_cast<float>(rock.radius))) {
            ++culled;
            cull_miss(rock.x, rock.y, static_cast<float>(rock.z), static_cast<float>(rock.radius));
            continue;
        }
        add_rock(rock);
    }
    for (const Obstacle &fragment : world.fragments) {
        if (fragment.retired) continue;
        if (!view.contains(relative(origin, fragment.x, fragment.y, fragment.z),
                           static_cast<float>(fragment.radius))) {
            ++culled;
            continue;
        }
        add_rock(fragment);
    }

    // The station turns about its own origin; the ring and arms are its colliders too.
    scene.add_model(models.store.model("station"), relative(origin, STATION.x, STATION.y, 0.0),
                    spin_about_z(static_cast<float>(world.stationSpin)), 1.0f, false, 0.0f);
    scene.add_model(models.store.model("beacon"), relative(origin, RELAY.x, RELAY.y, 0.0),
                    glm::quat(1, 0, 0, 0), 1.0f, false, 0.0f);
    scene.add_model(models.store.model("derelict"), relative(origin, DERELICT.x, DERELICT.y, 0.0),
                    spin_about_z(0.35f), 1.0f, false, 0.0f);

    // One crate model serves both kinds: the black box is tinted and canted so it reads as the
    // odd one out without a second export.
    const glm::vec3 blackbox_tint(1.0f, 0.82f, 0.62f);
    for (const Cargo &cargo : world.cargos) {
        if (cargo.collected) continue;
        const glm::vec3 at = relative(origin, cargo.position.x, cargo.position.y, 0.0);
        if (!view.contains(at, 24.0f)) {
            ++culled;
            continue;
        }
        const bool blackbox = cargo.kind == CargoKind::Blackbox;
        float cant = static_cast<float>(cargo.position.x) * 0.01f;
        if (blackbox) cant += 1.0f;
        scene.add_model(models.store.model("cargo"), at, spin_about_z(cant), 1.0f, false, 0.0f,
                        blackbox ? blackbox_tint : glm::vec3(1.0f));
    }
    for (const Ore &chunk : world.ore) {
        const glm::vec3 at = relative(origin, chunk.x, chunk.y, 0.0);
        if (!view.contains(at, 12.0f)) {
            ++culled;
            continue;
        }
        scene.add_model(models.store.model("ore"), at,
                        spin_about_z(static_cast<float>(chunk.id) * 0.7f), 1.0f, false, 0.0f);
    }

    const int ship_class = static_cast<int>(world.ship.shipClass);
    const float thrust = std::max(0.0f, static_cast<float>(world.ship.thrustLevel));
    // The sim already solved the jets; the renderer only has to fade each cone on its own share.
    const glm::vec4 jets(static_cast<float>(world.ship.rcsJet[0]), static_cast<float>(world.ship.rcsJet[1]),
                         static_cast<float>(world.ship.rcsJet[2]), static_cast<float>(world.ship.rcsJet[3]));
    scene.add_model(models.store.model(SHIP_MODEL_NAMES[ship_class]),
                    relative(origin, world.ship.position.x, world.ship.position.y, 0.0),
                    spin_about_z(static_cast<float>(world.ship.angle)), config::SHIP_SCALE,
                    world.ship.thrustLevel > 0.02f, ui::clamp01(thrust / 1.65f), glm::vec3(1.0f),
                    jets);

    // Contact sparks, fracture dust and trails, the dock pulse: the sim's edges, as light. Every
    // one of them is placed relative to the camera's origin, like everything else here.
    add_effects(scene, models, world_effects(), origin, world.elapsed);
    scene.culled = culled;
}

orrery::Frame orrery_frame_for(const World &world, bool true_scale, int target_body) {
    orrery::Frame frame;
    frame.t = world.elapsed;
    frame.true_scale = true_scale;
    const size_t count = std::min(world.system.bodies.size(), world.bodies.size());
    frame.bodies.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        const Body &body = world.system.bodies[i];
        orrery::Body mark;
        mark.name = body.name;
        mark.position = world.bodies[i].position;
        mark.primary = body.parent >= 0 ? world.bodies[static_cast<size_t>(body.parent)].position
                                        : glm::dvec2(0.0);
        mark.elements = body.elements;
        mark.radius = body.radius;
        // The star is the body with no conic of its own; everything else rides a ring.
        if (body.parent < 0) mark.elements = orbit::Elements{};
        // The chart's ink: the star is warm, everything else is chart vellum (E11's two inks).
        mark.color = body.parent < 0 ? orrery::ink::STAR : orrery::ink::VELLUM;
        frame.bodies.push_back(mark);
    }
    if (!world.system.bodies.empty() && world.system.belt.parent >= 0) {
        const size_t parent = static_cast<size_t>(world.system.belt.parent);
        if (parent < world.bodies.size()) frame.belt_primary = world.bodies[parent].position;
    }
    frame.belt_inner = world.system.belt.inner;
    frame.belt_outer = world.system.belt.outer;
    frame.belt_count = world.system.belt.count;
    frame.belt_seed = world.system.belt.seed;
    frame.ship_position = world.system_position();
    frame.ship_heading = world.ship.angle;
    // The live transfer is the ship's own conic about its primary: the chart draws the orbit the
    // ship is actually on, not a decorative arc (plan 4.4).
    const size_t primary = static_cast<size_t>(world.primary < 0 ? 0 : world.primary);
    if (primary < world.system.bodies.size() && world.system.bodies[primary].mu > 0.0 &&
        !world.system.bodies.empty()) {
        frame.has_transfer = true;
        // With a plan in the book the drawn conic is the plan's prediction; without one it is the
        // orbit the ship is actually on (plan 4.4: the real conic, not a decorative arc).
        frame.transfer = world.nodes.empty()
                             ? orbit::from_state(frame.ship_position, world.system_velocity(),
                                                 world.system.bodies[primary].mu, frame.t)
                             : world.planned_conic();

        // The prediction (plan 3.6): the ship's conic walked forward through up to three SOI
        // crossings, so a gravity assist can be aimed instead of guessed at. The render layer draws
        // these; the maths is orbit/encounter's and the world is only read.
        const std::vector<orbit::EncounterLeg> legs =
            orbit::predict_encounters(world.system, world.bodies, frame.transfer, frame.t, 3);
        for (const orbit::EncounterLeg &leg : legs) {
            orrery::PredictedPath path;
            path.points = leg.points;
            path.body = leg.body;
            path.post = leg.post;
            // A leg after a crossing is an intention rather than the orbit the ship is on, and the
            // chart says so with the same dash the planner uses for a node.
            path.dashed = leg.post;
            if (leg.body >= 0 && static_cast<size_t>(leg.body) < world.system.bodies.size()) {
                path.soi = world.system.bodies[static_cast<size_t>(leg.body)].soi;
            }
            frame.predicted.push_back(std::move(path));
        }

        // Aerobraking is a consequence of the drag model, and what the map owes it is the shape of
        // the pass (plan 3.3): only a conic that dips into the air gets one.
        if (world.primary >= 0 && static_cast<size_t>(world.primary) < world.system.bodies.size()) {
            const Body &body = world.system.bodies[static_cast<size_t>(world.primary)];
            const SurfaceProfile *surface =
                static_cast<size_t>(world.primary) < world.surfaces.size()
                    ? &world.surfaces[static_cast<size_t>(world.primary)]
                    : nullptr;
            const glm::dvec2 relative = frame.ship_position - world.bodies[primary].position;
            const glm::dvec2 velocity = world.system_velocity() - world.bodies[primary].velocity;
            const PredictedPass pass = predict_pass(body, surface, relative, velocity, body.mu,
                                                   DRAG_CD_AREA_OVER_MASS, NOSE_RADIUS);
            if (pass.enters) {
                orrery::PredictedPath path;
                path.post = true;
                path.dashed = true;
                path.body = world.primary;
                // The pass is in the body's frame; the chart draws about the frame origin, so every
                // point carries the body's own position with it.
                path.points.reserve(pass.points.size());
                for (const glm::dvec2 &at : pass.points) {
                    path.points.push_back(world.bodies[primary].position + at);
                }
                frame.predicted.push_back(std::move(path));
            }
        }
    }

    // The almanac's second block: the burn to the selected body, as the ephemeris would print it.
    // Two circular orbits, which is what a Hohmann is: from the ship's own radius to the target's
    // semi-major axis. The ship's orbit here is near-circular by construction - the zone co-orbits
    // Wayfarer - so the number is the transfer's, not an idealisation of it.
    if (target_body >= 0 && static_cast<size_t>(target_body) < count &&
        world.system.bodies[0].mu > 0.0) {
        const Body &body = world.system.bodies[static_cast<size_t>(target_body)];
        const double mu = world.system.bodies[0].mu;
        const double from_radius = glm::length(frame.ship_position);
        const double to_radius = body.elements.a;
        const orbit::Hohmann plan = orbit::hohmann(from_radius, to_radius, mu);
        const double ship_motion = std::sqrt(mu / (from_radius * from_radius * from_radius));
        const double target_motion = std::sqrt(mu / (to_radius * to_radius * to_radius));
        const double phase_now = std::atan2(frame.ship_position.y, frame.ship_position.x) -
                                 std::atan2(world.bodies[static_cast<size_t>(target_body)].position.y,
                                            world.bodies[static_cast<size_t>(target_body)].position.x);
        frame.target.has_target = true;
        frame.target.name = body.name;
        frame.target.ship = world.ship.spec ? world.ship.spec->name : "";
        frame.target.r = from_radius;
        // The almanac's `v` is the ship's speed in the system, not its 30 m/s crawl inside the
        // zone: the transfer is flown against the star, so that is the number that matters.
        frame.target.speed = glm::length(world.system_velocity());
        frame.target.dv = plan.dv_total;
        frame.target.arrival = plan.transfer_time;
        frame.target.window = orbit::time_to_window(phase_now, orbit::phase_angle_required(target_motion, plan.transfer_time),
                                                    ship_motion, target_motion);
        frame.target.from = world.system.bodies[primary].name;
        frame.target.to = body.name;

        // R-8: the arc the label promises. It is the transfer ellipse itself, not the orbit the ship
        // happens to be on: periapsis where the ship is now, apoapsis at the target's orbit, so the
        // drawn conic runs from one ring to the other exactly as the window countdown implies.
        if (to_radius > from_radius) {
            orbit::Elements arc;
            arc.a = plan.a_transfer;
            arc.e = (to_radius - from_radius) / (to_radius + from_radius);
            // The ellipse's line of apsides points at the ship: the burn happens where the ship is.
            arc.omega = std::atan2(frame.ship_position.y, frame.ship_position.x);
            arc.M0 = 0.0;  // at periapsis, which is the departure
            arc.t0 = frame.t;
            arc.mu = mu;
            frame.has_transfer = true;
            frame.transfer = arc;
        }
    }
    return frame;
}

ui::ChartFrame chart_frame_for(const World &world) {
    ui::ChartFrame frame;
    frame.bounds_min = glm::vec2(static_cast<float>(SECTOR.minX), static_cast<float>(SECTOR.minY));
    frame.bounds_max = glm::vec2(static_cast<float>(SECTOR.maxX), static_cast<float>(SECTOR.maxY));
    for (const Obstacle &rock : world.rocks) {
        if (rock.z != 0) continue;
        frame.rocks.push_back({glm::vec2(static_cast<float>(rock.x), static_cast<float>(rock.y)),
                               static_cast<float>(rock.radius)});
    }
    for (const Obstacle &fragment : world.fragments) {
        frame.fragments.push_back(
            {glm::vec2(static_cast<float>(fragment.x), static_cast<float>(fragment.y)),
             static_cast<float>(fragment.radius)});
    }
    for (const Ore &chunk : world.ore) {
        frame.ore.push_back({glm::vec2(static_cast<float>(chunk.x), static_cast<float>(chunk.y)),
                             static_cast<float>(chunk.amount)});
    }
    for (const Cargo &cargo : world.cargos) {
        if (cargo.collected) continue;
        frame.marks.push_back({cargo.name,
                               glm::vec2(static_cast<float>(cargo.position.x),
                                         static_cast<float>(cargo.position.y)),
                               ui::ChartFrame::Mark::Kind::Cargo});
    }
    frame.marks.push_back({"Wayfarer",
                           glm::vec2(static_cast<float>(STATION.x), static_cast<float>(STATION.y)),
                           ui::ChartFrame::Mark::Kind::Station});
    frame.marks.push_back({"Relay",
                           glm::vec2(static_cast<float>(RELAY.x), static_cast<float>(RELAY.y)),
                           ui::ChartFrame::Mark::Kind::Relay});
    frame.marks.push_back({"Kite's End",
                           glm::vec2(static_cast<float>(DERELICT.x), static_cast<float>(DERELICT.y)),
                           ui::ChartFrame::Mark::Kind::Derelict});
    frame.ship = glm::vec2(static_cast<float>(world.ship.position.x),
                           static_cast<float>(world.ship.position.y));
    frame.heading = static_cast<float>(world.ship.angle);
    const Real speed = length(world.ship.velocity);
    if (speed > 1.0) {
        frame.velocity = glm::vec2(static_cast<float>(world.ship.velocity.x / speed),
                                   -static_cast<float>(world.ship.velocity.y / speed));
    }
    return frame;
}


namespace {

/**
 * The orbit block's forced window (plan 4.6): an SOI change forces it on for ten seconds. App
 * carries the sim and is not this phase's to grow, so the HUD's own frame-to-frame memory lives
 * here; the clock it reads is the world's, which keeps a capture deterministic.
 */
double orbit_forced_until = -1.0;

/** Seconds from now to a mean anomaly of this conic, wrapped into one period. */
double to_anomaly(const orbit::Elements &elements, double elapsed, double target) {
    constexpr double TAU = 6.283185307179586;
    const double n = orbit::mean_motion(elements);
    if (!(n > 0.0)) return 0.0;
    double delta = std::fmod(target - (elements.M0 + n * (elapsed - elements.t0)), TAU);
    if (delta < 0.0) delta += TAU;
    return delta / n;
}

/**
 * The program the flight is in, from the world's own state: the director's context (plan 4.6).
 * The order is the order of the job - a berth in range is the approach, a planned node is the
 * pilot's own instruction, and a body with a surface under a conic that will not clear it is a
 * descent.
 */
Program context_program(const World &world, const Body *primary, const orbit::Elements &conic,
                        double radius) {
    if (world.target_port >= 0 && !world.station_ports.empty()) return Program::Dock;
    if (!world.nodes.empty()) return Program::Transfer;
    if (primary && primary->terrain.present && !primary->terrain.pads.empty() && conic.a > 0.0) {
        if (primary->atmosphere.present && radius - primary->radius < primary->atmosphere.top) {
            return Program::Land;
        }
        if (orbit::periapsis(conic) < primary->radius) return Program::Deorbit;
    }
    return Program::Circularize;
}

}  // namespace

opra::HudFrame make_hud_frame(App &app, Uint32 width, Uint32 height) {
    World &world = app.world;
    opra::HudFrame frame;
    frame.screen = glm::vec2(static_cast<float>(width), static_cast<float>(height));
    // F1: the camera is no longer on the ship, so the collar's centre is a projection and every
    // consumer reads this field instead of doing the arithmetic. `project` takes the origin in
    // double and returns pixels, which is the only way the subtraction stays exact.
    const glm::mat4 view = view_projection(app.camera);
    frame.shipScreen = project(view, app.camera.origin, world.ship.position.x,
                               world.ship.position.y, 0.0f, frame.screen.x, frame.screen.y);
    // A guard, not a layout: if the ship is not in this camera's frame at all - the follow has not
    // caught up, or another camera is flying - the numbers coming out of the projection are metres
    // per pixel away from anything the HUD can draw. Clamping them keeps every consumer's
    // arithmetic finite; where the collar should go when the ship is off screen is a framing
    // decision (G2), not an arithmetic one.
    if (!std::isfinite(frame.shipScreen.x) || !std::isfinite(frame.shipScreen.y)) {
        frame.shipScreen = frame.screen * 0.5f;
    }
    const glm::vec2 limit = frame.screen * 2.0f;
    frame.shipScreen = glm::clamp(frame.shipScreen, -limit, limit);
    frame.shipRadiusPx = static_cast<float>(world.ship.bounds.halfLength) *
                         (static_cast<float>(height) / (2.0f * app.camera.half_height));
    frame.thrust = static_cast<float>(world.ship.thrustLevel);
    frame.heat = static_cast<float>(world.ship.heat);
    frame.time = static_cast<float>(world.elapsed);
    frame.hideCollar = app.screen == ui::Screen::Chart || app.screen == ui::Screen::Manual ||
                       app.cinematic;
    frame.sessionSeconds = static_cast<float>(world.elapsed);
    frame.status = app.screen != ui::Screen::Flight
                       ? opra::StatusDot::Paused
                       : (world.ship.hull < world.ship.spec->hull * 0.5
                              ? opra::StatusDot::UnderFire
                              : opra::StatusDot::Nominal);
    frame.chartOpen = app.screen == ui::Screen::Chart;
    frame.zoom = app.zoom_current;
    // F10: the density the pilot chose, the context that overrides it, and the pass's switch.
    frame.density = app.density;
    frame.forceGuns = app.mining;
    frame.debug = app.debug;
    frame.warpRate = app.warp.rate();
    // The world under the ship (plan 3.2-3.7): the air, the ground, the pad's base, the survey and
    // the satellites. All of it is the world's own state - the HUD computes none of it.
    frame.airDensity = world.air_density;
    frame.airFlux = world.air_flux;
    frame.altitude = world.air_altitude;
    frame.landed = world.landed_body >= 0;
    frame.baseName = world.base_name.empty() ? nullptr : world.base_name.c_str();
    if (world.primary >= 0 && static_cast<size_t>(world.primary) < world.system.bodies.size() &&
        static_cast<size_t>(world.primary) < world.bodies.size()) {
        const Body &body = world.system.bodies[static_cast<size_t>(world.primary)];
        frame.worldsValid = body.terrain.present || body.atmosphere.present;
        frame.worldsBody = body.name.c_str();
        const glm::dvec2 relative =
            world.system_position() - world.bodies[static_cast<size_t>(world.primary)].position;
        const glm::dvec2 velocity =
            world.system_velocity() - world.bodies[static_cast<size_t>(world.primary)].velocity;
        const double radius = glm::length(relative);
        // The rate of descent is the radial component: closing is positive, which is the sign the
        // touchdown gate and the hoverslam both read (plan 3.4).
        frame.descentRate =
            radius > 1.0 ? -(velocity.x * relative.x + velocity.y * relative.y) / radius : 0.0;
        const double mapped = surveyed_radians(world, world.primary);
        frame.surveyFraction = mapped > 0.0 ? std::min(1.0, mapped / (2.0 * 3.141592653589793)) : 0.0;
    }
    for (const Satellite &satellite : world.satellites) {
        if (satellite.checked && satellite.valid) ++frame.satellitesUp;
        if (!satellite.checked) ++frame.satellitesPending;
    }

    frame.contractId = "SR-084";
    frame.objective = "Resolve and recover";
    int recovered = 0;
    for (const opra::Cargo &cargo : world.cargos)
        if (cargo.collected) ++recovered;
    frame.progress = static_cast<float>(recovered) / static_cast<float>(world.cargos.size());

    frame.shipName = world.ship.spec->name;
    frame.hullFrac = static_cast<float>(world.ship.hull / world.ship.spec->hull);
    frame.fuelFrac = static_cast<float>(world.ship.fuel / world.ship.spec->fuel);
    frame.heatFrac = static_cast<float>(world.ship.heat);
    frame.hullValue = static_cast<float>(world.ship.hull);
    frame.fuelValue = static_cast<float>(world.ship.fuel);
    frame.heatValue = static_cast<float>(world.ship.heat * 100.0f);
    frame.assist = world.ship.assist;
    frame.braking = app.input.held(SDL_SCANCODE_X);
    frame.gunCount = 2;
    frame.gunsReady = app.mining ? 1 : 2;
    frame.gunsHot = world.ship.heat > 0.9;
    frame.oreHeld = world.oreHeld;
    frame.accelerationG = world.ship.acceleration / 9.80665;
    frame.headingDeg = opra::heading(world.ship.angle);
    frame.speed = opra::length(world.ship.velocity);
    // Screen space is y-down and the world is y-up, so the y components flip here and nowhere else.
    frame.noseDir = glm::vec2(static_cast<float>(-std::sin(world.ship.angle)),
                              static_cast<float>(-std::cos(world.ship.angle)));
    if (frame.speed > 0.5) {
        frame.velocityDir =
            glm::vec2(static_cast<float>(world.ship.velocity.x / frame.speed),
                      static_cast<float>(-world.ship.velocity.y / frame.speed));
    }
    frame.wrecked = world.ship.hull <= 0;

    const Real speed = opra::length(world.ship.velocity);
    if (speed > 0.5) {
        frame.marks.push_back({static_cast<float>(std::atan2(world.ship.velocity.y,
                                                             world.ship.velocity.x)),
                               static_cast<float>(speed), opra::MarkKind::Velocity, 1.0f});
    }
    for (const opra::Cargo &cargo : world.cargos) {
        if (cargo.collected) continue;
        const Real dx = cargo.position.x - world.ship.position.x;
        const Real dy = cargo.position.y - world.ship.position.y;
        const Real range = std::hypot(dx, dy);
        frame.marks.push_back({static_cast<float>(std::atan2(dy, dx)), static_cast<float>(range),
                               opra::MarkKind::Contact,
                               static_cast<float>(std::max<Real>(0.15, 1.0 - range / 2000.0))});
    }
    for (const opra::Ore &chunk : world.ore) {
        const Real dx = chunk.x - world.ship.position.x;
        const Real dy = chunk.y - world.ship.position.y;
        const Real range = std::hypot(dx, dy);
        if (range > 900) continue;
        frame.marks.push_back({static_cast<float>(std::atan2(dy, dx)), static_cast<float>(range),
                               opra::MarkKind::Ore, 1.0f});
    }
    std::vector<Contact> contacts = world.contacts();
    const int tracked = contact_index(contacts, world.target);
    if (tracked >= 0) {
        const Contact &contact = contacts[static_cast<size_t>(tracked)];
        const Real dx = contact.position.x - world.ship.position.x;
        const Real dy = contact.position.y - world.ship.position.y;
        const Real range = std::hypot(dx, dy);
        frame.marks.push_back({static_cast<float>(std::atan2(dy, dx)), static_cast<float>(range),
                               opra::MarkKind::Target, 1.0f});
    }

    if (app.screen != ui::Screen::Flight) {
        frame.context = "paused  Esc to resume";
        frame.contextColor = ui::tokens::DRIVE;
    } else if (world.docked) {
        frame.context = "hard dock - Wayfarer  R";
        frame.contextColor = ui::tokens::NAV;
    } else if (app.mining) {
        frame.context = "cutter engaged";
        frame.contextColor = ui::tokens::DRIVE;
    } else if (opra::can_dock(world.ship)) {
        frame.context = "dock ready  R";
        frame.contextColor = ui::tokens::NAV;
    } else {
        for (const opra::Cargo &cargo : world.cargos) {
            if (!cargo.collected && opra::can_recover(world.ship, cargo)) {
                frame.context = "recover  R";
                frame.contextColor = ui::tokens::NAV;
                break;
            }
        }
    }

    // ---- the instrument blocks (G8, G9): the conic, the director's cue and the minimap's own
    // contents, read out of the world here so the HUD layer never sees one.
    const size_t chief = static_cast<size_t>(world.primary < 0 ? 0 : world.primary);
    const Body *primary = nullptr;
    glm::dvec2 primary_r{0.0};
    if (chief < world.system.bodies.size() && chief < world.bodies.size() &&
        world.system.bodies[chief].mu > 0.0) {
        primary = &world.system.bodies[chief];
        primary_r = world.system_position() - world.bodies[chief].position;
    }
    const double radius = glm::length(primary_r);
    const orbit::Elements conic = world.planned_conic();
    if (primary && conic.mu > 0.0 && conic.a != 0.0) {
        frame.orbit.valid = true;
        frame.orbit.body = primary->name.c_str();
        frame.orbit.altitude = radius - primary->radius;
        frame.orbit.eccentricity = conic.e;
        frame.orbit.elliptic = orbit::is_elliptic(conic);
        frame.orbit.periapsis = orbit::periapsis(conic) - primary->radius;
        frame.orbit.apoapsis = frame.orbit.elliptic ? orbit::apoapsis(conic) - primary->radius : 0.0;
        frame.orbit.period = frame.orbit.elliptic ? orbit::period(conic) : 0.0;
        for (const orbit::Node &node : world.nodes) {
            frame.orbit.dvPlanned += std::hypot(node.prograde, node.radial);
        }
        frame.orbit.toPeriapsis = to_anomaly(conic, world.elapsed, 0.0);
        frame.orbit.toApoapsis = to_anomaly(conic, world.elapsed, 3.14159265358979323846);
        // The context's one timed override (plan 4.6): an SOI change is the moment the conic moves
        // under the ship, so the orbit block stays up while the pilot takes it in.
        if (world.soi_switched) orbit_forced_until = world.elapsed + 10.0;
        frame.forceOrbit = world.elapsed < orbit_forced_until;
    }

    // The director (F9, plan 3.8): the context picks the program, the program solves it, the pilot
    // flies it. Nothing here writes to the world - `evaluate` cannot.
    const Program program = context_program(world, primary, conic, radius);
    const Cue cue = evaluate(program, world, world.elapsed);
    frame.director.active = cue.active;
    if (cue.active) {
        frame.director.program = program_name(program);
        frame.director.target = program_target(program, world);
        frame.director.heading = cue.heading;
        frame.director.throttle = cue.throttle;
        frame.director.actual = world.ship.thrustLevel;
        frame.director.burnIn = cue.burn_in;
        frame.director.burnFor = cue.burn_for;
        frame.director.dvRemaining = cue.dv_remaining;
        frame.director.burn = cue.burn_in != 0.0 || cue.burn_for > 0.0;
        frame.director.step = cue.step;
        for (const GateTerm &gate : cue.gates) {
            frame.director.gates.push_back({gate.name, gate.value, gate.limit, gate.unit, gate.ok});
        }
        frame.forceProgram = !frame.director.all_ok();
    }

    // The minimap (F5): the collar's own marks by bearing and range, the conic it shares with the
    // orbit block, and the corridor's geometry while a berth is in range.
    bool contact_in_range = false;
    for (const CollarMark &mark : frame.marks) {
        if (mark.kind == MarkKind::Velocity) continue;
        frame.minimap.blips.push_back({mark.bearing, mark.range, mark.kind == MarkKind::Target});
        if (mark.range <= MINIMAP_RANGE) contact_in_range = true;
    }
    const bool berth_in_range = world.target_port >= 0 && !world.station_ports.empty();
    frame.minimap.active = true;
    frame.minimap.mode = resolve_minimap_mode(berth_in_range, contact_in_range);
    frame.minimap.primary = frame.orbit.body;
    frame.minimap.altitude = frame.orbit.altitude;
    frame.minimap.periapsis = frame.orbit.periapsis;
    frame.minimap.apoapsis = frame.orbit.apoapsis;
    frame.minimap.elliptic = frame.orbit.elliptic;
    if (berth_in_range) {
        frame.minimap.berthOffset = glm::vec2(
            static_cast<float>(world.ship_port_state.position.x - world.target_state.position.x),
            static_cast<float>(world.ship_port_state.position.y - world.target_state.position.y));
        frame.minimap.berthBearing = static_cast<float>(
            std::atan2(world.target_state.normal.y, world.target_state.normal.x));
        frame.minimap.axial = static_cast<float>(world.gate.axial);
        frame.minimap.lateral = static_cast<float>(world.gate.lateral);
        frame.minimap.closing = static_cast<float>(world.gate.closing);
        frame.minimap.lateralLimit = static_cast<float>(GateLimits{}.lateral_max);
    }

    return frame;
}

}  // namespace opra
