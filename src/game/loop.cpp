#include "game/loop.h"

#include <algorithm>

#include "core/crash.h"
#include "core/log.h"
#include "game/bindings.h"
#include "game/input.h"
#include "gpu/gpu.h"
#include "render/renderer.h"

namespace opra {

void run_loop(App &app, bool debug, int frame_limit) {
    const Real dt = 1.0 / 120.0;
    double accumulator = 0.0;
    Uint64 previous = SDL_GetTicks();
    Uint64 last_frame_ticks = previous;
    bool running = true;
    int frames = 0;
    int debug_frames = 0;
    int previous_submits = gpu::submit_count();

    while (running) {
        set_phase("poll events");
        app.input.begin_frame();
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (!apply_event(event, app.input)) running = false;
        }

        const Uint64 now = SDL_GetTicks();
        // Real seconds since the last frame, clamped so a stall (a breakpoint, a resize) cannot
        // hand the update path a huge step. The simulation still steps at the fixed dt below.
        const double elapsed = std::min(static_cast<double>(now - previous) / 1000.0, 0.25);
        accumulator = std::min(accumulator + elapsed, 0.25);
        previous = now;

        SDL_GPUCommandBuffer *cmd = SDL_AcquireGPUCommandBuffer(app.renderer.device.handle);
        if (!cmd) fatal("SDL_AcquireGPUCommandBuffer");
        SDL_GPUTexture *swapchain = nullptr;
        Uint32 width = 0, height = 0;
        // Waiting throttles the loop to the display and avoids burning a submit on every spin of a
        // hidden or minimised window, which an immediate acquire would return empty for.
        if (!SDL_WaitAndAcquireGPUSwapchainTexture(cmd, app.window, &swapchain, &width, &height) ||
            swapchain == nullptr) {
            SDL_CancelGPUCommandBuffer(cmd);
            continue;
        }

        set_phase("step simulation");
        if (app.screen != ui::Screen::Flight) {
            accumulator = 0.0;
        } else if (app.warp.railed()) {
            const FlightInput flight = flight_input_from(app.input);
            const bool thrusting = flight.thrust != 0.0 || flight.turn != 0.0 ||
                                   flight.strafe != 0.0 || flight.brake;
            // Above 10x the frame is one analytic advance along the conic, not thousands of fixed
            // steps: the rail would otherwise have to run 120 * rate integrations per second and
            // would still accumulate drift, which is what E2 chose the conic to avoid.
            const double advance = std::min(elapsed * app.warp.rate(), WARP_MAX_ADVANCE);
            app.world.warp_step(advance);
            app.now += advance;
            accumulator = 0.0;
            // s2.7's new rule: any round or torpedo in flight is real time - a warp step would
            // advance them past what their swept segments can be trusted over.
            if (app.world.combat_active()) {
                if (app.warp.drop_to_real_time(Warp::Drop::Zoom)) app.toast("Warp dropped to 1x - weapons live");
            } else if (app.warp.update(thrusting, app.world.contact_recent(), app.world.soi_switched,
                                       app.world.air_density > 0.0)) {
                app.toast("Warp dropped to 1x");
            }
        } else {
            const FlightInput flight = flight_input_from(app.input);
            // Real time scaled by the rail: below the railed threshold the sim still steps at its
            // fixed rate, just more often, so thrust and contacts keep their instant response.
            accumulator = std::min(
                accumulator + std::min(elapsed * app.warp.rate(), 0.25), 0.25);
            while (accumulator >= dt) {
                app.world.step(flight, dt);
                app.now += dt;
                accumulator -= dt;
            }
            const bool thrusting = flight.thrust != 0.0 || flight.turn != 0.0 ||
                                   flight.strafe != 0.0 || flight.brake;
            if (app.world.combat_active()) {
                if (app.warp.drop_to_real_time(Warp::Drop::Zoom)) {
                    app.toast("Warp dropped to 1x - weapons live");
                }
            } else {
                app.warp.update(thrusting, app.world.contact_recent(), app.world.soi_switched,
                                app.world.air_density > 0.0);
            }
        }

        // Built from the state this frame will draw: stepping first keeps the ship centred.
        app.camera = active_camera(app, width, height);
        set_phase("update");
        update_app(app, width, height, elapsed);

        set_phase("ensure targets");
        // D-4: the sample count changes here, before the depth target is sized for it. Applying it
        // mid-frame left a 4x colour target bound against a 1x depth texture.
        app.renderer.samples = app.pending_samples;
        ensure_depth(app.renderer, width, height);
        set_phase("render");
        render_app(app, cmd, swapchain, app.renderer.device.swap_format, width, height);
        set_phase("submit");
        gpu::submit(cmd);

        // Frame time for the debug panel, smoothed over roughly a second.
        const float frame_seconds = static_cast<float>(now - last_frame_ticks) / 1000.0f;
        last_frame_ticks = now;
        if (frame_seconds > 0.0f) {
            const float instant = 1.0f / frame_seconds;
            app.fps = app.fps > 0.0f ? app.fps * 0.9f + instant * 0.1f : instant;
        }
        if (debug && debug_frames < 6) {
            ++debug_frames;
            SDL_Log("frame %d: %d submits (%d this frame) | %u instances in %u runs | %llu tri",
                    debug_frames, gpu::submit_count(), gpu::submit_count() - previous_submits,
                    app.renderer.instance_count, app.renderer.run_count,
                    static_cast<unsigned long long>(app.renderer.triangle_count));
            previous_submits = gpu::submit_count();
        }
        if (app.wants_quit) running = false;
        if (frame_limit > 0 && ++frames >= frame_limit) running = false;
    }
}

}  // namespace opra
