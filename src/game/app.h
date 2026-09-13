// The application: the world, the input, the camera and the frame's UI. Knows about everything;
// nothing knows about it.
#pragma once

#include <string>
#include <unordered_map>

#include <optional>
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
#include "sim/designs.h"
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
    DesignStore designs;
    PartTable part_table;
    void build_part_table();
    /** The LOD hysteresis (plan 05 s2.5): per-object level, kept across frames. */
    std::unordered_map<unsigned long long, int> lod_memory;
    Settings settings;
    /** The immediate-mode context and this frame's pointer/nav state. */
    ui::Context ui;
    ui::Pointer pointer;
    ui::Nav nav;
    /** The screen stack (plan 06 §2.1). Startup at bottom; current is stack.back(). */
    std::vector<ui::Screen> stack = {ui::Screen::Startup};
    ui::Screen current() const { return stack.empty() ? ui::Screen::Startup : stack.back(); }
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
    ui::ContractState contract_state;
    ui::ShipyardState shipyard_state;
    ui::ManualState manual_state;
    ui::ChartState chart_state;
    float title_zoom = 1.0f;
    float title_yaw = 0.0f;
    float title_pitch = orrery::MAP_PITCH;
    float title_fly_t = 0.0f;
    bool title_dragging = false;
    /** The title screen's choice, consumed by the update pass on the frame after it is made. */
    ui::TitleAction title_action = ui::TitleAction::None;
    /** The body the planner's transfer aims at, or -1 for none (the map screen's survivor). */
    int map_target = -1;
    orrery::Meshes orrery_meshes;
    /** Rebuilt once a frame while the plate is up, so the camera and the scene agree. */
    orrery::Frame map_frame;
    /** The time-warp rail: stepped, with the automatic drops that keep it honest. */
    Warp warp;
    /**
     * The sample count the settings screen asked for. The loop copies it into the renderer before
     * ensure_depth, because changing it mid-frame leaves a 4x colour target against a 1x depth
     * texture and the render pass is rejected.
     */
    int pending_samples = 1;

    /**
     * The one continuous zoom (plan 05 J2): the target half-height the wheel edits, in metres, and
     * the smoothed one the camera actually draws with. Smoothing is multiplicative - a zoom that
     * feels the same at 30 m must also feel the same at 3e9 m - so the easing runs in log space.
     */
    double half_height = HOME_HALF;
    double half_height_current = HOME_HALF;
    /** The last framing the M toggle left, so the key is a there-and-back (s2.1's home returns). */
    double system_recall_half = 0.0;
    /** The last left-click, for the double-click that frames a body (s2.1). */
    double last_click_at = -10.0;
    glm::vec2 last_click_at_px{0.0f};
    /**
     * Orbit pitch in radians, from the settings and never from input (F1): fixed during flight.
     * Yaw is locked; roll is always 0. The viewer keeps its own turntable.
     */
    float pitch = CAMERA_PITCH_DEFAULT;
    /** The camera's damping state: where the view is centred and how fast that is moving. */
    FollowState follow;
    /** The body the follow tracks, or -1 for the ship. A double-click on a body sets it. */
    int follow_body = -1;
    /**
     * The camera's freedom (plan 05 J1): a Ctrl-drag swings the eye inside a cone about the home
     * axis - x is azimuth, y elevation, radians - clamped to thirty degrees, and released it walks
     * back home so a look is never a new home.
     */
    glm::dvec2 camera_look{0.0};
    /** The cursor position while Ctrl is held: the anchor the cone angle is dragged from. */
    std::optional<glm::vec2> look_anchor;
    bool cinematic = false;
    /** F10: how much HUD is on. Density 2 is the baseline (plan 05 J7); the context forces blocks. */
    Density density = Density::Two;
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
 * than on the ship, so the ship reads off centre by the lead and the deadzone. `look` swings the
 * eye inside the cone about the home axis (plan 05 J1).
 */
Camera camera_for(const World &world, Uint32 width, Uint32 height, double half_height,
                  bool cinematic, float pitch, const glm::dvec2 &follow,
                  const glm::dvec2 &look = glm::dvec2(0.0));

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
