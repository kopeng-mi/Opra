// Player settings, persisted as plain key=value under %LOCALAPPDATA%/Opra/settings.ini.
#pragma once

#include "game/config.h"

namespace opra {

struct Settings {
    /** Where the 0 key and a fresh run put the camera. */
    float zoom_default = config::ZOOM_DEFAULT;
    /**
     * The camera's orbit pitch in degrees, read once and fixed during flight (F1): pitch is a
     * framing choice, not a control. The model viewer keeps its own turntable.
     */
    float camera_pitch = 32.0f;
    /** Attitude assist on at the start of a run. */
    bool assist = true;
    /** No camera smoothing or transition animation. */
    bool reduced_motion = false;
    /** Multisample count: 1 or 4. Changing it rebuilds the pipelines. */
    int msaa = config::MSAA_SAMPLES;
    /** Draw the frame budget in the corner of the HUD. */
    bool show_stats = false;

    /** Reads the file; a missing or malformed file keeps the defaults. */
    void load();
    /** Writes the file, creating the directory. Returns false if it could not be written. */
    bool save() const;
};

/** The settings file's path, under the user's local app data. Empty if it cannot be resolved. */
const char *settings_path();

}  // namespace opra
