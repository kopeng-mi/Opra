#include "game/bindings.h"

namespace opra {

/** Arrow keys duplicate the drive and turn bindings, as they always have. Mouse rows document the
 *  pointer bindings the code reads directly; their scancodes are UNKNOWN and never polled. */
const Binding BINDINGS[] = {
    {Action::DriveForward, "W", "main drive, reverse", "hold shift for a hard burn",
     SDL_SCANCODE_W, SDL_SCANCODE_UP},
    {Action::DriveReverse, "S", "main drive, reverse", "", SDL_SCANCODE_S, SDL_SCANCODE_DOWN},
    {Action::TurnLeft, "A", "rotate", "the nose is not the direction of travel", SDL_SCANCODE_A,
     SDL_SCANCODE_LEFT},
    {Action::TurnRight, "D", "rotate", "", SDL_SCANCODE_D, SDL_SCANCODE_RIGHT},
    {Action::StrafeLeft, "Q", "strafe", "the jets answer through the allocator", SDL_SCANCODE_Q,
     SDL_SCANCODE_UNKNOWN},
    {Action::StrafeRight, "E", "strafe", "", SDL_SCANCODE_E, SDL_SCANCODE_UNKNOWN},
    {Action::KillVelocity, "X", "kill velocity", "a deceleration ramp, not an instant stop",
     SDL_SCANCODE_X, SDL_SCANCODE_UNKNOWN},
    {Action::Boost, "Shift", "hard burn", "spends propellant faster than the drive alone",
     SDL_SCANCODE_LSHIFT, SDL_SCANCODE_RSHIFT},
    {Action::Assist, "F", "attitude assist", "trims rotation when you are not turning",
     SDL_SCANCODE_F, SDL_SCANCODE_UNKNOWN},
    {Action::Interact, "R", "scan, recover or dock", "cargo and the station answer to it",
     SDL_SCANCODE_R, SDL_SCANCODE_UNKNOWN},
    {Action::Cutter, "C / right mouse", "mining cutter", "bites rock where the beam lands",
     SDL_SCANCODE_C, SDL_SCANCODE_UNKNOWN},
    {Action::CycleContact, "Tab", "cycle contact", "cargo, station, relay, wreck",
     SDL_SCANCODE_TAB, SDL_SCANCODE_UNKNOWN},
    {Action::SelectContact, "Left mouse", "select contact", "guns are not fitted in this build",
     SDL_SCANCODE_UNKNOWN, SDL_SCANCODE_UNKNOWN},
    {Action::LookAround, "Ctrl + mouse", "swing the view",
     "the eye leans up to thirty degrees off home and eases back",
     SDL_SCANCODE_UNKNOWN, SDL_SCANCODE_UNKNOWN},
    {Action::CameraScale, "Mouse wheel", "one zoom, hull to system",
     "a notch is a quarter decade; shift makes it ten times that",
     SDL_SCANCODE_UNKNOWN, SDL_SCANCODE_UNKNOWN},
    {Action::Cinematic, "V", "cinematic view", "pulls back and drops the collar",
     SDL_SCANCODE_V, SDL_SCANCODE_UNKNOWN},
    {Action::Chart, "M", "sector chart", "the whole belt, its own scale", SDL_SCANCODE_M,
     SDL_SCANCODE_UNKNOWN},
    {Action::Map, "G", "system scale", "jumps to the whole system and back; the wheel goes further",
     SDL_SCANCODE_G, SDL_SCANCODE_UNKNOWN},
    {Action::Density, "K", "hud density", "flight, working, everything - cycles", SDL_SCANCODE_K,
     SDL_SCANCODE_UNKNOWN},
    {Action::Deploy, "L", "deploy satellite", "on the orbit you are already on, checked a period later", SDL_SCANCODE_L,
     SDL_SCANCODE_UNKNOWN},
    {Action::Minimap, "J", "minimap mode", "contacts, orbit, corridor - cycles", SDL_SCANCODE_J,
     SDL_SCANCODE_UNKNOWN},
    {Action::Manual, "H", "this manual", "esc closes it", SDL_SCANCODE_H, SDL_SCANCODE_UNKNOWN},
    {Action::ZoomIn, "+", "camera scale", "the wheel does the same",
     SDL_SCANCODE_EQUALS, SDL_SCANCODE_KP_PLUS},
    {Action::ZoomOut, "-", "camera scale", "", SDL_SCANCODE_MINUS, SDL_SCANCODE_KP_MINUS},
    {Action::ZoomReset, "0", "home framing", "the flight shot, back on your own hull",
     SDL_SCANCODE_0, SDL_SCANCODE_UNKNOWN},
    {Action::ModelViewer, "F2", "model viewer", "the export pipeline's own screen",
     SDL_SCANCODE_F2, SDL_SCANCODE_UNKNOWN},
    {Action::Pause, "Esc", "pause", "esc resumes", SDL_SCANCODE_ESCAPE, SDL_SCANCODE_UNKNOWN},
    {Action::Settings, "F1", "settings", "view, flight and renderer options",
     SDL_SCANCODE_F1, SDL_SCANCODE_UNKNOWN},
    {Action::WarpUp, ">", "warp up", "above 10x the ship rides its conic: no thrust",
     SDL_SCANCODE_PERIOD, SDL_SCANCODE_UNKNOWN},
    {Action::WarpDown, "<", "warp down", "a contact, a burn or a new sphere drops it to 1x",
     SDL_SCANCODE_COMMA, SDL_SCANCODE_UNKNOWN},
    {Action::PlanNode, "N", "plan transfer", "inserts the two burns for the selected body",
     SDL_SCANCODE_N, SDL_SCANCODE_UNKNOWN},
    {Action::WarpToNode, "B", "warp to node", "advances to the next planned burn, exactly",
     SDL_SCANCODE_B, SDL_SCANCODE_UNKNOWN},
    {Action::WeaponsFree, "Y", "weapons free / hold",
     "the point-defence mounts engage the tracked contact inside 700 m",
     SDL_SCANCODE_Y, SDL_SCANCODE_UNKNOWN},
    {Action::FireTorpedo, "T", "torpedo away", "proportional navigation on the tracked contact",
     SDL_SCANCODE_T, SDL_SCANCODE_UNKNOWN},
};

const int BINDING_COUNT = static_cast<int>(sizeof(BINDINGS) / sizeof(BINDINGS[0]));

bool held(const Input &input, Action action) {
    for (const Binding &binding : BINDINGS) {
        if (binding.action != action) continue;
        if (binding.primary != SDL_SCANCODE_UNKNOWN && input.held(binding.primary)) return true;
        if (binding.secondary != SDL_SCANCODE_UNKNOWN && input.held(binding.secondary)) return true;
    }
    return false;
}

bool pressed(const Input &input, Action action) {
    for (const Binding &binding : BINDINGS) {
        if (binding.action != action) continue;
        if (binding.primary != SDL_SCANCODE_UNKNOWN && input.pressed(binding.primary)) return true;
        if (binding.secondary != SDL_SCANCODE_UNKNOWN && input.pressed(binding.secondary)) {
            return true;
        }
    }
    return false;
}

}  // namespace opra
