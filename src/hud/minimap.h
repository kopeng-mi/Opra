// F5: one corner, one footprint, three modes. The contact radar while there is something to
// avoid, the orbital strip while coasting or transferring, and the corridor plan view on approach.
// North is world north in every mode, the same frame the collar draws its marks in: a rotating
// minimap beside a fixed collar is two instruments disagreeing.
//
// The mode is context-driven (plan 4.6) with a key to cycle it. The pilot's choice and the context
// live here rather than in App, which carries the world and is not this phase's to grow; the
// choice is HUD memory, and `minimap_choice` is the one place that owns it.
#pragma once

#include <vector>

#include <glm/glm.hpp>

#include "core/units.h"
#include "ui/draw.h"
#include "ui/ui.h"

namespace opra {

enum class MinimapMode { Contact, Orbit, Corridor };

/** The next mode, wrapping: the key's own cycle. */
MinimapMode next_minimap_mode(MinimapMode mode);
/** The name the toast and the mode's own header print. */
const char* minimap_mode_name(MinimapMode mode);

/** The radar's outer ring, metres: the collar's own scale, so the two instruments agree. */
inline constexpr float MINIMAP_RANGE = 2000.0f;

/** The mode the context asks for: the corridor on approach, the radar with a contact inside it,
 *  the orbital strip otherwise (plan 4.6). */
MinimapMode minimap_context_mode(bool berth_in_range, bool contact_in_range);

/** The pilot's choice. `manual` stays false until the key is first pressed, so the context owns the
 *  mode until the pilot takes it over. */
struct MinimapChoice {
    MinimapMode mode = MinimapMode::Contact;
    bool manual = false;
};

MinimapChoice& minimap_choice();

/**
 * The mode to draw, and the one place the precedence is written down: the pilot's choice once the
 * key has taken over, the context until then, and the corridor whenever a berth is in range - the
 * one moment the plan says the context outranks everything (F10's context override).
 */
MinimapMode resolve_minimap_mode(bool berth_in_range, bool contact_in_range);

/** Everything the minimap draws, copied out of the world by game/scene.cpp. */
struct MinimapFrame {
    MinimapMode mode = MinimapMode::Contact;
    /** Contact radar: the collar's marks, by bearing and range in the collar's own frame. */
    struct Blip {
        float bearing = 0.0f;
        float range = 0.0f;
        bool tracked = false;
    };
    std::vector<Blip> blips;
    float range = MINIMAP_RANGE;
    /** Orbital strip: the conic's own figures, above the body's mean radius. */
    const char* primary = "";
    double altitude = 0.0;
    double periapsis = 0.0;
    double apoapsis = 0.0;
    bool elliptic = false;
    /** Corridor plan view: the ship's offset from the berth, in the sector frame's metres. */
    glm::vec2 berthOffset{0.0f};
    float berthBearing = 0.0f;  // the berth's outward normal, a world bearing
    float axial = 0.0f;
    float lateral = 0.0f;
    float closing = 0.0f;
    float lateralLimit = 8.0f;
    bool active = false;
};

/** The height the minimap block needs: one footprint, the same in every mode. */
float minimap_block_height(const MinimapFrame& minimap);

/** Draws the minimap inside `at`, which the block layouter placed. */
void build_minimap(UIBatch& batch, const MinimapFrame& minimap, const ui::Rect& at);

}  // namespace opra
