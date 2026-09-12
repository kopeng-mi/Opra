#include "game/capture.h"

#include <cstdio>
#include <fstream>
#include <vector>

#include "core/log.h"
#include "game/settings.h"
#include "render/camera.h"
#include "render/orrery.h"
#include "sim/system.h"
#include "render/renderer.h"

namespace opra {

bool save_bmp(const char *path, const Uint8 *rgba, int w, int h) {
    const int row_bytes = w * 3;
    const int padding = (4 - (row_bytes % 4)) % 4;
    const int image_bytes = (row_bytes + padding) * h;
    std::vector<Uint8> out(54 + image_bytes, 0);

    out[0] = 'B';
    out[1] = 'M';
    const Uint32 file_size = static_cast<Uint32>(54 + image_bytes);
    const Uint32 pixel_offset = 54;
    const Uint32 header_size = 40;
    const Uint16 planes = 1, bpp = 24;
    std::memcpy(&out[2], &file_size, 4);
    std::memcpy(&out[10], &pixel_offset, 4);
    std::memcpy(&out[14], &header_size, 4);
    std::memcpy(&out[18], &w, 4);
    std::memcpy(&out[22], &h, 4);
    std::memcpy(&out[26], &planes, 2);
    std::memcpy(&out[28], &bpp, 2);
    std::memcpy(&out[34], &image_bytes, 4);

    for (int y = 0; y < h; ++y) {
        const Uint8 *src = rgba + static_cast<size_t>(h - 1 - y) * w * 4;
        Uint8 *dst = out.data() + 54 + static_cast<size_t>(y) * (row_bytes + padding);
        for (int x = 0; x < w; ++x) {
            dst[x * 3 + 0] = src[x * 4 + 2];
            dst[x * 3 + 1] = src[x * 4 + 1];
            dst[x * 3 + 2] = src[x * 4 + 0];
        }
    }

    std::ofstream file(path, std::ios::binary);
    if (!file) return false;
    file.write(reinterpret_cast<const char *>(out.data()), static_cast<std::streamsize>(out.size()));
    return file.good();
}


int run_capture(App &app, const char *path, const char *view, double seconds) {
    // The golden camera is the project's, not the pilot's: settings.ini carries whatever the last
    // live session left behind - a look's pitch among it - and a capture that inherits it turns
    // every flight view into a false diff. Pin the frame-content settings here and never mark
    // them dirty, so the pilot's own file is untouched on the way out.
    const Settings defaults;
    app.settings.camera_pitch = defaults.camera_pitch;
    app.settings.zoom_default = defaults.zoom_default;
    app.settings.reduced_motion = defaults.reduced_motion;
    app.world.ship.assist = defaults.assist;
    app.pitch = CAMERA_PITCH_DEFAULT;
    app.zoom = defaults.zoom_default;
    app.zoom_current = app.zoom;

        // The plate is a state, not a separate screen: every capture leaves it revealed, and
        // `--view startup` is the one that stays. The names are the graph's, bar the two the shot
        // script has always used - `help` for the manual, `models` for the viewer - and the two
        // that are not screens at all: `none` only hides the HUD, `cutter` is flight with the beam
        // running.
        app.title_reveal = 1.0f;
        if (SDL_strcmp(view, "none") == 0) {
            app.screen = ui::Screen::Flight;
            app.hud_hidden = true;
        } else if (SDL_strcmp(view, "cutter") == 0) {
            app.screen = ui::Screen::Flight;
        } else if (SDL_strcmp(view, "help") == 0) {
            app.screen = ui::screen_from_name("manual");
        } else if (SDL_strcmp(view, "models") == 0) {
            app.screen = ui::screen_from_name("viewer");
        } else if (SDL_strcmp(view, "startup") == 0) {
            app.screen = ui::Screen::Startup;
        } else {
            const ui::Screen named = ui::screen_from_name(view);
            // Flight is the fallback: a name the graph does not know is not the plate.
            app.screen = named == ui::Screen::Startup ? ui::Screen::Flight : named;
        }
        if (app.screen == ui::Screen::Map) {
            app.map_target = 4;  // Halberd: the plan's own example transfer
        }


        const Uint32 width = 1600, height = 900;
        const opra::FlightInput demo{1.0, 0.28, 0.0, false, false};
        const Real dt = 1.0 / 120.0;
        const int steps = static_cast<int>(seconds / dt);
        for (int i = 0; i < steps; ++i) {
            app.world.step(demo, dt);
            // The follow has to keep up with the world here too, or the camera this path builds
            // would be centred on the sector origin with the ship 13 Gm away.
            follow_step(app.follow,
                        glm::dvec2(app.world.ship.position.x, app.world.ship.position.y),
                        glm::dvec2(app.world.ship.velocity.x, app.world.ship.velocity.y), dt,
                        FollowParams{});
            app.now += dt;
        }
        app.camera = active_camera(app, width, height);
        // The map and the plate are built from the world as it stands after the scripted flight,
        // which is the same order the loop uses: the frame first, then the camera that frames it.
        if (app.screen == ui::Screen::Map || app.screen == ui::Screen::Startup) {
            app.map_frame = orrery_frame_for(app.world, app.true_scale, app.map_target);
            app.camera = active_camera(app, width, height);
        }
        if (SDL_strcmp(view, "body") == 0) {
            app.screen = ui::Screen::Map;
            // One planet at true scale, filling the chart: the golden that holds the albedo map,
            // the cloud deck and the night lights all in reach (plan-04 s3.4). Tessera is the one
            // body that carries all three.
            const int body = app.world.system.index_of("tessera");
            const Body &def = app.world.system.bodies[static_cast<size_t>(body)];
            orrery::Body mark;
            mark.name = def.name;
            // The chart's own origin: at the body's real position the true-scale glyph is sub-pixel
            // against a field that reaches its orbit, so the shot centres the body itself.
            mark.position = glm::dvec2(0.0);
            mark.radius = def.radius;
            // A body with no orbit of its own reads as the system's star in the orrery, and would
            // be drawn by the star pipeline: the mark carries a circular orbit at its own radius,
            // which classifies it a planet without pulling its real apoapsis into the field.
            mark.elements = orbit::Elements{};
            mark.elements.a = def.radius;
            mark.color = orrery::ink::VELLUM;
            mark.terrain_seed = static_cast<float>(def.terrain.seed);
            mark.terrain_amplitude = static_cast<float>(def.terrain.amplitude);
            // No air shell in this shot: at true scale the shell's additive glow washes the disc
            // out (a close-range air pass needs its own tuning, plan 4.4), and the surface itself
            // is what this golden holds.
            mark.scale_height = 0.0f;
            mark.atmosphere_top = 0.0f;
            mark.albedo_map = def.albedo_map;
            mark.cloud_map = def.cloud_map;
            mark.night_map = def.night_map;
            app.map_frame = orrery::Frame{};
            app.map_frame.t = app.world.elapsed;
            app.map_frame.true_scale = true;
            app.map_frame.ship_position = glm::dvec2(0.0);
            app.map_frame.bodies.push_back(mark);
        }
        // A moment of mining, so the beam and the fracture path appear in the frame.
        if (SDL_strcmp(view, "cutter") == 0) {
            // Point the hull at the nearest rock so the beam has something to bite.
            Real best = 1e18;
            const opra::Obstacle *nearest = nullptr;
            for (const opra::Obstacle &rock : app.world.rocks) {
                if (rock.z != 0 || rock.hp <= 0) continue;
                const Real range = std::hypot(rock.x - app.world.ship.position.x,
                                              rock.y - app.world.ship.position.y);
                if (range < best) {
                    best = range;
                    nearest = &rock;
                }
            }
            if (nearest) {
                const Real dx = nearest->x - app.world.ship.position.x;
                const Real dy = nearest->y - app.world.ship.position.y;
                app.world.ship.angle = std::atan2(-dx, dy);
            }
            app.mining = true;
            app.input.right = true;
            const size_t fragments_before = app.world.fragments.size();
            const size_t ore_before = app.world.ore.size();
            for (int i = 0; i < 480; ++i) {
                app.world.step(demo, dt);
                follow_step(app.follow,
                            glm::dvec2(app.world.ship.position.x, app.world.ship.position.y),
                            glm::dvec2(app.world.ship.velocity.x, app.world.ship.velocity.y), dt,
                            FollowParams{});
                app.camera = active_camera(app, width, height);
                update_app(app, width, height, dt);
                app.now += dt;
                // Stop on the break: the blast is a 0.8 s effect, so a capture that ran the whole
                // mining loop would photograph the rock's absence rather than the fracture. A small
                // rock leaves ore and no fragments, so both signals count.
                if (app.world.fragments.size() > fragments_before ||
                    app.world.ore.size() > ore_before) {
                    break;
                }
            }
            app.toast("Rock broken - ore in the drift");
        }

        SDL_GPUTexture *target =
            gpu::create_color(app.renderer.device, width, height, SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM, SDL_GPU_SAMPLECOUNT_1);
        ensure_depth(app.renderer, width, height);

        SDL_GPUCommandBuffer *cmd = SDL_AcquireGPUCommandBuffer(app.renderer.device.handle);
        render_app(app, cmd, target, SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM, width, height);

        const Uint32 byte_size = width * height * 4;
        SDL_GPUTransferBufferCreateInfo transfer_info{};
        transfer_info.usage = SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD;
        transfer_info.size = byte_size;
        SDL_GPUTransferBuffer *transfer = SDL_CreateGPUTransferBuffer(app.renderer.device.handle, &transfer_info);
        if (!transfer) fatal("SDL_CreateGPUTransferBuffer (download)");

        SDL_GPUCopyPass *copy = SDL_BeginGPUCopyPass(cmd);
        SDL_GPUTextureRegion region{};
        region.texture = target;
        region.w = width;
        region.h = height;
        region.d = 1;
        SDL_GPUTextureTransferInfo destination{};
        destination.transfer_buffer = transfer;
        destination.pixels_per_row = width;
        destination.rows_per_layer = height;
        SDL_DownloadFromGPUTexture(copy, &region, &destination);
        SDL_EndGPUCopyPass(copy);

        SDL_GPUFence *fence = SDL_SubmitGPUCommandBufferAndAcquireFence(cmd);
        if (!fence) fatal("SDL_SubmitGPUCommandBufferAndAcquireFence");
        SDL_WaitForGPUFences(app.renderer.device.handle, true, &fence, 1);
        SDL_ReleaseGPUFence(app.renderer.device.handle, fence);

        const void *mapped = SDL_MapGPUTransferBuffer(app.renderer.device.handle, transfer, false);
        const bool ok = save_bmp(path, static_cast<const Uint8 *>(mapped),
                                 static_cast<int>(width), static_cast<int>(height));
        SDL_UnmapGPUTransferBuffer(app.renderer.device.handle, transfer);
        SDL_ReleaseGPUTransferBuffer(app.renderer.device.handle, transfer);
        SDL_ReleaseGPUTexture(app.renderer.device.handle, target);
        std::printf(
            "screenshot: %s | view %s | sim %.1fs | hull %.0f fuel %.0f ore %.0f speed %.1f "
            "thrust %.2f fxruns %zu | fragments %zu ore %zu | %u instances %u runs %llu tri | "
            "submits %d\n",
            ok ? path : "FAILED", view, app.world.elapsed, app.world.ship.hull,
            app.world.ship.fuel, app.world.oreHeld, length(app.world.ship.velocity),
            app.world.ship.thrustLevel, app.renderer.effect_runs.size(),
            app.world.fragments.size(), app.world.ore.size(), app.renderer.instance_count,
            app.renderer.run_count,
            static_cast<unsigned long long>(app.renderer.triangle_count), gpu::submit_count());

    return 0;
}

}  // namespace opra
