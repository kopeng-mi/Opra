// Every key binding in one table. The flight input, the manual screen and the HUD all read this,
// so a binding can never drift from what the manual tells the pilot.
#pragma once

#include <SDL3/SDL.h>

#include "game/input.h"

namespace opra {

enum class Action {
    DriveForward,
    DriveReverse,
    TurnLeft,
    TurnRight,
    StrafeLeft,
    StrafeRight,
    KillVelocity,
    Boost,
    Assist,
    Interact,
    Cutter,
    CycleContact,
    /** Documentation rows for the pointer bindings, which the code reads directly. */
    SelectContact,
    /** The hand on the camera: Ctrl and the mouse, read directly like the pointer rows above. */
    LookAround,
    CameraScale,
    Cinematic,
    Chart,
    /** F10: the HUD's density, cycled 1 -> 2 -> 3 -> 1. */
    Density,
    /** F5: the minimap's mode, cycled contacts -> orbit -> corridor -> contacts. */
    Minimap,
    /** Deploys a satellite on the ship's own conic about the body it is orbiting (plan 3.7). */
    Deploy,
    Manual,
    ZoomIn,
    ZoomOut,
    ZoomReset,
    ModelViewer,
    Pause,
    /** Time warp, stepped: 1 / 10 / 100 / 1k / 10k / 100k. Above 10x the ship rides its conic. */
    WarpUp,
    WarpDown,
    /** The system map: the orrery and the ephemeris, with true scale on Y. */
    Map,
    TrueScale,
    /** The planner: insert the selected body's transfer burns, and advance to the first of them. */
    PlanNode,
    WarpToNode,
};

struct Binding {
    Action action;
    const char *keys;    // as printed in the manual
    const char *effect;  // what it does, in the manual's voice
    const char *note;
    SDL_Scancode primary;
    SDL_Scancode secondary;  // SDL_SCANCODE_UNKNOWN when there is only one
};

extern const Binding BINDINGS[];
extern const int BINDING_COUNT;

bool held(const Input &input, Action action);
bool pressed(const Input &input, Action action);

}  // namespace opra
