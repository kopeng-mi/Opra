// The screens that are not the flight HUD: the sector chart, the manual, and the toast line.
// Everything here takes plain frames, never a World: the UI layer does not know the sim.
#pragma once

#include <string>
#include <vector>

#include <glm/glm.hpp>

#include "render/camera.h"
#include "render/scene.h"
#include "sim/component.h"
#include "ui/draw.h"
#include "ui/ui.h"

namespace opra::orrery { struct Frame; }

namespace opra {
struct Input;
}

namespace opra::ui {

enum class ToastKind { Info, Warning };

/** One line of plain type under the contract it is about. */
struct Toast {
    std::string text;
    double until = 0.0;
    ToastKind kind = ToastKind::Info;
};

/** The chart's inputs, on the chart's own scale. */
struct ChartFrame {
    struct Dot {
        glm::vec2 at;
        float radius;
    };
    struct Mark {
        enum class Kind { Cargo, Station, Relay, Derelict };
        std::string text;
        glm::vec2 at;
        Kind kind = Kind::Cargo;
    };
    std::vector<Dot> rocks;
    std::vector<Dot> fragments;
    std::vector<Dot> ore;
    std::vector<Mark> marks;
    glm::vec2 ship{0.0f};
    glm::vec2 velocity{0.0f};
    float heading = 0.0f;
    glm::vec2 bounds_min{0.0f};  // sector extent, metres
    glm::vec2 bounds_max{0.0f};
};

/** The contract screen (plan 06 §4.2): interactive chart of the zone with payout/deadline/licence. */
struct ContractResult {
    bool accept = false;
    bool back = false;
};

struct ContractState {
    glm::vec2 pan{0.0f};
    float zoom = 1.0f;
    bool hazards = false;
    int hovered = -1;
    bool dragging = false;
    glm::vec2 drag_start{0.0f};
};

ContractResult build_contract(Context &ui, const Rect &screen, ContractState &state,
                             const ChartFrame &chart);

/** The shipyard screen (plan 06 §4.3): 3D turntable, parts palette, and DERIVED stats. */
struct ShipyardPart {
    std::string name;
    glm::vec3 pos{0.0f};
    bool mirrored = false;
};

struct ShipyardState {
    ShipDesign design;
    int category = 0;
    int held = -1;
    int hovered_slot = -1;
    Facing hovered_facing = Facing::Fore;
    int selected = -1;
    bool mirror = true;
    bool show_flanges = true;

    // Catalogue 3D hover preview and mount ghost (PLAN-09 U6, U7)
    std::string preview_part;
    float preview_yaw = 0.0f;
    std::optional<Placement> ghost_placement;
    std::optional<Placement> ghost_mirror;

    // Turntable
    float yaw = 0.6f;
    float pitch = 0.35f;
    float distance = 1.0f;
    float yaw_target = 0.6f;
    float pitch_target = 0.35f;
    float distance_target = 1.0f;
    bool dragging = false;
    glm::vec2 pointer{0.0f};

    PartTable parts;
    ShipSpec cached_spec{};
    bool spec_dirty = true;

    // Compatibility field
    bool has_drive = true;

    bool design_has_drive() const {
        if (!design.placements.empty()) return opra::design_has_drive(design, parts);
        return has_drive;
    }
};

struct ShipyardResult {
    bool launch = false;
    bool back = false;
};

ShipyardResult build_shipyard(Context &ui, const Rect &screen, ShipyardState &state);
void build_shipyard_scene(SceneBuilder &scene, const ModelSet &models, const ShipyardState &state);
Camera shipyard_camera(const ModelSet &models, const ShipyardState &state, uint32_t width, uint32_t height);

/** The sector chart state (plan 06 §4.5): pan, zoom, and selected marker. */
struct ChartState {
    glm::vec2 pan{0.0f};
    float zoom = 1.0f;
    int selected_mark = -1;
    bool dragging = false;
    glm::vec2 drag_start{0.0f};
};

/** The whole belt, one flat view, on its own scale (plan 06 §4.5). */
void build_chart(UIBatch &batch, const ChartFrame &frame, float width, float height,
                ChartState *state = nullptr);

/** Ephemeris 2D chart ink drawn behind the title block (PLAN-08 §10.3). */
void build_ephemeris(UIBatch &batch, const struct orrery::Frame &frame, const Rect &at, float alpha);

/** The flight manual state (plan 06 §4.6): pagination and typing filter. */
struct ManualState {
    std::string filter;
    int page = 0;
};

/** The flight manual: paginated columns, live key highlighting, and typing filter (plan 06 §4.6). */
void build_help(UIBatch &batch, float width, float height, const Input *input = nullptr,
                ManualState *state = nullptr);

/** Draws the live toasts, oldest first, under the contract line. */
void draw_toasts(UIBatch &batch, const std::vector<Toast> &toasts, double now, float x, float y);

}  // namespace opra::ui
