// The frame loop: poll, step the simulation at a fixed rate, draw, repeat.
#pragma once

#include "game/app.h"

namespace opra {

/** Runs until the window closes, the menu asks to quit, or `frame_limit` frames have been drawn
 *  (a negative limit means no limit). `debug` adds the per-frame budget log. */
void run_loop(App &app, bool debug, int frame_limit);

}  // namespace opra
