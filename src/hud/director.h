// The director panel (plan 4.6, F9): the program's cue drawn the way the pilot reads it - the gate
// lines with their ticks and crosses, the caret on the step in hand, the commanded throttle beside
// the held one, and the countdown to the burn.
//
// This is a copy of the sim's cue, not the cue itself: the HUD layer never sees a World, the same
// way DockFrame does not (hud.h says why). The translation happens once, in game/scene.cpp.
#pragma once

#include <string>
#include <vector>

#include "core/units.h"
#include "ui/draw.h"
#include "ui/ui.h"

namespace opra {

struct DirectorFrame {
    /** False when the context's program has no solution for this world: no panel, no blank box. */
    bool active = false;
    /** The header: the program and what it is working on. */
    std::string program;
    std::string target;
    /** The bearing the nose should take, radians, in the collar's own frame (the director cross). */
    Real heading = 0;
    Real throttle = 0;  // commanded, 0..1
    Real actual = 0;    // what the pilot is holding, for the pair
    Real burnIn = 0;    // seconds until the burn, or the time since it, for a node
    Real burnFor = 0;   // seconds of burn remaining
    Real dvRemaining = 0;
    /** True when the burn fields say something: a program with no burn draws no countdown. */
    bool burn = false;
    /** The active step, named. The caret sits on it. */
    const char* step = "";

    struct Gate {
        const char* name;
        Real value;
        Real limit;
        const char* unit;
        bool ok;
    };
    std::vector<Gate> gates;

    /** True when every gate holds: what the panel's caret colour reads. */
    bool all_ok() const;
};

/** The height the panel needs for the rows it will draw, so the block layouter can place it. */
float director_panel_height(const DirectorFrame& director, bool full_gates);

/** Draws the panel inside `at`. `full_gates` is the density's own gate: at level 2 the panel shows
 *  the failing terms, at 3 the whole list (F10). */
void build_director_panel(UIBatch& batch, const DirectorFrame& director, const ui::Rect& at,
                          bool full_gates);

}  // namespace opra
