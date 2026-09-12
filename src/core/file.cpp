#include "core/file.h"

#include <filesystem>

#include "core/log.h"

namespace opra {

std::vector<Uint8> read_file(const std::string &path) {
    size_t size = 0;
    void *data = SDL_LoadFile(path.c_str(), &size);
    if (!data) fatal(("SDL_LoadFile " + path).c_str());
    std::vector<Uint8> out(static_cast<Uint8 *>(data), static_cast<Uint8 *>(data) + size);
    SDL_free(data);
    return out;
}

std::string asset_path(const std::string &relative) {
    const char *base = SDL_GetBasePath();
    if (!base) return relative;
    const std::string beside_exe = std::string(base) + relative;
    if (std::filesystem::exists(beside_exe)) return beside_exe;
    // Development layout: the executable sits in build/<config>/, the assets in the source tree
    // two levels up. A shipped build keeps them beside the executable.
    const std::filesystem::path source =
        std::filesystem::path(base) / ".." / ".." / relative;
    std::error_code error;
    if (std::filesystem::exists(source, error)) return source.string();
    return beside_exe;
}

}  // namespace opra
