// Offscreen capture: render one frame, read it back, write a BMP. Used by --screenshot.
#pragma once

#include <SDL3/SDL.h>

#include "game/app.h"

namespace opra {

/** Writes a 24-bit bottom-up BMP from an RGBA8 buffer. */
bool save_bmp(const char *path, const Uint8 *rgba, int width, int height);

/**
 * Simulates `seconds` of a scripted flight, opens the requested screen, renders one frame
 * offscreen and writes it. `view` is flight, chart, help, cutter or none.
 */
int run_capture(App &app, const char *path, const char *view, double seconds);

}  // namespace opra
