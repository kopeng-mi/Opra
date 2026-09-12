#include "core/crash.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <string>

#include <SDL3/SDL.h>

#ifdef _WIN32
#include <windows.h>
#endif

namespace opra {
namespace {

const char *g_phase = "startup";

std::string crash_log_path() {
    const char *local = SDL_getenv("LOCALAPPDATA");
    if (!local || !*local) return "opra-crash.log";
    return std::string(local) + "\\Opra\\crash.log";
}

#ifdef _WIN32
LONG WINAPI handler(EXCEPTION_POINTERS *info) {
    const EXCEPTION_RECORD *record = info->ExceptionRecord;
    std::FILE *file = std::fopen(crash_log_path().c_str(), "a");
    if (file) {
        const std::time_t now = std::time(nullptr);
        char when[64] = {};
        std::strftime(when, sizeof when, "%Y-%m-%d %H:%M:%S", std::localtime(&now));
        const unsigned long long address =
            reinterpret_cast<unsigned long long>(record->ExceptionAddress);
        // Which module: a fault inside SDL or the D3D12 runtime reads very differently from one
        // inside the game, and the address alone does not say.
        char module[64] = "unknown";
        HMODULE owner = nullptr;
        if (GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                                   GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                               reinterpret_cast<LPCWSTR>(record->ExceptionAddress), &owner)) {
            char path[MAX_PATH] = {};
            if (GetModuleFileNameA(owner, path, MAX_PATH)) {
                const char *name = std::strrchr(path, '\\');
                std::snprintf(module, sizeof module, "%s", name ? name + 1 : path);
            }
        }
        const HMODULE self = GetModuleHandleW(nullptr);
        std::fprintf(file, "%s  code 0x%08lx  module %s  offset 0x%llx  phase '%s'\n", when,
                     static_cast<unsigned long>(record->ExceptionCode), module,
                     address - reinterpret_cast<unsigned long long>(self), g_phase);
        std::fflush(file);
        std::fclose(file);
    }
    // Let the default handler run too: a dialog and a non-zero exit code are what a player needs.
    return EXCEPTION_CONTINUE_SEARCH;
}
#endif

}  // namespace

void set_phase(const char *phase) { g_phase = phase; }

const char *last_phase() { return g_phase; }

void install_crash_handler() {
#ifdef _WIN32
    SetUnhandledExceptionFilter(handler);
    // Crashes on a worker thread reach the filter as well, so nothing else is needed here.
#endif
}

}  // namespace opra
