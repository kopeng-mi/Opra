// The interactive screens, built on ui/ui.h: the pause menu and the settings screen.
#pragma once

#include "game/settings.h"
#include "ui/ui.h"

namespace opra::ui {

/** What the pause menu asked for this frame. */
struct PauseResult {
    bool resume = false;
    bool open_settings = false;
    bool quit = false;
};

/** Pause: resume, settings, quit. Space or Esc resumes. */
PauseResult build_pause(Context &ui, const Rect &screen);

/** Settings: default zoom, assist, reduced motion, MSAA, debug stats. */
struct SettingsResult {
    bool back = false;
    /** True on the frame a value changed: the caller applies and persists it. */
    bool changed = false;
};

SettingsResult build_settings(Context &ui, const Rect &screen, Settings &settings);

/** The frame budget, drawn in the corner when the debug stats setting is on. */
void build_stats(Context &ui, const Rect &screen, float fps, unsigned instances, unsigned runs,
                 unsigned long long triangles, int submits);

}  // namespace opra::ui
