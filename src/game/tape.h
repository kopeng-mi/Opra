// The scripted input tape: the fourth test layer of PLAN-03 §5.2. A tape is data - a list of beats
// keyed to a fixed 120 Hz clock, each holding the game's own keys, plus named checkpoints that
// assert the state the beats reached and named capture beats that write a frame. `--tape` runs one
// in place of the frame loop, with no wall clock and no swapchain, so the same tape gives the same
// checkpoints and the same pixels every run. tools/drive_input.py is the runner.
#pragma once

namespace opra {

struct App;

/**
 * Runs the named tape against a started App. Every frame is one fixed 120 Hz step, driven through
 * App::input so the game's own handlers - including ui/flow.h's tables - do the work; each frame
 * renders offscreen, and a capture beat reads the frame back and writes it with save_bmp.
 *
 * The first failed checkpoint stops the run and prints the shape the selftest uses,
 *
 *     FAIL tape: <what> expected <x> got <y>
 *
 * so the output reads like a test. `json_path`, when not null, receives the checkpoints and the
 * capture paths as JSON - what tools/drive_input.py reads. Returns the failure count, which is the
 * process's exit code.
 */
int run_tape(App &app, const char *name, const char *json_path);

}  // namespace opra
