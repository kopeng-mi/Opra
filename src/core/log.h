// Logging and fatal exits. The only place that decides how a hard failure reports itself.
#pragma once

#include <SDL3/SDL.h>

#include <cstdio>
#include <cstdlib>

namespace opra {

/** Reports the current SDL error and exits. Used for conditions the program cannot continue past. */
[[noreturn]] inline void fatal(const char *what) {
    SDL_Log("%s failed: %s", what, SDL_GetError());
    std::fprintf(stderr, "%s failed: %s\n", what, SDL_GetError());
    std::exit(1);
}

}  // namespace opra
