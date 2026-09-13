// Opra — entry point: arguments, initialisation, the frame loop, shutdown. Nothing else.
// Flags: --selftest               run the headless assert suite and exit
//        --screenshot <file.bmp>  simulate, render one frame offscreen, write a BMP, exit
//        --seconds <n>            seconds of scripted flight before the screenshot lands
//        --view flight|chart|help|cutter|none|pause|settings   screen for the capture
//        --zoom <n>               starting camera scale (0.6 .. 3.0)
//        --frames <n>             run n frames then exit
//        --hidden                 create the window hidden
//        --debug                  enable GPU validation
//        --dump-models            print each model's bounds
//        --tape <name>            run a scripted input tape headless instead of the loop
//        --json <path>            write the tape's checkpoints and captures as JSON

#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>

#include <algorithm>
#include <cstdlib>
#include <cstring>

#include "core/crash.h"
#include "core/log.h"
#include "game/app.h"
#include "game/capture.h"
#include "game/loop.h"
#include "game/config.h"
#include "game/tape.h"
#include "gpu/gpu.h"
#include "render/renderer.h"
#include "selftest.h"

namespace {

constexpr int kWindowWidth = 1600;
constexpr int kWindowHeight = 900;

/** Everything the command line can say. */
struct Options {
    const char *screenshot = nullptr;
    const char *view = "flight";
    double seconds = 0.0;
    /** 0 means "whatever the settings say". */
    float zoom = 0.0f;
    /** 0 keeps the default; 1..3 sets the HUD's density for the capture (F10). */
    int density = 0;
    int frame_limit = -1;
    bool hidden = false;
    bool debug_gpu = false;
    bool dump_models = false;
    bool selftest = false;
    /** The scripted input tape to run instead of the loop, or null. */
    const char *tape = nullptr;
    /** Where the tape's report is written, or null to keep it on stdout. */
    const char *json = nullptr;
};

Options parse_options(int argc, char **argv) {
    Options options;
    for (int i = 1; i < argc; ++i) {
        const char *arg = argv[i];
        const bool has_value = i + 1 < argc;
        if (SDL_strcmp(arg, "--selftest") == 0) {
            options.selftest = true;
        } else if (SDL_strcmp(arg, "--dump-models") == 0) {
            options.dump_models = true;
        } else if (SDL_strcmp(arg, "--hidden") == 0) {
            options.hidden = true;
        } else if (SDL_strcmp(arg, "--debug") == 0) {
            options.debug_gpu = true;
        } else if (has_value && SDL_strcmp(arg, "--screenshot") == 0) {
            options.screenshot = argv[++i];
        } else if (has_value && SDL_strcmp(arg, "--view") == 0) {
            options.view = argv[++i];
        } else if (has_value && SDL_strcmp(arg, "--seconds") == 0) {
            options.seconds = std::atof(argv[++i]);
        } else if (has_value && SDL_strcmp(arg, "--zoom") == 0) {
            options.zoom = static_cast<float>(std::atof(argv[++i]));
        } else if (has_value && SDL_strcmp(arg, "--frames") == 0) {
            options.frame_limit = std::atoi(argv[++i]);
        } else if (has_value && SDL_strcmp(arg, "--density") == 0) {
            options.density = std::atoi(argv[++i]);
        } else if (has_value && SDL_strcmp(arg, "--tape") == 0) {
            options.tape = argv[++i];
        } else if (has_value && SDL_strcmp(arg, "--json") == 0) {
            options.json = argv[++i];
        }
    }
    return options;
}

}  // namespace

int main(int argc, char **argv) {
    const Options options = parse_options(argc, argv);
    if (options.selftest) return opra::run_selftest();

    opra::install_crash_handler();
    SDL_SetAppMetadata("Opra", "0.1.0", "dev.lostfleet.opra");
    if (!SDL_Init(SDL_INIT_VIDEO)) opra::fatal("SDL_Init");
    if (!TTF_Init()) opra::fatal("TTF_Init");

    opra::App app;
    if (options.zoom > 0.0f) {
        // --zoom is a multiple of the home framing: 1 is the 150 m flight shot (plan 05 s3.1).
        app.half_height = opra::clamp_half_height(opra::HOME_HALF * options.zoom);
        app.half_height_current = app.half_height;
    }
    // A tape draws offscreen and reads its frames back, so its window is never shown.
    app.init(options.hidden || options.tape != nullptr, options.debug_gpu);
    if (options.density > 0) {
        // The capture path is how the golden images cover all three levels (F7, F10).
        app.density = static_cast<opra::Density>(std::clamp(options.density, 1, 3));
    }

    if (options.dump_models) opra::dump_models(app.models, app.world);

    int exit_code = 0;
    if (options.tape) {
        exit_code = opra::run_tape(app, options.tape, options.json);
    } else if (options.screenshot) {
        exit_code =
            opra::run_capture(app, options.screenshot, options.view, options.seconds);
    } else {
        opra::run_loop(app, options.debug_gpu, options.frame_limit);
    }

    app.shutdown();
    return exit_code;
}
