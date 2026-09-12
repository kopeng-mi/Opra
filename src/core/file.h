// File loading and asset path resolution.
#pragma once

#include <string>
#include <vector>

#include <SDL3/SDL.h>

namespace opra {

/** Reads a whole file, or exits: a missing asset is never a silent empty buffer. */
std::vector<Uint8> read_file(const std::string &path);

/** Resolves a path relative to the executable's directory. */
std::string asset_path(const std::string &relative);

}  // namespace opra
