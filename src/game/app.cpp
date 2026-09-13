#include "game/app.h"

#include <filesystem>

#include "core/file.h"
#include "core/log.h"
#include "game/bindings.h"
#include "game/config.h"
#include "game/effects.h"
#include "gpu/gpu.h"
#include "orbit/transfer.h"
#include "render/renderer.h"
#include "sim/system.h"
#include "ui/draw.h"

namespace opra {

using namespace opra::config;  // tunables, bare by design

/**
 * Nearest body inside the beam's swept segment, or null. Walks the grid along the segment rather
 * than the whole field, and fragments are filed there too, so they are cuttable like rock.
 */
Obstacle *beam_target(World &world, const Vec2 &muzzle, const Vec2 &direction, Real range,
                      Vec2 &hit_point) {
    Real best_t = 2.0;
    Obstacle *best = nullptr;
    const Real end_x = muzzle.x + direction.x * range;
    const Real end_y = muzzle.y + direction.y * range;
    std::vector<Obstacle *> candidates;
    world.grid.near_segment(muzzle.x, muzzle.y, end_x, end_y, candidates);
    for (Obstacle *body : candidates) {
        if (body->z != 0 || body->hp <= 0) continue;
        Circle circle{body->x, body->y, body->radius * 0.92};
        Real t = 0;
        if (!segment_circle_hit(muzzle.x, muzzle.y, end_x, end_y, circle, circle.radius, t)) {
            continue;
        }
        if (body->retired) continue;  // broken bodies stop existing as targets
        if (t < best_t) {
            best_t = t;
            best = body;
        }
    }
    hit_point = {muzzle.x + direction.x * range * best_t, muzzle.y + direction.y * range * best_t};
    return best;
}

void App::build_part_table() {
    part_table.clear();
    for (const std::string &name : models.store.names()) {
        const ModelMeta &meta = models.store.meta(name);
        if (meta.part.has_value()) {
            const ModelPart &mp = *meta.part;
            PartSpec ps;
            ps.kind = mp.kind;
            ps.span = mp.span;
            ps.axial = mp.axial;
            ps.dry_mass = mp.mass;
            ps.propellant = mp.propellant;
            ps.thrust = mp.thrust;
            ps.cooling = mp.cooling;
            ps.heat_capacity = mp.heat_capacity;
            ps.rcs_jets = mp.rcs_jets;
            ps.rcs_authority = mp.rcs_authority;
            part_table[name] = ps;
        }
    }
}

namespace {

void validate_design_parts(const ShipDesign &design, const PartTable &parts) {
    std::vector<std::string> missing;
    for (const auto &p : design.placements) {
        if (parts.find(p.part) == parts.end()) {
            if (std::find(missing.begin(), missing.end(), p.part) == missing.end()) {
                missing.push_back(p.part);
            }
        }
    }
    if (!missing.empty()) {
        std::string msg = "Design '" + design.name + "' references missing part(s): ";
        for (size_t i = 0; i < missing.size(); ++i) {
            if (i > 0) msg += ", ";
            msg += missing[i];
        }
        fatal(msg.c_str());
    }
}

}  // namespace

void App::init(bool hidden, bool debug_gpu) {
    debug = debug_gpu;
    settings.load();
    // F1: the pitch is a setting, so it has to be picked up before the first frame, not only when
    // the settings screen is touched.
    pitch = settings.camera_pitch * 0.017453292519943295f;
    renderer.samples = settings.msaa >= 4 ? 4 : 1;
    pending_samples = renderer.samples;
    world.ship.assist = settings.assist;
    // The sector co-orbits Wayfarer, which the system file names: the anchor is a lookup, not a
    // constant, so a different system file needs no code change (E4).
    system = load_system(asset_path("assets/systems/nereid.json"));
    system_anchor = system.index_of("wayfarer");
    if (system_anchor < 0) fatal("system file has no wayfarer body to anchor the sector to");
    world.attach_system(system, system_anchor);
    // Built once here so the first map frame's camera has a system to frame: the loop picks the
    // camera before the update pass runs.
    map_frame = orrery_frame_for(world, map_target);
    window = SDL_CreateWindow("Opra", 1600, 900, SDL_WINDOW_RESIZABLE | (hidden ? SDL_WINDOW_HIDDEN : 0));
    if (!window) fatal("SDL_CreateWindow");

    renderer.device = gpu::create_device(window, debug_gpu);
    SDL_GPUDevice *gpu = renderer.device.handle;
    SDL_Log("GPU: %s (%s)", SDL_GetGPUDeviceDriver(gpu),
            SDL_GetStringProperty(SDL_GetGPUDeviceProperties(gpu),
                                  SDL_PROP_GPU_DEVICE_NAME_STRING, "?"));
    // Shipped faces only, per E10: the build carries its own type, so a machine without
    // bahnschrift or segoeui still renders the HUD. A missing face is fatal, not silent.
    text.set_fonts(asset_path("assets/fonts/HydrogenWhiskey.otf"),
                   asset_path("assets/fonts/BarlowCondensed-SemiBold.ttf"),
                   asset_path("assets/fonts/Barlow-Regular.ttf"));

    models.build(asset_path("assets/models.json"));
    build_part_table();
    designs.load(asset_path("assets/designs.json"));
    world.design = designs.design("Kestrel");
    validate_design_parts(world.design, part_table);
    world.rebuild_from_design(part_table);
    sync_ship_collider();
    sync_ports();
    // The orrery's meshes must exist before the upload: the draw table is built from the library.
    orrery_meshes = orrery::add_meshes(models.library);
    upload_mesh_library(renderer, models.library);

    SDL_GPUSamplerCreateInfo sampler_info{};
    sampler_info.min_filter = SDL_GPU_FILTER_LINEAR;
    sampler_info.mag_filter = SDL_GPU_FILTER_LINEAR;
    sampler_info.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST;
    sampler_info.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    sampler_info.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    sampler_info.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    renderer.sampler = SDL_CreateGPUSampler(gpu, &sampler_info);
    if (!renderer.sampler) fatal("SDL_CreateGPUSampler");
}

void App::shutdown() {
    text.destroy(renderer.device);
    destroy_renderer(renderer);
    gpu::destroy_device(renderer.device);
    SDL_DestroyWindow(window);
    TTF_Quit();
    SDL_Quit();
}

void App::toast(std::string message, double duration) {
    toasts.push_back({std::move(message), now + duration});
    if (toasts.size() > 4) toasts.erase(toasts.begin());
}

Camera active_camera(const App &app, Uint32 width, Uint32 height) {
    // Startup diorama camera (PLAN-08 §10.2): pitch 28°, slow yaw drift, half_height 95 m,
    // berthed ship at station Port A shifted right to clear title plate.
    if (app.current() == ui::Screen::Startup) {
        Camera camera;
        const float pitch = 0.4887f; // 28 deg
        const float yaw = app.title_yaw != 0.0f ? app.title_yaw : 0.55f;
        const float half_height = 95.0f;
        const float aspect = height > 0 ? static_cast<float>(width) / static_cast<float>(height) : 1.0f;

        camera.half_height = half_height;
        camera.aspect = aspect;
        camera.up = glm::vec3(0.0f, 0.0f, 1.0f);

        const glm::vec3 fwd(std::cos(pitch) * std::sin(yaw),
                            -std::cos(pitch) * std::cos(yaw),
                            std::sin(pitch));
        const glm::vec3 right_vec = glm::normalize(glm::cross(fwd, glm::vec3(0.0f, 0.0f, 1.0f)));

        const float ship_half_len = static_cast<float>(app.world.ship.bounds.halfLength > 0 ? app.world.ship.bounds.halfLength : 24.0);
        glm::vec3 target{100.6f + 1.2f - ship_half_len, -8.0f, 0.0f};

        const float shift = 2.0f * (0.5f - 0.62f) * half_height * aspect;
        target += right_vec * shift;

        camera.target = target;
        camera.eye = target + fwd * static_cast<float>(half_height / std::tan(static_cast<double>(CAMERA_FOV_Y) * 0.5));
        camera.origin = target;
        return camera;
    }
    if (app.current() == ui::Screen::Viewer) return app.viewer.camera(app.models, width, height);
    if (app.current() == ui::Screen::Shipyard) {
        return ui::shipyard_camera(app.models, app.shipyard_state, width, height);
    }
    return camera_for(app.world, width, height, app.half_height_current, app.cinematic, app.pitch,
                      app.follow.target, app.camera_look);
}

void App::apply_settings() {
    pending_samples = settings.msaa >= 4 ? 4 : 1;
    // F1: the pitch is read here and nowhere else. There is no binding that changes it mid-flight.
    pitch = settings.camera_pitch * 0.017453292519943295f;
    settings_dirty = true;
}

// P4: the hull collides as the exporter's shape set, scaled into sim metres. The single box it
// replaced (D-19) registered hits in the gaps between the Mule's chassis rails.
void App::sync_ship_collider() {
    if (!world.design.placements.empty()) {
        const Real design_scale = world.design.scale;
        Collider &collider = world.ship.collider;
        collider.shapes.clear();
        world.pdc_mounts.clear();
        Real max_radius = 0.0;

        for (const Placement &p : world.design.placements) {
            if (p.destroyed) continue;
            const ModelMeta &meta = models.store.meta(p.part);
            const Mount m = mount_transform(world.design.spine, p);
            const glm::dvec3 f = m.rot * glm::dvec3(0.0, 1.0, 0.0);
            const Real yaw = (std::abs(f.x) + std::abs(f.y) < 1e-6) ? 0.0 : std::atan2(-f.x, f.y);
            const Real cos_y = std::cos(yaw);
            const Real sin_y = std::sin(yaw);
            const Vec2 o = {m.pos.x * design_scale, m.pos.y * design_scale};

            for (const ModelShape &shape : meta.shapes) {
                Shape out;
                out.kind = shape.kind == ModelShape::Kind::Circle ? Shape::Kind::Circle : Shape::Kind::Box;
                const Vec2 local_pos = {shape.pos.x * design_scale, shape.pos.y * design_scale};
                out.local_pos = {cos_y * local_pos.x - sin_y * local_pos.y + o.x,
                                 sin_y * local_pos.x + cos_y * local_pos.y + o.y};
                out.local_angle = shape.angle + yaw;
                out.half_length = shape.half.y * design_scale;
                out.half_width = shape.half.x * design_scale;
                out.radius = shape.radius * design_scale;

                const Real c_dist = std::hypot(out.local_pos.x, out.local_pos.y);
                const Real shape_r = (out.kind == Shape::Kind::Box)
                                         ? std::hypot(out.half_width, out.half_length)
                                         : out.radius;
                max_radius = std::max(max_radius, c_dist + shape_r);

                collider.shapes.push_back(out);
            }

            for (const auto &[id, at] : meta.hardpoints) {
                if (id.rfind("pdc.", 0) != 0) continue;
                const Vec2 local_hp = {at.x * design_scale, at.y * design_scale};
                const Vec2 rotated_hp = {cos_y * local_hp.x - sin_y * local_hp.y + o.x,
                                         sin_y * local_hp.x + cos_y * local_hp.y + o.y};
                world.pdc_mounts.push_back(rotated_hp);
            }
        }

        if (collider.shapes.size() > 64) {
            fatal(("Composed collider shape count " +
                   std::to_string(collider.shapes.size()) + " exceeds 64 (gate 14)").c_str());
        }

        collider.bounds_radius = max_radius;
        world.ship.bounds = collider_bounds(collider);
        world.derived.length = 2.0 * world.ship.bounds.halfLength;
        return;
    }

    const ModelMeta &meta =
        models.store.meta(SHIP_MODEL_NAMES[static_cast<int>(world.ship.shipClass)]);
    const Real scale = meta.scale;
    Collider &collider = world.ship.collider;
    collider.shapes.clear();
    collider.shapes.reserve(meta.shapes.size());
    for (const ModelShape &shape : meta.shapes) {
        Shape out;
        out.kind = shape.kind == ModelShape::Kind::Circle ? Shape::Kind::Circle : Shape::Kind::Box;
        out.local_pos = {shape.pos.x * scale, shape.pos.y * scale};
        out.local_angle = shape.angle;
        out.half_length = shape.half.y * scale;
        out.half_width = shape.half.x * scale;
        out.radius = shape.radius * scale;
        collider.shapes.push_back(out);
    }
    if (collider.shapes.empty()) {
        // A legacy sidecar wrote no shapes: its AABB at sim scale is the whole hull again.
        collider = box_collider(meta.collider.halfLength * scale, meta.collider.halfWidth * scale);
    } else {
        collider.bounds_radius = meta.bounds_radius * scale;
    }
    // The collar radius and the cutter muzzle want one pair of extents, not a shape list.
    world.ship.bounds = collider_bounds(collider);
    // The PDC mounts ride the sidecar's hardpoints (s5.2): every `pdc.` anchor in the model is a
    // turret, in ship-frame metres, exactly the conversion the collider shapes went through.
    world.pdc_mounts.clear();
    for (const auto &[id, at] : meta.hardpoints) {
        if (id.rfind("pdc.", 0) != 0) continue;
        world.pdc_mounts.push_back({at.x * scale, at.y * scale});
    }
}

void App::sync_ports() {
    // E9: ports are hardpoints in the model, so they arrive with the model and a modular hull
    // brings its own. The one place that knows both layers does the conversion, exactly as the
    // collider does.
    std::vector<Port> station;
    const ModelMeta &station_meta = models.store.meta("station");
    for (const ModelPort &port : station_meta.ports) {
        const Real scale = station_meta.scale;
        station.push_back(Port{port.id, {port.pos.x * scale, port.pos.y * scale}, port.normal,
                               port.size_class});
    }

    if (!world.design.placements.empty()) {
        std::vector<Port> ship_ports;
        char next_port_id = 'A';
        const Real design_scale = world.design.scale;
        for (const Placement &p : world.design.placements) {
            if (p.destroyed) continue;
            const ModelMeta &meta = models.store.meta(p.part);
            const Mount m = mount_transform(world.design.spine, p);
            const glm::dvec3 f = m.rot * glm::dvec3(0.0, 1.0, 0.0);
            const Real yaw = (std::abs(f.x) + std::abs(f.y) < 1e-6) ? 0.0 : std::atan2(-f.x, f.y);
            const Real cos_y = std::cos(yaw);
            const Real sin_y = std::sin(yaw);
            const Vec2 o = {m.pos.x * design_scale, m.pos.y * design_scale};

            for (const ModelPort &mp : meta.ports) {
                std::string assigned_id(1, next_port_id++);
                const Vec2 local_pos = {mp.pos.x * design_scale, mp.pos.y * design_scale};
                const Vec2 c_prime = {cos_y * local_pos.x - sin_y * local_pos.y + o.x,
                                      sin_y * local_pos.x + cos_y * local_pos.y + o.y};
                const Vec2 norm_prime = {cos_y * mp.normal.x - sin_y * mp.normal.y,
                                         sin_y * mp.normal.x + cos_y * mp.normal.y};
                ship_ports.push_back(Port{assigned_id, c_prime, norm_prime, mp.size_class});
            }
        }
        Port ship;
        if (!ship_ports.empty()) {
            ship = ship_ports.front();
        } else {
            ship = Port{"aft", {0.0, -world.ship.bounds.halfLength}, {0.0, -1.0}, 'M'};
        }
        world.set_ports(station, ship);
        return;
    }

    Port ship;
    const ModelMeta &ship_meta =
        models.store.meta(SHIP_MODEL_NAMES[static_cast<int>(world.ship.shipClass)]);
    if (!ship_meta.ports.empty()) {
        const ModelPort &port = ship_meta.ports.front();
        const Real scale = ship_meta.scale;
        ship = Port{port.id, {port.pos.x * scale, port.pos.y * scale}, port.normal, port.size_class};
    } else {
        // A hull whose model exports no port berths on its aft face: the convention every ship in
        // this build follows, so a missing hardpoint is a fallback, not a fatal.
        ship = Port{"aft", {0.0, -world.ship.bounds.halfLength}, {0.0, -1.0}, 'M'};
    }
    world.set_ports(station, ship);
}

void App::plan_transfer() {
    // The planner lives in the flight view now (plan 05 J2): the target body is whatever a
    // double-click framed last, and the numbers are computed from the world, not from a chart
    // frame, because there is no chart frame any more.
    if (map_target < 1 || system.bodies.empty() || system.bodies.size() < 2) return;
    if (map_target >= static_cast<int>(system.bodies.size())) return;
    if (map_target >= static_cast<int>(world.bodies.size())) return;
    const Body &body = system.bodies[static_cast<size_t>(map_target)];
    const double mu = system.bodies[0].mu;
    if (mu <= 0.0) return;
    const double from = glm::length(world.system_position());
    const double to_radius = body.elements.a;
    const orbit::Hohmann plan = orbit::hohmann(from, to_radius, mu);
    const double ship_motion = std::sqrt(mu / (from * from * from));
    const double target_motion = std::sqrt(mu / (to_radius * to_radius * to_radius));
    const double phase_now = std::atan2(world.system_position().y, world.system_position().x) -
                             std::atan2(world.bodies[static_cast<size_t>(map_target)].position.y,
                                        world.bodies[static_cast<size_t>(map_target)].position.x);
    const double window_seconds = orbit::time_to_window(
        phase_now, orbit::phase_angle_required(target_motion, plan.transfer_time), ship_motion,
        target_motion);
    // Two burns, not one: the departure at the window, the arrival a transfer later. The arrival
    // is retrograde in the orbital frame - it is a braking burn - and that is the whole reason the
    // node carries a signed prograde component rather than a magnitude.
    const double departure = world.elapsed + std::max(0.0, window_seconds);
    world.nodes.push_back(orbit::Node{departure, plan.dv1, 0.0});
    world.nodes.push_back(orbit::Node{departure + plan.transfer_time, -plan.dv2, 0.0});
    toast("Transfer planned: " + body.name);
}

void App::reset_run() {
    ShipDesign current_design = world.design;
    world = World{};
    world.design = std::move(current_design);
    world.rebuild_from_design(part_table);
    world.attach_system(system, system_anchor);
    world.ship.assist = settings.assist;
    clear_effects();
    sync_ship_collider();
    sync_ports();
    half_height = HOME_HALF;
    half_height_current = half_height;
    follow_body = -1;
    system_recall_half = 0.0;
    // The camera goes back on the ship with the new run's lead, and with no spring transient: a
    // reset that swung the frame across 13 Gm would be the first thing the player saw.
    follow_snap(follow, glm::dvec2(world.ship.position.x, world.ship.position.y),
                glm::dvec2(world.ship.velocity.x, world.ship.velocity.y), FollowParams{});
    beam_active = false;
    mining = false;
    wrecked = false;
    guns_warned = false;
    toasts.clear();
    toast("Run restarted");
}

void App::reload_models_if_stale(bool force) {
    if (!force && !models.store.stale()) return;
    const std::string manifest = asset_path("assets/models.json");
    const int before = models.library.size();
    models.build(manifest);
    build_part_table();
    world.rebuild_from_design(part_table);
    upload_mesh_library(renderer, models.library);
    sync_ship_collider();
    sync_ports();
    SDL_Log("models reloaded: %d meshes before, %d after", before, models.library.size());
    toast("models reloaded");
}

Camera camera_for(const World &world, Uint32 width, Uint32 height, double half_height,
                  bool cinematic, float pitch, const glm::dvec2 &follow, const glm::dvec2 &look) {
    Camera camera;
    // One continuous zoom (plan 05 J2): the half-height IS the control. Cinematic is the home
    // framing pulled back twice, not a second camera.
    camera.half_height = clamp_half_height(half_height * (cinematic ? 2.0 : 1.0));
    camera.aspect = static_cast<float>(width) / static_cast<float>(height);
    // F1: the render origin is the follow point, not the ship. The ship is placed like every other
    // instance - as float(world - origin) - so it sits off centre by the lead and the deadzone, and
    // nothing Gm-scale reaches a float because the follow stays within a few hundred metres of it.
    camera.origin = glm::dvec3(follow.x, follow.y, 0.0);
    camera.target = glm::vec3(0.0f);
    // J1: the eye swings inside the cone about the home axis; the follow point never moves.
    camera.eye = look_eye(camera.half_height_f(), pitch, look);
    return camera;
}



}  // namespace opra
