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

/**
 * The model a level draws with. The bold model is the authored one; the blocky LOD is its
 * silhouette-extruded sibling under `<name>_blocky` when the manifest carries one, and the
 * detailed tier falls back to the bold model until an asset names the extra export.
 */
const Model &lod_model(const ModelSet &models, const std::string &name, int level) {
    if (level <= 1) {
        const std::string blocky = name + "_blocky";
        for (const std::string &candidate : models.store.names()) {
            if (candidate == blocky) return models.store.model(blocky);
        }
    }
    return models.store.model(name);
}

/**
 * A hex colour like "#ff9c5c" as a tint, or white when it does not parse: the system file carries
 * each body's albedo tint, and a body without a map still deserves its own hue.
 */
glm::vec3 tint_from_hex(const std::string &text) {
    if (text.size() != 7 || text[0] != '#') return {1.0f, 1.0f, 1.0f};
    const auto nibble = [](char c) -> int {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
        return -1;
    };
    const int r = nibble(text[1]) * 16 + nibble(text[2]);
    const int g = nibble(text[3]) * 16 + nibble(text[4]);
    const int b = nibble(text[5]) * 16 + nibble(text[6]);
    if (r < 0 || g < 0 || b < 0) return {1.0f, 1.0f, 1.0f};
    return {static_cast<float>(r) / 255.0f, static_cast<float>(g) / 255.0f,
            static_cast<float>(b) / 255.0f};
}

/**
 * The system's own bodies, drawn at real scale (plan 05 s2.3). Anything farther than the far plane
 * is re-projected onto a shell just inside it, in double here and cast to float exactly once:
 *
 *   dir = normalize(p - eye),  d' = far * 0.9,  p' = eye + dir * d',  r' = r * d' / d
 *
 * r'/d' = r/d, so the angular radius is unchanged and so is the direction - a body crossing `far`
 * does not move and does not change size, by construction rather than by tuning. The follow body
 * sits at the render origin, where d is zero; it is the one body never drawn (s2.8).
 */
void add_system_bodies(SceneBuilder &scene, const World &world, const glm::dvec3 &origin,
                       float far_z, int models_planet_mesh, float viewport_height) {
    for (size_t i = 0; i < world.system.bodies.size() && i < world.bodies.size(); ++i) {
        const Body &def = world.system.bodies[i];
        // A body that names a model is a structure - Wayfarer, the zone's own anchor - and the
        // sector's dressing draws it as that model. Through the planet pipeline it would be a
        // 60 m tan sphere sitting on the ship's spawn, which is not a sky, it is a bug.
        if (!def.model.empty()) continue;
        // The zone frame: barycentric minus the anchor, which is the frame `origin` lives in.
        const glm::dvec2 at_zone = world.body_zone_position(static_cast<int>(i));
        const glm::dvec3 offset(at_zone.x - origin.x, at_zone.y - origin.y, 0.0);
        const double d = glm::length(offset);
        // s2.8's degenerate guard, taken literally: a body the camera is *inside* is not drawable
        // (the eye is within its own sphere). A body the camera frames from outside - a
        // double-click at 2.5 radii - is exactly the thing to draw, and skipping it would blank
        // the one view the pilot asked for.
        if (d <= static_cast<double>(def.radius) * 1.05) continue;

        const bool deep = d > static_cast<double>(far_z);
        const double d_drawn = deep ? static_cast<double>(far_z) * 0.9 : d;
        const double r_drawn = static_cast<double>(def.radius) * d_drawn / d;
        const glm::dvec3 drawn = deep ? offset * (d_drawn / d) : offset;

        // s2.5: under eight pixels across, the overlay's icon is the honest mark and the geometry
        // stands down. The pixel arithmetic is s2.5's own formula, here against the true distance.
        const double px_diameter = 2.0 * static_cast<double>(def.radius) / d *
                                   (static_cast<double>(viewport_height) * 0.5) /
                                   std::tan(static_cast<double>(CAMERA_FOV_Y) * 0.5);
        if (px_diameter < 8.0) continue;

        SkyBody sky;
        sky.mesh = models_planet_mesh;
        sky.center = glm::vec3(drawn);  // the one float cast, at the end, in the scene builder
        sky.radius = static_cast<float>(r_drawn);
        sky.deep = deep;
        sky.color = tint_from_hex(def.tint);
        sky.terrain_seed = static_cast<float>(def.terrain.seed);
        sky.terrain_amplitude = static_cast<float>(def.terrain.amplitude);
        sky.scale_height = def.atmosphere.present
                               ? static_cast<float>(def.atmosphere.scale_height / def.radius)
                               : 0.0f;
        sky.atmosphere_top = def.atmosphere.present
                                 ? static_cast<float>(def.atmosphere.top / def.radius)
                                 : 0.0f;
        sky.albedo_map = def.albedo_map;
        sky.cloud_map = def.cloud_map;
        sky.night_map = def.night_map;
        sky.photosphere_map = def.photosphere_map;
        sky.kind = def.kind == BodyClass::Star ? BodyKind::Star : BodyKind::Planet;
        scene.bodies.push_back(sky);
    }
}

}  // namespace

/**
 * s2.5's hysteresis: the deadband is the whole point. A crossfade would need a blend pipeline and
 * a second draw per object, for an effect nobody sees at a ten percent deadband.
 */
int lod_level(float px, int previous) {
    const auto stays = [](float value, float lo, float hi) { return value >= lo && value <= hi; };
    switch (previous) {
        case 3:
            if (stays(px, 250.0f, 275.0f)) return 3;  // detailed holds to +10%
            break;
        case 2:
            if (stays(px, 54.0f, 275.0f)) return 2;   // bold holds from 54 to 275
            break;
        case 1:
            if (stays(px, 7.2f, 66.0f)) return 1;     // blocky holds from 7.2 to 66
            break;
        default:
            break;
    }
    return px > 250.0f ? 3 : px >= 60.0f ? 2 : px >= 8.0f ? 1 : 0;
}


void build_scene(SceneBuilder &scene, const ModelSet &models, const World &world,
                 const Backdrop &backdrop, const Camera &camera, float screen_width,
                 float screen_height, std::unordered_map<unsigned long long, int> &lod_memory) {
    int culled = 0;
    const glm::dvec3 origin = camera.origin;
    const ViewFrustum view = view_frustum(camera, 260.0f);
    const glm::mat4 view_proj = view_projection(camera);

    // The system's own bodies (plan 05 s2.3): the sky is never empty, at any zoom. Anything beyond
    // the far plane is re-projected exactly, so the flight view at hull scale still carries every
    // planet at its true bearing and angular size.
    add_system_bodies(scene, world, origin, camera.far_z(), models.planet_mesh, screen_height);

    // s2.5's selection, per drawn object: on-screen size decides, never distance, and s2.8's
    // assert falls out - inside the near plane an object is an icon, because its px is tiny.
    const auto object_px = [&](Real x, Real y, Real radius) {
        const double dx = x - origin.x, dy = y - origin.y;
        const double d = std::max(std::sqrt(dx * dx + dy * dy), 1.0);
        return 2.0 * static_cast<double>(radius) / d *
               (static_cast<double>(screen_height) * 0.5) /
               std::tan(static_cast<double>(CAMERA_FOV_Y) * 0.5);
    };
    const auto lod_key = [](unsigned long long id) { return id; };
    const auto select = [&](unsigned long long id, Real x, Real y, Real radius) {
        const float px = static_cast<float>(object_px(x, y, radius));
        const int previous = lod_memory.count(lod_key(id)) ? lod_memory[lod_key(id)] : -1;
        const int level = lod_level(px, previous < 0 ? -1 : previous);
        lod_memory[lod_key(id)] = level;
        return level;
    };
    // The key light rides the star: the direction from the render origin toward the star, which at
    // hull scale is a bearing and at system scale is the geometry the sun shader draws.
    if (!world.system.bodies.empty() && !world.bodies.empty()) {
        const glm::dvec2 to_star = world.body_zone_position(0) - glm::dvec2(origin.x, origin.y);
        const double reach = glm::length(to_star);
        if (reach > 1.0) {
            const glm::vec3 scene_light_tint = tint_from_hex(world.system.bodies[0].tint);
            if (SDL_getenv("OPRA_DEBUG_SKY")) {
                SDL_Log("light: to_star=(%.3e, %.3e) tint=(%.2f, %.2f, %.2f)", to_star.x, to_star.y,
                        scene_light_tint.r, scene_light_tint.g, scene_light_tint.b);
            }
            scene.light.direction_to_star =
                glm::vec3(static_cast<float>(to_star.x / reach), static_cast<float>(to_star.y / reach),
                          0.35f);
            scene.light.color = scene_light_tint;
            scene.light.intensity = 1.0f;
        }
    }

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
                  rock.z == 0 ? glm::vec3(1.0f) : glm::vec3(0.22f));
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

    // The station turns about its own origin; the ring and arms are its colliders too. Each
    // structure's LOD rides its own pixel size; under eight px the overlay's icon owns it.
    const auto draw_structure = [&](unsigned long long id, const std::string &name, Real x, Real y,
                                    Real radius, const glm::quat &spin) {
        const int level = select(id, x, y, radius);
        if (level < 1) return;  // icon: geometry stands down (s2.5)
        scene.add_model(lod_model(models, name, level), relative(origin, x, y, 0.0), spin, 1.0f,
                        false, 0.0f);
    };
    draw_structure(1, "station", STATION.x, STATION.y, 78.0,
                   spin_about_z(static_cast<float>(world.stationSpin)));
    draw_structure(2, "beacon", RELAY.x, RELAY.y, 20.0, glm::quat(1, 0, 0, 0));
    draw_structure(3, "derelict", DERELICT.x, DERELICT.y, 41.0, spin_about_z(0.35f));

    // One crate model serves both kinds: the black box is tinted and canted so it reads as the
    // odd one out without a second export.
    const glm::vec3 blackbox_tint(1.0f, 0.82f, 0.62f);
    int cargo_index = 0;
    for (const Cargo &cargo : world.cargos) {
        ++cargo_index;
        if (cargo.collected) continue;
        const glm::vec3 at = relative(origin, cargo.position.x, cargo.position.y, 0.0);
        if (!view.contains(at, 24.0f)) {
            ++culled;
            continue;
        }
        const bool blackbox = cargo.kind == CargoKind::Blackbox;
        float cant = static_cast<float>(cargo.position.x) * 0.01f;
        if (blackbox) cant += 1.0f;
        if (select(1000 + cargo_index, cargo.position.x, cargo.position.y, 12.0) >= 1) {
            scene.add_model(lod_model(models, "cargo", 2), at, spin_about_z(cant), 1.0f, false, 0.0f,
                            blackbox ? blackbox_tint : glm::vec3(1.0f));
        }
    }
    for (const Ore &chunk : world.ore) {
        const glm::vec3 at = relative(origin, chunk.x, chunk.y, 0.0);
        if (!view.contains(at, 12.0f)) {
            ++culled;
            continue;
        }
        if (select(2000 + static_cast<unsigned long long>(chunk.id), chunk.x, chunk.y, 7.0) >= 1) {
            scene.add_model(lod_model(models, "ore", 2), at,
                            spin_about_z(static_cast<float>(chunk.id) * 0.7f), 1.0f, false, 0.0f);
        }
    }

    const int ship_class = static_cast<int>(world.ship.shipClass);
    const float thrust = std::max(0.0f, static_cast<float>(world.ship.thrustLevel));
    // The sim already solved the jets; the renderer only has to fade each cone on its own share.
    const glm::vec4 jets(static_cast<float>(world.ship.rcsJet[0]), static_cast<float>(world.ship.rcsJet[1]),
                         static_cast<float>(world.ship.rcsJet[2]), static_cast<float>(world.ship.rcsJet[3]));
    const std::string ship_model = SHIP_MODEL_NAMES[ship_class];
    // The own ship follows the same rule as everything else (s2.5): under eight px the chevron in
    // the overlay is the honest mark and the hull stands down.
    if (select(0, world.ship.position.x, world.ship.position.y,
               static_cast<Real>(world.ship.bounds.halfLength)) >= 1) {
        scene.add_model(lod_model(models, ship_model, 2),
                        relative(origin, world.ship.position.x, world.ship.position.y, 0.0),
                        spin_about_z(static_cast<float>(world.ship.angle)), config::SHIP_SCALE,
                        world.ship.thrustLevel > 0.02f, ui::clamp01(thrust / 1.65f), glm::vec3(1.0f),
                        jets);
    }

    // Contact sparks, fracture dust and trails, the dock pulse: the sim's edges, as light. Every
    // one of them is placed relative to the camera's origin, like everything else here.
    add_effects(scene, models, world_effects(), origin, world.elapsed);
    scene.culled = culled;
}

orrery::Frame orrery_frame_for(const World &world, int target_body) {
    orrery::Frame frame;
    frame.t = world.elapsed;
    frame.true_scale = false;  // the plate keeps the glyph floor: honest sizes live in the flight view
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
        mark.albedo_map = body.albedo_map;
        mark.cloud_map = body.cloud_map;
        mark.night_map = body.night_map;
        mark.photosphere_map = body.photosphere_map;
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

/** The zoom scale the warp suggestion reads (s2.7): the camera's own half-height. */
double camera_half_for_suggestion(App &app) { return app.half_height_current; }

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

// ------------------------------------------------------------------ the zoom overlay (s2.4, s2.6)

/**
 * World (double) to screen pixels, in the camera's own basis. Points behind the eye report false
 * and are never projected: a polyline clips its segments against the near plane in eye space
 * instead, because dropping a sample leaves a visible gap in an orbit that passes behind the
 * camera (s2.4). The s2.8 rule about non-positive clip w is this test.
 */
struct ScreenProjector {
    glm::dvec3 eye{0.0};
    glm::dvec3 forward{0.0, 1.0, 0.0};
    glm::dvec3 right{1.0, 0.0, 0.0};
    glm::dvec3 up{0.0, 0.0, 1.0};
    double tan_x = 1.0;
    double tan_y = 1.0;
    double width = 1.0;
    double height = 1.0;
    /** Anything closer to the eye plane than this is behind, for both the test and the clip. */
    double near_eps = 1.0e-6;

    static ScreenProjector for_camera(const Camera &camera, float w, float h) {
        ScreenProjector out;
        out.eye = camera.origin + glm::dvec3(camera.eye);
        const glm::dvec3 forward =
            glm::normalize(glm::dvec3(camera.target) - glm::dvec3(camera.eye));
        out.forward = forward;
        out.right = glm::normalize(glm::cross(forward, glm::dvec3(0.0, 1.0, 0.0)));
        out.up = glm::cross(out.right, forward);
        out.tan_y = std::tan(static_cast<double>(CAMERA_FOV_Y) * 0.5);
        out.tan_x = out.tan_y * static_cast<double>(camera.aspect);
        out.width = w;
        out.height = h;
        return out;
    }

    /** True unless the point is behind the eye plane. Double in, pixels out. */
    bool to_screen(const glm::dvec3 &world, glm::dvec2 &out_px) const {
        const glm::dvec3 v = world - eye;
        const double z = glm::dot(v, forward);
        if (z <= near_eps) return false;
        const double x = glm::dot(v, right);
        const double y = glm::dot(v, up);
        out_px = glm::dvec2((x / (z * tan_x) * 0.5 + 0.5) * width,
                            (0.5 - y / (z * tan_y) * 0.5) * height);
        return true;
    }

    /** Eye-plane distance, for the near clip. */
    double depth(const glm::dvec3 &world) const { return glm::dot(world - eye, forward); }
};

/** One polyline sample: the world point and, when in front, where it lands. */
struct PathSample {
    glm::dvec3 world{0.0};
    glm::dvec2 screen{0.0};
    bool front = false;
};

/**
 * Walks a sampled polyline into the UI batch: segments behind the eye are clipped against the near
 * plane in eye space (never dropped - s2.4), and the rest draw as ordinary UI marks with the depth
 * test off: the overlay pass.
 */
void draw_polyline(UIBatch &ui, const ScreenProjector &projector,
                   const std::vector<PathSample> &path, const glm::vec4 &color, float thickness) {
    const double w = projector.width, h = projector.height;
    for (size_t i = 0; i + 1 < path.size(); ++i) {
        glm::dvec3 a = path[i].world;
        glm::dvec3 b = path[i + 1].world;
        const bool a_front = path[i].front;
        const bool b_front = path[i + 1].front;
        if (!a_front && !b_front) continue;  // the whole segment is behind the eye
        if (!a_front || !b_front) {
            // Clip in eye space against the near plane: the crossing point is exact, not an
            // extrapolated screen-space guess.
            const glm::dvec3 front = a_front ? a : b;
            const glm::dvec3 back = a_front ? b : a;
            const double zf = projector.depth(front);
            const double zb = projector.depth(back);
            // The clip lands just INSIDE the eye plane's threshold, so the projection of the
            // crossed point - which rejects z <= near_eps - accepts it.
            const double clip_z = projector.near_eps * 2.0;
            if (zf <= clip_z) continue;
            const double t = (clip_z - zb) / (zf - zb);
            const glm::dvec3 crossed = back + (front - back) * t;
            if (a_front) b = crossed; else a = crossed;
        }
        glm::dvec2 pa, pb;
        if (!projector.to_screen(a, pa) || !projector.to_screen(b, pb)) {
            if (thickness > 1.1f) {
                static int rej = 0;
                if (rej < 4) {
                    SDL_Log("conic to_screen reject at seg %zu: za %.4e zb %.4e", i,
                            projector.depth(a), projector.depth(b));
                    ++rej;
                }
            }
            continue;
        }
        // Cull: both ends well outside on the same side of the frame cannot cross it.
        if ((pa.x < -64.0 && pb.x < -64.0) || (pa.x > w + 64.0 && pb.x > w + 64.0) ||
            (pa.y < -64.0 && pb.y < -64.0) || (pa.y > h + 64.0 && pb.y > h + 64.0)) {
            continue;
        }
        ui::push_line(ui, glm::vec2(pa), glm::vec2(pb), thickness, color);
    }
}

/** Sample cap and the screen angle that buys a subdivision (s2.4). */
constexpr int OVERLAY_SAMPLES_MAX = 512;
constexpr double OVERLAY_SUBDIVIDE_ANGLE = 4.0 * 3.14159265358979323846 / 180.0;

/**
 * Adaptive conic sampling (s2.4): a coarse pass in eccentric anomaly - a circle then costs about
 * 90 samples - then subdivide wherever the angle between successive screen-space segments exceeds
 * four degrees, so the budget spends itself where the curvature is. Bounded at 512 per conic. A
 * hyperbolic branch samples a bounded window about periapsis, never to infinity (s2.8). `centre`
 * is the conic's parent, in barycentric metres: every sample is built about its own parent, which
 * is the s2.8 rule for a line crossing an SOI boundary.
 */
void sample_conic(const orbit::Elements &elements, const glm::dvec2 &centre,
                  const ScreenProjector &projector, std::vector<PathSample> &out) {
    out.clear();
    if (elements.a == 0.0 || elements.mu <= 0.0) return;
    const double e = elements.e;

    const auto at_anomaly = [&](double E) {
        const double r = elements.a * (1.0 - e * std::cos(E));
        const double nu = 2.0 * std::atan2(std::sqrt(1.0 + e) * std::sin(E * 0.5),
                                           std::sqrt(1.0 - e) * std::cos(E * 0.5));
        const double theta = nu + elements.omega;
        return centre + glm::dvec2(std::cos(theta) * r, std::sin(theta) * r);
    };

    // Uniform in E; a hyperbolic branch takes a bounded window about periapsis (s2.8).
    const double span = orbit::is_elliptic(elements) ? 6.283185307179586 : 3.4;
    const int coarse = 90;
    struct Seg {
        double e0, e1;
    };
    std::vector<Seg> segments;
    segments.reserve(static_cast<size_t>(coarse));
    for (int i = 0; i < coarse; ++i) {
        segments.push_back({-span * 0.5 + span * static_cast<double>(i) / coarse,
                            -span * 0.5 + span * static_cast<double>(i + 1) / coarse});
    }

    const auto angle_between = [](const glm::dvec2 &u, const glm::dvec2 &v) {
        const double lu = glm::length(u), lv = glm::length(v);
        return lu > 1e-9 && lv > 1e-9
                   ? std::acos(glm::clamp(glm::dot(u / lu, v / lv), -1.0, 1.0))
                   : 0.0;
    };
    std::vector<Seg> refined;
    for (int pass = 0; pass < 4 && static_cast<int>(segments.size()) < OVERLAY_SAMPLES_MAX; ++pass) {
        refined.clear();
        bool any_split = false;
        for (const Seg &segment : segments) {
            const double mid = (segment.e0 + segment.e1) * 0.5;
            const glm::dvec2 p0 = at_anomaly(segment.e0);
            const glm::dvec2 p1 = at_anomaly(mid);
            const glm::dvec2 p2 = at_anomaly(segment.e1);
            glm::dvec2 s0, s1, s2;
            const bool ok0 = projector.to_screen(glm::dvec3(p0, 0.0), s0);
            const bool ok1 = projector.to_screen(glm::dvec3(p1, 0.0), s1);
            const bool ok2 = projector.to_screen(glm::dvec3(p2, 0.0), s2);
            // On screen the turn is measured in screen space; off screen, in world angle, so the
            // shape is already right when the wheel brings it back into the frame.
            const double turn = ok0 && ok1 && ok2 ? angle_between(s1 - s0, s2 - s1)
                                                  : angle_between(p1 - p0, p2 - p1);
            if (turn > OVERLAY_SUBDIVIDE_ANGLE) {
                refined.push_back({segment.e0, mid});
                refined.push_back({mid, segment.e1});
                any_split = true;
            } else {
                refined.push_back(segment);
            }
        }
        segments.swap(refined);
        if (!any_split) break;
    }

    out.reserve(segments.size() + 1);
    for (const Seg &segment : segments) {
        PathSample sample;
        sample.world = glm::dvec3(at_anomaly(segment.e0), 0.0);
        sample.front = projector.to_screen(sample.world, sample.screen);
        out.push_back(sample);
    }
    if (!segments.empty()) {
        // The closing sample, so an ellipse closes.
        PathSample sample;
        sample.world = glm::dvec3(at_anomaly(segments.back().e1), 0.0);
        sample.front = projector.to_screen(sample.world, sample.screen);
        out.push_back(sample);
    }
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
    frame.shipRadiusPx = static_cast<float>(
        world.ship.bounds.halfLength * (static_cast<double>(height) / (2.0 * app.camera.half_height)));
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
    frame.zoom = app.half_height_current;
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
    // Close-quarters proximity cues (s5.1): the nearest three solid contacts inside 500 m, on the
    // collar with their range as the tick's strength. The pilot reads "how close, on which side"
    // without cycling targets.
    {
        std::vector<const Obstacle *> near;
        for (const Obstacle &rock : world.rocks) {
            if (rock.retired || rock.z != 0) continue;
            const Real dx = rock.x - world.ship.position.x;
            const Real dy = rock.y - world.ship.position.y;
            const Real range = std::hypot(dx, dy);
            if (range > 500.0 || range < 1.0) continue;
            near.push_back(&rock);
        }
        std::sort(near.begin(), near.end(), [&](const Obstacle *a, const Obstacle *b) {
            const Real da = std::hypot(a->x - world.ship.position.x, a->y - world.ship.position.y);
            const Real db = std::hypot(b->x - world.ship.position.x, b->y - world.ship.position.y);
            return da < db;
        });
        int shown = 0;
        for (const Obstacle *rock : near) {
            if (shown >= 3) break;
            ++shown;
            const Real dx = rock->x - world.ship.position.x;
            const Real dy = rock->y - world.ship.position.y;
            const Real range = std::hypot(dx, dy);
            frame.marks.push_back({static_cast<float>(std::atan2(dy, dx)),
                                   static_cast<float>(range), opra::MarkKind::Hostile,
                                   static_cast<float>(std::max<Real>(0.3, 1.0 - range / 500.0))});
        }
    }

    {
        std::vector<Contact> tracked_list = world.contacts();
        const int tracked_at = contact_index(tracked_list, world.target);
        if (tracked_at >= 0) {
            const Contact &contact = tracked_list[static_cast<size_t>(tracked_at)];
            const Real dx = contact.position.x - world.ship.position.x;
            const Real dy = contact.position.y - world.ship.position.y;
            const Real range = std::hypot(dx, dy);
            frame.marks.push_back({static_cast<float>(std::atan2(dy, dx)),
                                   static_cast<float>(range), opra::MarkKind::Target, 1.0f});
        }
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

    // ---- the s4.3 readout maths. One place computes them; the HUD only prints.
    {
        // Vertical and horizontal speed against the dominant body; in the zone the frame's own
        // tide is what the ship actually feels, so the numbers are against the primary's state.
        frame.verticalSpeed = frame.descentRate;  // already the radial component, closing positive
        const double v2 = frame.speed * frame.speed;
        frame.horizontalSpeed = std::sqrt(std::max(0.0, v2 - frame.verticalSpeed * frame.verticalSpeed));
        const Real wet = world.ship.spec->mass + world.ship.fuel;
        frame.massTonnes = wet / 1000.0;
        // TWR against the primary's local gravity; the dash when there is no dominant body.
        if (primary && primary->mu > 0.0 && radius > 1.0) {
            const double g = primary->mu / (radius * radius);
            if (g > 1.0e-6) {
                frame.twr = static_cast<double>(world.ship.spec->thrust) / (wet * g);
                frame.twrValid = true;
            }
        }
        // dv = Isp * g0 * ln(m_wet/m_dry). The sim's exhaust model is the thrust divided by its
        // full-throttle flow (14 t/s), and the ln is guarded: empty tanks print 0.00, not -inf.
        const Real mdot = 14000.0;
        const double isp_g0 = mdot > 0.0 ? static_cast<double>(world.ship.spec->thrust) / mdot : 0.0;
        const Real dry = world.ship.spec->mass;
        frame.deltaV = wet > dry * 1.0000001 ? isp_g0 * std::log(static_cast<double>(wet) / dry) : 0.0;
        frame.burnSeconds = world.ship.spec->thrust > 0.0
                                ? frame.deltaV * wet / static_cast<double>(world.ship.spec->thrust)
                                : 0.0;
        frame.throttle = ui::clamp01(static_cast<float>(world.ship.thrustLevel));
        // The warp rail's suggestion (s2.7), for the time block to show.
        const double scale = camera_half_for_suggestion(app);
        frame.warpSuggest = std::clamp(std::pow(10.0, std::floor(std::log10(scale / 50.0))), 1.0,
                                       100000.0);
        // The tracked contact: range and closing rate (s4.3's last row).
        std::vector<Contact> contacts = world.contacts();
        const int tracked = contact_index(contacts, world.target);
        if (tracked >= 0) {
            const Contact &contact = contacts[static_cast<size_t>(tracked)];
            const Real dx = contact.position.x - world.ship.position.x;
            const Real dy = contact.position.y - world.ship.position.y;
            const Real range = std::hypot(dx, dy);
            if (range > 1.0) {
                const Real closing = -(dx * world.ship.velocity.x + dy * world.ship.velocity.y) / range;
                frame.targetValid = true;
                frame.targetName = contact.name.c_str();
                frame.targetRange = range;
                frame.closingRate = closing;
            }
        }
        // The first node: dv, burn seconds, T- (s4.1's NODE block). Consumed nodes go dark via
        // nodeValid - run_nodes erases them, so a past node never prints a negative T-.
        if (!world.nodes.empty()) {
            const orbit::Node &node = world.nodes.front();
            frame.nodeValid = true;
            frame.nodeDeltaV = std::hypot(node.prograde, node.radial);
            frame.nodeBurnSeconds =
                world.ship.spec->thrust > 0.0
                    ? frame.nodeDeltaV * (world.ship.spec->mass + world.ship.fuel) /
                          static_cast<double>(world.ship.spec->thrust)
                    : 0.0;
            frame.nodeTMinus = std::max(0.0, node.t - world.elapsed);
        }
        // Incoming fire (s4.5): a fresh hull contact inside a second is what the threat block is
        // for. K10's rounds land here through the same timer.
        frame.underFire = world.ship.contactTimer < 1.0;
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

// ---------------------------------------------------------------------- the overlay (s2.4, s2.6)

void build_zoom_overlay(UIBatch &ui, const World &world, const Camera &camera, float width,
                        float height) {
    using namespace ui;
    const ScreenProjector projector = ScreenProjector::for_camera(camera, width, height);
    const size_t count = std::min(world.system.bodies.size(), world.bodies.size());

    std::vector<PathSample> path;

    // Body orbit rings: a body's own conic about its own parent, in barycentric coordinates (s2.8).
    // A ring draws while its primary is small enough on screen that the ring is information rather
    // than a horizon - past that the hull you are standing next to is its own orbit.
    const double ring_gate_px = 250.0;
    for (size_t i = 0; i < count; ++i) {
        const Body &def = world.system.bodies[i];
        if (def.parent < 0 || static_cast<size_t>(def.parent) >= count) continue;
        if (def.mu <= 0.0) continue;  // a station rides no ring of its own
        const glm::dvec2 at_zone = world.body_zone_position(static_cast<int>(i));
        const glm::dvec2 centre =
            world.body_zone_position(def.parent);  // the ring is about its own parent (s2.8)
        const double distance =
            std::max(glm::length(glm::dvec3(at_zone.x - camera.origin.x, at_zone.y - camera.origin.y,
                                            0.0)), 1.0);
        const double px = 2.0 * def.radius / distance *
                          (static_cast<double>(height) * 0.5) / std::tan(CAMERA_FOV_Y * 0.5);
        if (px > ring_gate_px) continue;
        sample_conic(def.elements, centre, projector, path);
        draw_polyline(ui, projector, path, with_alpha(ui::tokens::ETCH_DIM, 0.40f), 1.0f);
    }

    // The ship's own conic about its primary: the actual orbit, or the plan's prediction when the
    // pilot has nodes on the book. Drawn once the view is past hull scale, where an orbit is the
    // line the pilot is flying rather than a line through the room.
    const size_t chief = static_cast<size_t>(world.primary < 0 ? 0 : world.primary);
    if (camera.half_height >= 1.0e3 && chief < world.system.bodies.size() &&
        world.system.bodies[chief].mu > 0.0 && !world.system.bodies.empty()) {
        const orbit::Elements conic =
            world.nodes.empty()
                ? orbit::from_state(world.system_position(), world.system_velocity(),
                                    world.system.bodies[chief].mu, world.elapsed)
                : world.planned_conic();
        const glm::dvec2 centre = world.body_zone_position(static_cast<int>(chief));
        const glm::vec4 ink = world.nodes.empty()
                                  ? with_alpha(ui::tokens::NAV, 0.55f)
                                  : with_alpha(ui::tokens::DRIVE, 0.7f);
        sample_conic(conic, centre, projector, path);
        draw_polyline(ui, projector, path, ink, 1.2f);

        // Predicted legs (plan 3.6): where the conic goes after the next SOI crossings, dashed
        // because a leg after a crossing is an intention. System scale only: inside it the answer
        // is the conic itself.
        if (camera.half_height >= 1.0e6) {
            const std::vector<orbit::EncounterLeg> legs =
                orbit::predict_encounters(world.system, world.bodies, conic, world.elapsed, 3);
            for (const orbit::EncounterLeg &leg : legs) {
                path.clear();
                path.reserve(leg.points.size());
                for (const glm::dvec2 &at : leg.points) {
                    // Predicted points are barycentric; the projector is zone-frame.
                    const glm::dvec2 at_zone = at - glm::dvec2(world.anchor.position);
                    PathSample sample;
                    sample.world = glm::dvec3(at_zone, 0.0);
                    sample.front = projector.to_screen(sample.world, sample.screen);
                    path.push_back(sample);
                }
                draw_polyline(ui, projector, path, with_alpha(ui::tokens::DRIVE, 0.35f), 1.0f);
            }
        }
    }

    // Manoeuvre nodes (s2.6: crossed circle), at the point on the conic where the burn happens.
    // The walk is run_nodes': each node's position is read off the conic that exists before it.
    if (!world.nodes.empty() && chief < world.system.bodies.size() &&
        world.system.bodies[chief].mu > 0.0) {
        orbit::Elements leg = orbit::from_state(world.system_position(), world.system_velocity(),
                                                world.system.bodies[chief].mu, world.elapsed);
        for (const orbit::Node &node : world.nodes) {
            const orbit::State at = orbit::state_at(leg, node.t);
            const glm::dvec2 at_zone = at.r - glm::dvec2(world.anchor.position);
            glm::dvec2 px;
            if (projector.to_screen(glm::dvec3(at_zone, 0.0), px)) {
                const glm::vec2 mark(px);
                ui::push_arc(ui, mark, 6.0f, 0.0f, 6.2831853f, 1.4f, ui::tokens::DRIVE);
                ui::push_line(ui, mark - glm::vec2(4.2f, 4.2f), mark + glm::vec2(4.2f, 4.2f), 1.2f,
                              ui::tokens::DRIVE);
                ui::push_line(ui, mark + glm::vec2(-4.2f, 4.2f), mark + glm::vec2(4.2f, -4.2f),
                              1.2f, ui::tokens::DRIVE);
            }
            leg = orbit::apply_node(leg, node);
        }
    }

    // Discoveries (s6.1): the survey's permanent finds, as crossed circles with labels - the
    // chart's own mark for "something is here that was not here when you arrived".
    for (const World::Discovery &discovery : world.discoveries) {
        if (!discovery.found) continue;
        const glm::dvec2 at_zone =
            world.body_zone_position(discovery.body) +
            glm::dvec2(std::cos(discovery.theta), std::sin(discovery.theta)) *
                world.system.bodies[static_cast<size_t>(discovery.body)].radius;
        glm::dvec2 px;
        if (!projector.to_screen(glm::dvec3(at_zone, 0.0), px)) continue;
        const glm::vec2 mark(px);
        ui::push_arc(ui, mark, 5.0f, 0.0f, 6.2831853f, 1.2f, ui::tokens::NAV);
        ui::push_line(ui, mark - glm::vec2(3.5f, 3.5f), mark + glm::vec2(3.5f, 3.5f), 1.2f,
                      ui::tokens::NAV);
        ui::push_line(ui, mark + glm::vec2(-3.5f, 3.5f), mark + glm::vec2(3.5f, -3.5f), 1.2f,
                      ui::tokens::NAV);
        ui::push_text(ui, discovery.name.c_str(), 12.0f, {mark.x + 8.0f, mark.y - 6.0f},
                      TextAlign::Left, with_alpha(ui::tokens::NAV, 0.9f), TextFace::Label);
    }

    // ---- icons (s2.6): the honest mark for anything under eight pixels across
    struct IconMark {
        glm::vec2 px{0.0f};
        std::string label;
        int kind = 0;  // 0 planet, 1 star, 2 station, 3 own ship
        float px_radius = 0.0f;
    };
    std::vector<IconMark> marks;

    for (size_t i = 0; i < count; ++i) {
        const Body &def = world.system.bodies[i];
        const glm::dvec2 at_zone = world.body_zone_position(static_cast<int>(i));
        glm::dvec2 px;
        if (!projector.to_screen(glm::dvec3(at_zone, 0.0), px)) continue;
        const double distance =
            std::max(glm::length(glm::dvec3(at_zone.x - camera.origin.x, at_zone.y - camera.origin.y,
                                            0.0)), 1.0);
        const double px_radius =
            def.radius / distance * (static_cast<double>(height) * 0.5) / std::tan(CAMERA_FOV_Y * 0.5);
        if (px_radius >= 4.0) continue;  // eight pixels across: geometry takes over from the icon
        IconMark mark;
        mark.px = glm::vec2(px);
        mark.label = def.name;
        mark.kind = def.kind == BodyClass::Star ? 1 : 0;
        mark.px_radius = static_cast<float>(px_radius);
        marks.push_back(mark);
    }
    // The station, at the marks' own scale rule.
    {
        glm::dvec2 px;
        if (projector.to_screen(glm::dvec3(STATION.x, STATION.y, 0.0), px)) {
            const double distance =
                std::max(glm::length(glm::dvec3(STATION.x - camera.origin.x, STATION.y - camera.origin.y,
                                                0.0)), 1.0);
            const double px_radius = 78.0 / distance * (static_cast<double>(height) * 0.5) /
                                     std::tan(CAMERA_FOV_Y * 0.5);
            if (px_radius < 4.0) {
                IconMark mark;
                mark.px = glm::vec2(px);
                mark.label = "Wayfarer";
                mark.kind = 2;
                mark.px_radius = static_cast<float>(px_radius);
                marks.push_back(mark);
            }
        }
    }
    // Own ship: the filled chevron once the hull itself is under eight pixels.
    {
        glm::dvec2 px;
        if (projector.to_screen(glm::dvec3(world.ship.position.x, world.ship.position.y, 0.0), px)) {
            const double ship_px = static_cast<double>(world.ship.bounds.halfLength) * 2.0 /
                                   camera.half_height * (static_cast<double>(height) * 0.5);
            if (ship_px < 8.0) {
                IconMark mark;
                mark.px = glm::vec2(px);
                mark.label = world.ship.spec ? world.ship.spec->name : "";
                mark.kind = 3;
                mark.px_radius = static_cast<float>(ship_px * 0.5);
                marks.push_back(mark);
            }
        }
    }

    // Declutter (s2.6): a 48 px bucket grid; more than three in one bucket and the individuals
    // become one cluster mark with a count.
    struct Bucket {
        int total = 0;
        glm::vec2 sum{0.0f};
    };
    std::vector<int> dropped(marks.size(), 0);
    std::vector<Bucket> buckets(marks.size());
    for (size_t i = 0; i < marks.size(); ++i) {
        const int bx = static_cast<int>(marks[i].px.x / 48.0f);
        const int by = static_cast<int>(marks[i].px.y / 48.0f);
        int bucket = -1;
        for (size_t k = 0; k < buckets.size(); ++k) {
            if (dropped[k] != 0) continue;
            const int kx = static_cast<int>(marks[k].px.x / 48.0f);
            const int ky = static_cast<int>(marks[k].px.y / 48.0f);
            if (kx == bx && ky == by) { bucket = static_cast<int>(k); break; }
        }
        if (bucket < 0) {
            buckets[i].total = 1;
            buckets[i].sum = marks[i].px;
        } else {
            buckets[static_cast<size_t>(bucket)].total += 1;
            buckets[static_cast<size_t>(bucket)].sum += marks[i].px;
            dropped[i] = 1;  // folded into the bucket's cluster mark
        }
    }
    for (size_t i = 0; i < marks.size(); ++i) {
        if (dropped[i] == 0 && buckets[i].total > 3) dropped[i] = 2;  // becomes the cluster mark
    }

    for (size_t i = 0; i < marks.size(); ++i) {
        const IconMark &mark = marks[i];
        if (dropped[i] == 1) continue;
        if (dropped[i] == 2) {
            const glm::vec2 at = buckets[i].sum / static_cast<float>(buckets[i].total);
            ui::push_disc(ui, at, 3.0f, with_alpha(ui::tokens::ETCH, 0.9f));
            ui::push_text(ui, std::to_string(buckets[i].total).c_str(), 11.0f,
                          {at.x + 6.0f, at.y - 5.0f}, TextAlign::Left,
                          with_alpha(ui::tokens::ETCH_DIM, 0.9f), TextFace::Label);
            continue;
        }
        const glm::vec2 at = mark.px;
        switch (mark.kind) {
            case 1: {  // star: filled disc with a four-point flare
                ui::push_disc(ui, at, std::max(3.0f, mark.px_radius), ui::tokens::DRIVE);
                for (const glm::vec2 dir : {glm::vec2(1, 0), glm::vec2(-1, 0), glm::vec2(0, 1),
                                            glm::vec2(0, -1)}) {
                    ui::push_line(ui, at + dir * 4.0f, at + dir * 10.0f, 1.0f,
                                  with_alpha(ui::tokens::DRIVE, 0.7f));
                }
                break;
            }
            case 2: {  // station: ring
                ui::push_arc(ui, at, std::max(3.5f, mark.px_radius), 0.0f, 6.2831853f, 1.2f,
                             ui::tokens::NAV);
                break;
            }
            case 3: {  // own ship: filled chevron, oriented to heading
                const float a = static_cast<float>(-world.ship.angle);
                const glm::vec2 nose(-std::sin(a), -std::cos(a));
                const glm::vec2 side(-nose.y, nose.x);
                ui::push_triangle(ui, at + nose * 6.0f, at - nose * 4.0f + side * 4.0f,
                                  at - nose * 4.0f - side * 4.0f, ui::tokens::ETCH);
                break;
            }
            default:  // planet / moon: filled disc
                ui::push_disc(ui, at, std::max(3.0f, mark.px_radius), ui::tokens::ETCH);
                break;
        }
    }

    // Labels: above roughly twelve pixels of separation from the last label placed, so system
    // scale reads as a chart and not a wall (s2.6).
    glm::vec2 last_label{-1e9f, -1e9f};
    for (size_t i = 0; i < marks.size(); ++i) {
        const IconMark &mark = marks[i];
        if (dropped[i] != 0 || mark.label.empty()) continue;
        if (glm::length(mark.px - last_label) < 12.0f) continue;
        last_label = mark.px;
        ui::push_text(ui, mark.label.c_str(), 12.0f, {mark.px.x + 7.0f, mark.px.y - 6.0f},
                      TextAlign::Left, with_alpha(ui::tokens::ETCH_DIM, 0.85f), TextFace::Label);
    }
}

}  // namespace opra
