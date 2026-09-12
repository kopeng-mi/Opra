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
    map_frame = orrery_frame_for(world, true_scale, map_target);
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
    // The map and the title plate share the orrery's own camera: the system seen from the pole,
    // the almanac taking the right of the frame when the map screen is up.
    if (app.screen == ui::Screen::Startup || app.screen == ui::Screen::Map) {
        return orrery::map_camera(app.map_frame, static_cast<float>(width),
                                  static_cast<float>(height),
                                  app.screen == ui::Screen::Startup ? 0.5f : 0.34f);
    }
    if (app.screen == ui::Screen::Viewer) return app.viewer.camera(app.models, width, height);
    return camera_for(app.world, width, height, app.zoom_current, app.cinematic, app.pitch,
                      app.follow.target + app.camera_pan);
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
    if (map_target < 1 || system.bodies.empty() || !map_frame.target.has_target) return;
    const Body &body = system.bodies[static_cast<size_t>(map_target)];
    const double mu = system.bodies[0].mu;
    if (mu <= 0.0) return;
    const double from = glm::length(world.system_position());
    const orbit::Hohmann plan = orbit::hohmann(from, body.elements.a, mu);
    // Two burns, not one: the departure at the window, the arrival a transfer later. The arrival
    // is retrograde in the orbital frame - it is a braking burn - and that is the whole reason the
    // node carries a signed prograde component rather than a magnitude.
    const double departure = world.elapsed + std::max(0.0, map_frame.target.window);
    world.nodes.push_back(orbit::Node{departure, plan.dv1, 0.0});
    world.nodes.push_back(orbit::Node{departure + plan.transfer_time, -plan.dv2, 0.0});
    toast("Transfer planned: " + body.name);
}

void App::reset_run() {
    world = World{};
    world.attach_system(system, system_anchor);
    world.ship.assist = settings.assist;
    clear_effects();
    sync_ship_collider();
    sync_ports();
    zoom = settings.zoom_default;
    zoom_current = zoom;
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
    upload_mesh_library(renderer, models.library);
    sync_ship_collider();
    sync_ports();
    SDL_Log("models reloaded: %d meshes before, %d after", before, models.library.size());
    toast("models reloaded");
}

Camera camera_for(const World &world, Uint32 width, Uint32 height, float zoom, bool cinematic,
                  float pitch, const glm::dvec2 &follow) {
    Camera camera;
    camera.half_height = (cinematic ? CINEMATIC_HALF : FLIGHT_HALF) / zoom;
    camera.aspect = static_cast<float>(width) / static_cast<float>(height);
    // F1: the render origin is the follow point, not the ship. The ship is placed like every other
    // instance - as float(world - origin) - so it sits off centre by the lead and the deadzone, and
    // nothing Gm-scale reaches a float because the follow stays within a few hundred metres of it.
    camera.origin = glm::dvec3(follow.x, follow.y, 0.0);
    camera.target = glm::vec3(0.0f);
    camera.eye = orbit_eye(camera.half_height, pitch);
    return camera;
}



}  // namespace opra
