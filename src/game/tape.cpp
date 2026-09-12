#include "game/tape.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <string>
#include <utility>
#include <vector>

#include <glm/glm.hpp>
#include <nlohmann/json.hpp>

#include "core/crash.h"
#include "core/log.h"
#include "game/app.h"
#include "game/bindings.h"
#include "game/capture.h"
#include "game/config.h"
#include "game/input.h"
#include "game/settings.h"
#include "game/warp.h"
#include "gpu/gpu.h"
#include "render/renderer.h"
#include "ui/flow.h"

namespace opra {
namespace {

using json = nlohmann::json;

/** The tape's only clock: the sim's own fixed rate, one step a frame. */
constexpr double kStep = 1.0 / 120.0;
constexpr Uint32 kWidth = 1600;
constexpr Uint32 kHeight = 900;
constexpr SDL_GPUTextureFormat kFormat = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;

// ------------------------------------------------------------------- the tape, as data

/**
 * One beat: from `at` seconds exactly `keys` are down, until the next beat replaces them. A key the
 * game reads as an edge (the screen graph's own table) fires on the beat's first frame; one it reads
 * continuously (the drive, the cutter) stays down for the beat's whole span.
 */
struct Beat {
    double at;
    std::vector<SDL_Scancode> keys;
};

/** What a checkpoint asserts. The minimum probes hold when the reading reaches `expected`; Screen,
 *  Nodes and Docked are exact. */
enum class Probe {
    Screen,       // `screen` names the expected screen
    MinSpeed,     // m/s
    MinDistance,  // metres from where the run started
    MinOre,       // world.oreHeld
    MaxOre,
    MinChunks,    // world.ore: the rock's spill, before the collector takes it
    MinFragments, // world.fragments
    Nodes,        // exact size of world.nodes
    Docked,       // exactly the flag
    MinElapsed,   // sim seconds
};

struct Check {
    Probe probe;
    double expected;
    const char *screen;  // Probe::Screen only
};

/** A named point the beats reach: every check must hold there. */
struct Checkpoint {
    double at;
    std::string name;
    std::vector<Check> checks;
};

/** A named frame: it is rendered, read back and written as a BMP when the beats reach `at`. */
struct CaptureBeat {
    double at;
    std::string name;
};

/**
 * One tape. `duration` is the last second it runs; `continuation` names where the flow carries on
 * once the later phases land - the seam a tape for G12-G14 attaches to.
 */
struct Tape {
    std::string name;
    double duration;
    std::vector<Beat> beats;
    std::vector<Checkpoint> checkpoints;
    std::vector<CaptureBeat> captures;
    const char *continuation;
};

/** A floor: the reading must reach `expected`. */
Check at_least(Probe probe, double expected) { return Check{probe, expected, nullptr}; }

/** A screen, by the name ui/flow.h gives it. */
Check screen_is(const char *screen) { return Check{Probe::Screen, 0.0, screen}; }

/**
 * The keys a beat holds, named the way the game names them: a bound action resolves through
 * game/bindings.h, so the tape can never drift from the game's own key table. A raw scancode covers
 * the keys a screen reads for itself, which the bindings deliberately do not own - the plate's
 * Return is a chosen row, not a binding.
 */
std::vector<SDL_Scancode> bind(std::initializer_list<Action> actions,
                               std::initializer_list<SDL_Scancode> raw = {}) {
    std::vector<SDL_Scancode> keys;
    for (Action action : actions) {
        for (int i = 0; i < BINDING_COUNT; ++i) {
            const Binding &binding = BINDINGS[i];
            if (binding.action == action && binding.primary != SDL_SCANCODE_UNKNOWN) {
                keys.push_back(binding.primary);
                break;
            }
        }
    }
    keys.insert(keys.end(), raw.begin(), raw.end());
    return keys;
}

/**
 * The one tape: boot -> menu -> launch -> fly -> mine -> map -> plan.
 *
 * The times are the game's own deterministic arithmetic, not a recording: the ship starts on the
 * sector origin at angle -0.63 with the nearest plane rock at (252.1, 123.2), 280.6 m out. Turning
 * right for 72 frames at the Kestrel's 1.35 rad/s^2 yaw, then releasing, puts the nose on that rock
 * (attitude assist damps the spin over the same 72 frames, for 1.35 * (72/120)^2 = 0.486 rad, which
 * is the bearing). The drive then runs 2 s, coasts to 199.5 m, and the cutter bites; the kill burn
 * stops the hull on the debris at 280 m, inside the collector's 62 m envelope, so the ore comes
 * aboard. Cinder is then selected on the map and the transfer's two burns are planned.
 */
const Tape &flow_tape() {
    static const Tape tape = {
        "flow",
        19.2,
        {
            {0.00, bind({})},
            // Launch: the plate's first row is Continue and Return chooses it, so MENU_FLOW's
            // Startup -> Flight edge takes the screen on the frame after this one.
            {1.60, bind({}, {SDL_SCANCODE_RETURN})},
            // The turn: 72 frames of starboard yaw, then the release that lets assist damp it.
            {1.70, bind({Action::TurnRight})},
            {2.30, bind({})},
            // The drive: 2 s of thrust, a coast down to cutting range, then the burn that stops the
            // hull beside the rock it broke.
            {2.90, bind({Action::DriveForward})},
            {4.90, bind({})},
            {9.95, bind({Action::Cutter})},
            {10.90, bind({Action::Cutter, Action::KillVelocity})},
            {14.20, bind({})},
            // The map, then Cinder, then the transfer's two burns. A beat each, on its own frame:
            // the map's row reads the target from the frame before the one the plan lands on.
            {18.00, bind({Action::Map})},
            {18.10, bind({})},
            {18.20, bind({Action::CycleContact})},
            {18.30, bind({})},
            {18.40, bind({Action::PlanNode})},
            {18.50, bind({})},
        },
        {
            {0.50, "boot", {screen_is("startup"), at_least(Probe::MaxOre, 0.0),
                            Check{Probe::Docked, 0.0, nullptr}}},
            {1.50, "menu", {screen_is("startup")}},
            {2.00, "launch", {screen_is("flight"), Check{Probe::Docked, 0.0, nullptr}}},
            // The drive opens at 2.90 and this lands at 4.00: 1.10 s at the Kestrel's 16.45 m/s^2 is
            // 18.1 m/s, so 17 is the floor that proves the drive is running and the ship is moving
            // under it. The coast and the kill burn carry the rest of the flight.
            {4.00, "fly", {screen_is("flight"), at_least(Probe::MinSpeed, 17.0),
                           at_least(Probe::MinDistance, 5.0)}},
            {11.50, "break", {screen_is("flight"), at_least(Probe::MinChunks, 1.0)}},
            {17.50, "mine", {screen_is("flight"), at_least(Probe::MinOre, 1.0)}},
            {18.70, "map", {screen_is("map")}},
            {18.90, "plan", {Check{Probe::Nodes, 2.0, nullptr}, at_least(Probe::MinElapsed, 15.0)}},
        },
        {
            {0.50, "boot"},
            {2.00, "launch"},
            {4.00, "fly"},
            {17.50, "mine"},
            {18.70, "map"},
            {18.90, "plan"},
        },
        // G12-G14: warp -> arrive -> deorbit -> land. They belong between `plan` and a landed
        // checkpoint, and the beats above stop exactly where the map's plan is made.
        "warp -> arrive -> deorbit -> land (G12-G14)",
    };
    return tape;
}

const Tape &tape_named(const char *name) {
    if (std::strcmp(name, "flow") == 0) return flow_tape();
    // Not fatal(): this is the command line, not SDL, and the message names the tapes that exist.
    std::fprintf(stderr, "tape: unknown tape '%s' (known: flow)\n", name);
    std::exit(1);
}

// ------------------------------------------------------------------- running one

/** What one checkpoint saw. Recorded whole, so a failure prints the delta and two runs compare
 *  field by field. */
struct Seen {
    std::string name;
    double at = 0.0;
    bool ok = true;
    std::string failure;
    std::string screen;
    double speed = 0.0;
    double distance = 0.0;
    double ore = 0.0;
    size_t chunks = 0;
    size_t fragments = 0;
    size_t nodes = 0;
    bool docked = false;
    double warp = 1.0;
    double elapsed = 0.0;
};

/** Reads every quantity the probes can ask for, whether or not this checkpoint asks. */
Seen read_state(const App &app, const glm::dvec2 &start, const Checkpoint &point) {
    Seen seen;
    seen.name = point.name;
    seen.at = point.at;
    seen.screen = ui::screen_name(app.screen);
    seen.speed = length(app.world.ship.velocity);
    seen.distance = glm::length(glm::dvec2(app.world.ship.position.x, app.world.ship.position.y) -
                                start);
    seen.ore = app.world.oreHeld;
    seen.chunks = app.world.ore.size();
    seen.fragments = app.world.fragments.size();
    seen.nodes = app.world.nodes.size();
    seen.docked = app.world.docked;
    seen.warp = app.warp.rate();
    seen.elapsed = app.world.elapsed;
    return seen;
}

/** The clause a failed probe reads as, or empty when it holds. The checkpoint's name prefixes it,
 *  so the sentence names the beat as well as the quantity - the selftest's own shape. */
std::string sentence_for(const Checkpoint &point, const Seen &seen, const Check &check) {
    char line[256];
    char wanted[64];
    char got[64];
    const auto fail = [&](const char *quantity, const char *expected, const char *value) {
        std::snprintf(line, sizeof line, "FAIL tape: checkpoint %s %s expected %s got %s",
                      point.name.c_str(), quantity, expected, value);
        return std::string(line);
    };
    switch (check.probe) {
        case Probe::Screen:
            if (seen.screen == check.screen) return {};
            return fail("screen", check.screen, seen.screen.c_str());
        case Probe::MinSpeed:
            if (seen.speed >= check.expected) return {};
            std::snprintf(wanted, sizeof wanted, ">= %.3f", check.expected);
            std::snprintf(got, sizeof got, "%.3f", seen.speed);
            return fail("speed", wanted, got);
        case Probe::MinDistance:
            if (seen.distance >= check.expected) return {};
            std::snprintf(wanted, sizeof wanted, ">= %.3f", check.expected);
            std::snprintf(got, sizeof got, "%.3f", seen.distance);
            return fail("distance", wanted, got);
        case Probe::MinOre:
            if (seen.ore >= check.expected) return {};
            std::snprintf(wanted, sizeof wanted, ">= %.3f", check.expected);
            std::snprintf(got, sizeof got, "%.3f", seen.ore);
            return fail("ore", wanted, got);
        case Probe::MaxOre:
            if (seen.ore <= check.expected) return {};
            std::snprintf(wanted, sizeof wanted, "<= %.3f", check.expected);
            std::snprintf(got, sizeof got, "%.3f", seen.ore);
            return fail("ore", wanted, got);
        case Probe::MinChunks:
            if (seen.chunks >= static_cast<size_t>(check.expected)) return {};
            std::snprintf(wanted, sizeof wanted, ">= %.0f", check.expected);
            std::snprintf(got, sizeof got, "%.0f", static_cast<double>(seen.chunks));
            return fail("ore chunks", wanted, got);
        case Probe::MinFragments:
            if (seen.fragments >= static_cast<size_t>(check.expected)) return {};
            std::snprintf(wanted, sizeof wanted, ">= %.0f", check.expected);
            std::snprintf(got, sizeof got, "%.0f", static_cast<double>(seen.fragments));
            return fail("fragments", wanted, got);
        case Probe::Nodes:
            if (seen.nodes == static_cast<size_t>(check.expected)) return {};
            std::snprintf(wanted, sizeof wanted, "%.0f", check.expected);
            std::snprintf(got, sizeof got, "%.0f", static_cast<double>(seen.nodes));
            return fail("nodes", wanted, got);
        case Probe::Docked:
            if (seen.docked == (check.expected != 0.0)) return {};
            std::snprintf(wanted, sizeof wanted, "%.0f", check.expected);
            std::snprintf(got, sizeof got, "%.0f", seen.docked ? 1.0 : 0.0);
            return fail("docked", wanted, got);
        case Probe::MinElapsed:
            if (seen.elapsed >= check.expected) return {};
            std::snprintf(wanted, sizeof wanted, ">= %.3f", check.expected);
            std::snprintf(got, sizeof got, "%.3f", seen.elapsed);
            return fail("elapsed", wanted, got);
    }
    return {};
}

/** The human-readable tail of a passing checkpoint line. */
std::string describe(const Seen &seen) {
    char line[320];
    std::snprintf(line, sizeof line,
                  "screen=%s speed=%.3f distance=%.3f ore=%.3f nodes=%zu elapsed=%.3f",
                  seen.screen.c_str(), seen.speed, seen.distance, seen.ore, seen.nodes,
                  seen.elapsed);
    return line;
}

/**
 * The loop's own stepping, at one fixed step a frame. The tape has no wall clock to sample, so the
 * accumulator and the frame-time clamp are gone; everything else - the rail, the automatic drops,
 * the sim's fixed rate - is exactly what run_loop does.
 */
void step(App &app) {
    if (app.screen != ui::Screen::Flight) return;
    const FlightInput flight = flight_input_from(app.input);
    const bool thrusting = flight.thrust != 0.0 || flight.turn != 0.0 || flight.strafe != 0.0 ||
                           flight.brake;
    if (app.warp.railed()) {
        const double advance = std::min(kStep * app.warp.rate(), WARP_MAX_ADVANCE);
        app.world.warp_step(advance);
        app.now += advance;
        if (app.warp.update(thrusting, app.world.contact_recent(), app.world.soi_switched,
                            app.world.air_density > 0.0)) {
            app.toast("Warp dropped to 1x");
        }
        return;
    }
    app.world.step(flight, kStep);
    app.now += kStep;
    app.warp.update(thrusting, app.world.contact_recent(), app.world.soi_switched,
                    app.world.air_density > 0.0);
}

/** Where a capture is written: beside the report, as `<tape>.<beat>.bmp`, or under artifacts/tape
 *  when the run keeps no report at all. */
std::string capture_path(const char *json_path, const std::string &tape,
                         const std::string &beat) {
    std::filesystem::path base = "artifacts/tape";
    if (json_path) {
        const std::filesystem::path report(json_path);
        base = report.parent_path().empty() ? std::filesystem::path(".") : report.parent_path();
    }
    return (base / (tape + "." + beat + ".bmp")).string();
}

/**
 * Reads a rendered offscreen frame back and writes it as a BMP. It uses its own command buffer, so
 * the frame's render is already submitted and the readback cannot change what it reads.
 */
bool write_capture(App &app, SDL_GPUTexture *target, const std::string &path) {
    std::error_code error;
    const std::filesystem::path file(path);
    if (file.has_parent_path()) std::filesystem::create_directories(file.parent_path(), error);

    SDL_GPUCommandBuffer *cmd = SDL_AcquireGPUCommandBuffer(app.renderer.device.handle);
    if (!cmd) fatal("tape: SDL_AcquireGPUCommandBuffer");
    SDL_GPUTransferBufferCreateInfo transfer_info{};
    transfer_info.usage = SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD;
    transfer_info.size = kWidth * kHeight * 4;
    SDL_GPUTransferBuffer *transfer =
        SDL_CreateGPUTransferBuffer(app.renderer.device.handle, &transfer_info);
    if (!transfer) fatal("tape: SDL_CreateGPUTransferBuffer");

    SDL_GPUCopyPass *copy = SDL_BeginGPUCopyPass(cmd);
    SDL_GPUTextureRegion region{};
    region.texture = target;
    region.w = kWidth;
    region.h = kHeight;
    region.d = 1;
    SDL_GPUTextureTransferInfo destination{};
    destination.transfer_buffer = transfer;
    destination.pixels_per_row = kWidth;
    destination.rows_per_layer = kHeight;
    SDL_DownloadFromGPUTexture(copy, &region, &destination);
    SDL_EndGPUCopyPass(copy);

    SDL_GPUFence *fence = SDL_SubmitGPUCommandBufferAndAcquireFence(cmd);
    if (!fence) fatal("tape: SDL_SubmitGPUCommandBufferAndAcquireFence");
    SDL_WaitForGPUFences(app.renderer.device.handle, true, &fence, 1);
    SDL_ReleaseGPUFence(app.renderer.device.handle, fence);

    const void *mapped = SDL_MapGPUTransferBuffer(app.renderer.device.handle, transfer, false);
    const bool ok = mapped && save_bmp(path.c_str(), static_cast<const Uint8 *>(mapped),
                                       static_cast<int>(kWidth), static_cast<int>(kHeight));
    SDL_UnmapGPUTransferBuffer(app.renderer.device.handle, transfer);
    SDL_ReleaseGPUTransferBuffer(app.renderer.device.handle, transfer);
    return ok;
}

void write_report(const char *path, const Tape &tape, const std::vector<Seen> &seen,
                  const std::vector<std::pair<CaptureBeat, std::string>> &captures, int failures) {
    json report;
    report["tape"] = tape.name;
    report["ok"] = failures == 0;
    report["duration"] = tape.duration;
    report["continuation"] = tape.continuation;
    report["checkpoints"] = json::array();
    for (const Seen &at : seen) {
        json entry;
        entry["name"] = at.name;
        entry["at"] = at.at;
        entry["ok"] = at.ok;
        entry["failure"] = at.failure;
        entry["screen"] = at.screen;
        entry["speed"] = at.speed;
        entry["distance"] = at.distance;
        entry["ore"] = at.ore;
        entry["chunks"] = at.chunks;
        entry["fragments"] = at.fragments;
        entry["nodes"] = at.nodes;
        entry["docked"] = at.docked;
        entry["warp"] = at.warp;
        entry["elapsed"] = at.elapsed;
        report["checkpoints"].push_back(std::move(entry));
    }
    report["captures"] = json::array();
    for (const auto &[capture, file] : captures) {
        report["captures"].push_back(
            {{"name", capture.name}, {"at", capture.at}, {"path", file}});
    }

    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out) {
        std::fprintf(stderr, "tape: cannot write the report to %s\n", path);
        return;
    }
    out << report.dump(2) << "\n";
}

}  // namespace

int run_tape(App &app, const char *name, const char *json_path) {
    const Tape &tape = tape_named(name);

    // The tape runs the shipped settings, not the player's file: attitude assist decides where the
    // ship ends up, so a checkpoint has to mean the same thing on every machine. The dirty flag is
    // cleared so nothing of the run is written back to the player's settings.
    app.settings = Settings{};
    app.apply_settings();
    app.settings_dirty = false;
    app.zoom = app.settings.zoom_default;
    app.zoom_current = app.zoom;
    app.world.ship.assist = app.settings.assist;

    SDL_GPUTexture *target =
        gpu::create_color(app.renderer.device, kWidth, kHeight, kFormat, SDL_GPU_SAMPLECOUNT_1);
    if (!target) fatal("tape: SDL_CreateGPUTexture");
    ensure_depth(app.renderer, kWidth, kHeight);

    const glm::dvec2 start(app.world.ship.position.x, app.world.ship.position.y);
    std::vector<Seen> seen;
    std::vector<std::pair<CaptureBeat, std::string>> captures;
    std::vector<SDL_Scancode> held;
    size_t next_beat = 0;
    size_t next_check = 0;
    size_t next_capture = 0;
    int failures = 0;
    const int frames = static_cast<int>(std::lround(tape.duration / kStep));

    std::printf("tape %s: %zu checkpoints, %zu captures, %.1fs\n", tape.name.c_str(),
                tape.checkpoints.size(), tape.captures.size(), tape.duration);
    for (int frame = 0; frame < frames; ++frame) {
        const double now = frame * kStep;
        // A beat or a checkpoint lands on the frame its time falls in. The half step keeps the
        // 1/120 clock from pushing one a whole frame late when the division rounds down.
        const double edge = now + kStep * 0.5;
        while (next_beat < tape.beats.size() && tape.beats[next_beat].at <= edge) {
            held = tape.beats[next_beat].keys;
            ++next_beat;
        }

        app.input.begin_frame();
        std::memset(app.input.keys, 0, sizeof app.input.keys);
        for (SDL_Scancode key : held) app.input.keys[key] = true;
        app.input.left = false;
        app.input.right = false;
        app.input.pointer_valid = false;

        set_phase("tape step");
        step(app);
        app.camera = active_camera(app, kWidth, kHeight);
        app.renderer.samples = app.pending_samples;
        set_phase("tape update");
        update_app(app, kWidth, kHeight, kStep);

        const bool capture_lands =
            next_capture < tape.captures.size() && tape.captures[next_capture].at <= edge;
        // The plate's rows are chosen in the draw - build_title is where a row's Return is read -
        // so the startup screen renders every frame. Every other screen is drawn on the beats that
        // photograph it; the sim and the update path run on all of them either way.
        if (app.screen == ui::Screen::Startup || capture_lands) {
            SDL_GPUCommandBuffer *cmd = SDL_AcquireGPUCommandBuffer(app.renderer.device.handle);
            if (!cmd) fatal("tape: SDL_AcquireGPUCommandBuffer (render)");
            set_phase("tape render");
            render_app(app, cmd, target, kFormat, kWidth, kHeight);
            gpu::submit(cmd);
        }
        if (capture_lands) {
            const CaptureBeat &capture = tape.captures[next_capture];
            const std::string path = capture_path(json_path, tape.name, capture.name);
            if (!write_capture(app, target, path)) {
                std::fprintf(stderr, "tape: cannot write %s\n", path.c_str());
            }
            captures.emplace_back(capture, path);
            ++next_capture;
        }

        while (next_check < tape.checkpoints.size() && tape.checkpoints[next_check].at <= edge) {
            const Checkpoint &point = tape.checkpoints[next_check];
            ++next_check;
            Seen at = read_state(app, start, point);
            for (const Check &check : point.checks) {
                at.failure = sentence_for(point, at, check);
                if (!at.failure.empty()) break;
            }
            at.ok = at.failure.empty();
            if (!at.ok) {
                std::printf("%s\n", at.failure.c_str());
                ++failures;
            } else {
                std::printf("  %-7s %.2fs  ok  %s\n", at.name.c_str(), at.at,
                            describe(at).c_str());
            }
            seen.push_back(std::move(at));
            if (failures > 0) break;
        }
        if (failures > 0) break;
    }

    if (json_path) write_report(json_path, tape, seen, captures, failures);
    std::printf("tape %s: %s (%d checkpoint%s failed of %zu)\n", tape.name.c_str(),
                failures == 0 ? "PASS" : "FAIL", failures, failures == 1 ? "" : "s", seen.size());
    std::printf("continuation: %s\n", tape.continuation);
    SDL_ReleaseGPUTexture(app.renderer.device.handle, target);
    return failures;
}

}  // namespace opra
