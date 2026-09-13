#include "game/settings.h"

#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>

#include "core/log.h"

namespace opra {
namespace {

std::string g_path;

std::string resolve_path() {
    const char *local = SDL_getenv("LOCALAPPDATA");
    if (!local || !*local) return {};
    return std::string(local) + "\\Opra\\settings.ini";
}

bool parse_bool(const std::string &text, bool fallback) {
    if (text == "1" || text == "true" || text == "yes") return true;
    if (text == "0" || text == "false" || text == "no") return false;
    return fallback;
}

}  // namespace

const char *settings_path() {
    if (g_path.empty()) g_path = resolve_path();
    return g_path.empty() ? nullptr : g_path.c_str();
}

void Settings::load() {
    const char *path = settings_path();
    if (!path) return;
    std::ifstream file(path);
    if (!file) return;  // first run: the defaults stand

    std::string line;
    while (std::getline(file, line)) {
        const size_t split = line.find('=');
        if (split == std::string::npos) continue;
        const std::string key = line.substr(0, split);
        const std::string value = line.substr(split + 1);
        if (key == "camera_pitch") {
            const float parsed = static_cast<float>(std::atof(value.c_str()));
            // A value outside the slider's own range came from a hand-edited file or an older
            // build: keep the default rather than pin the camera to an extreme it cannot show.
            if (parsed >= 17.0f && parsed <= 88.0f) camera_pitch = parsed;
        }
        if (key == "assist") assist = parse_bool(value, assist);
        if (key == "reduced_motion") reduced_motion = parse_bool(value, reduced_motion);
        if (key == "msaa") msaa = std::atoi(value.c_str()) >= 4 ? 4 : 1;
        if (key == "show_stats") show_stats = parse_bool(value, show_stats);
    }
}

bool Settings::save() const {
    const char *path = settings_path();
    if (!path) return false;
    std::error_code error;
    std::filesystem::create_directories(std::filesystem::path(path).parent_path(), error);
    std::ofstream file(path, std::ios::trunc);
    if (!file) return false;
    file << "# opra settings\n";
    file << "camera_pitch=" << camera_pitch << "\n";
    file << "assist=" << (assist ? 1 : 0) << "\n";
    file << "reduced_motion=" << (reduced_motion ? 1 : 0) << "\n";
    file << "msaa=" << msaa << "\n";
    file << "show_stats=" << (show_stats ? 1 : 0) << "\n";
    return file.good();
}

}  // namespace opra
