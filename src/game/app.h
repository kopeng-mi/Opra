// The application: the world, the input, the camera and the frame's UI. Knows about everything;
// nothing knows about it.
#pragma once

#include <string>
#include <vector>

#include <SDL3/SDL.h>

#include "game/camera_follow.h"
#include "game/input.h"
#include "game/settings.h"
#include "game/warp.h"
#include "hud/hud.h"
#include "render/backdrop.h"
#include "render/camera.h"
#include "render/gltf.h"
#include "render/orrery.h"
#include "render/renderer.h"
#include "game/scene.h"
#include "game/viewer.h"
#include "render/scene.h"
#include "render/text.h"
#include "sim/world.h"
#include "ui/draw.h"
#include "ui/flow.h"
#include "ui/menus.h"
#include "ui/title.h"
#include "ui/ui.h"
#include "ui/screens.h"

namespace opra {

struct App {
    World world;
    /** The loaded system, kept so a run reset re-attaches it: nothing about Nereid is hardcoded. */
    SystemDef system;
    int system_anchor = -1;
    ModelSet models;
    Renderer renderer;
    TextEngine text;
    Camera camera;
    Input input;
    SDL_Window *window = nullptr;
    /** Stars, nebula, dust and motes: real geometry at real depths, generated once. */
    Backdrop backdrop = build_backdrop();
    /** Reused every frame so instance and run vectors keep their capacity. */
    SceneBuilder scene;
    Settings settings;
    /** The immediate-mode context and this frame's pointer/nav state. */
    ui::Context ui;
    ui::Pointer pointer;
    ui::Nav nav;
    /** The screen state. Changed only through ui/flow.h's tables. */
    ui::Screen screen = ui::Screen::Startup;
    /** Where Back goes from a screen a transition pushed. FLOW says which edges push. */
    ui::Screen return_to = ui::Screen::Flight;
    Viewer viewer;
    bool wants_quit = false;
    /** Set when a setting changed: the file is written once, at the end of the frame. */
    bool settings_dirty = false;
    /** Smoothed frame time, for the debug panel. */
    float fps = 0.0f;
    /**
     * The startup plate (E12): the title screen is a state, not a separate program. The orrery
     * turns behind it and the rings draw in once over ~1.2 s, then settle.
     */
    int title_selected = 0;
    float title_reveal = 0.0f;
    /** The title screen's choice, consumed by the update pass on the frame after it is made. */
    ui::TitleAction title_action = ui::TitleAction::None;
    /** The system map's `Y`: draws every body at its real radius (plan P10). */
    bool true_scale = false;
    /** The body the map's transfer block describes, or -1 for none. */
    int map_target = -1;
    orrery::Meshes orrery_meshes;
    /** Rebuilt once a frame while the map or the plate is up, so the camera and the scene agree. */
    orrery::Frame map_frame;
    /** The time-warp rail: stepped, with the automatic drops that keep it honest. */
    Warp warp;
    /**
     * The sample count the settings screen asked for. The loop copies it into the renderer before
     * ensure_depth, because changing it mid-frame leaves a 4x colour target against a 1x depth
     * texture and the render pass is rejected.
     */
    int pending_samples = 1;

    float zoom = 1.35f;
    float zoom_current = 1.35f;
    /**
     * Orbit pitch in radians, from the settings and never from input (F1): fixed during flight.
     * Yaw is locked; roll is always 0. The viewer keeps its own turntable.
     */
    float pitch = CAMERA_PITCH_DEFAULT;
    /** The camera's damping state: where the view is centred and how fast that is moving. */
    FollowState follow;
    /**
     * The hand on the camera: an offset on the follow point from Ctrl and the mouse, in world
     * metres. It walks back to nothing when the key is released, so looking around never becomes a
     * new home (game/camera_follow.h says how it is stepped).
     */
    glm::dvec2 camera_pan{0.0};
    bool cinematic = false;
    /** F10: how much HUD is on. Cycled with the density key; the context forces blocks on top. */
    Density density = Density::One;
    bool mining = false;
    bool beam_active = false;
    /** World double metres: the beam is geometry in the same space as the ship, not a screen line. */
    glm::dvec2 beam_from{0.0};
    glm::dvec2 beam_to{0.0};
    bool guns_warned = false;
    /** The hull is gone: the HUD says so and R starts a fresh run. */
    bool wrecked = false;
    /** Scene only: the HUD is off, used to verify the 3D content on its own. */
    bool hud_hidden = false;
    double now = 0.0;
    std::vector<ui::Toast> toasts;
    /** Ticks of the last automatic asset poll; the poll runs at 1 Hz on the wall clock. */
    Uint64 model_poll_at = 0;
    /** --debug: enables the automatic asset poll and the extra frame logging. */
    bool debug = false;

    /** Window, GPU device, model library and sampler: everything the loop needs to exist. */
    void init(bool hidden, bool debug_gpu);

    /** Releases every GPU resource and shuts SDL down. */
    void shutdown();

    void toast(std::string message, double duration = 3.0);

    /**
     * Rebuilds the model library from disk when an asset changed. `force` skips the mtime check
     * (the F5 binding); the automatic poll runs once a second in --debug builds only, so a shipped
     * game never touches the disk mid-flight.
     */
    void reload_models_if_stale(bool force);

    /** Pushes the settings into the renderer and marks them for saving. */
    void apply_settings();

    /** Re-reads the ship's shape set from the loaded model. Call after any model load. */
    void sync_ship_collider();

    /** Re-reads the sector's docking ports (E9) from the station's and the hull's sidecars. */
    void sync_ports();

    /**
     * Inserts the two burns of a circular-to-circular transfer from the ship's orbit to the body
     * the map has selected, at the next departure window (plan 3.5). The planner's one action.
     */
    void plan_transfer();

    /** Starts a fresh run: new field, new ship, same settings and models. */
    void reset_run();
};

/**
 * The flight camera: perspective, tilted along the orbit, centred on the follow point (F1) rather
 * than on the ship, so the ship reads off centre by the lead and the deadzone.
 */
Camera camera_for(const World &world, Uint32 width, Uint32 height, float zoom, bool cinematic,
                  float pitch, const glm::dvec2 &follow);

/** The camera this frame should use: the viewer's while it is open, the flight camera otherwise. */
Camera active_camera(const App &app, Uint32 width, Uint32 height);

/** Keys, mouse, cutter and screens for one frame. Called after the simulation has stepped. */
void update_app(App &app, Uint32 width, Uint32 height, Real dt);

/** The flight HUD's inputs for this frame. */
HudFrame make_hud_frame(App &app, Uint32 width, Uint32 height);

/** The sector chart's inputs, copied out of the world (game/scene.cpp). */
ui::ChartFrame chart_frame_for(const World &world);

/** Builds the scene and the UI batch for this frame and hands both to the renderer. */
void render_app(App &app, SDL_GPUCommandBuffer *cmd, SDL_GPUTexture *color,
                SDL_GPUTextureFormat format, Uint32 width, Uint32 height);

/** Prints every model's part count and local bounds, plus one rock's resolved mesh. */
void dump_models(const ModelSet &models, const World &world);

}  // namespace opra
