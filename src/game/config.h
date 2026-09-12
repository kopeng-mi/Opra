// Tunables. One place to change how the game feels, none of them buried in a function body.
#pragma once

namespace opra::config {

// Camera.
inline constexpr float FLIGHT_HALF = 340.0f;     // half-height of the flight view at zoom 1, metres
inline constexpr float CINEMATIC_HALF = 470.0f;  // pulled-back framing
// The orbit pitch and the field of view live in render/camera.h: they are camera geometry, not
// game feel, and the render layer may not include this file.
inline constexpr float ZOOM_DEFAULT = 1.35f;
inline constexpr float ZOOM_MIN = 0.6f;
inline constexpr float ZOOM_MAX = 3.0f;
inline constexpr float ZOOM_STEP = 0.08f;
inline constexpr float ZOOM_SMOOTHING = 6.0f;

// The camera's follow (F1): the camera moves only a little, and the ship moves across the frame.
// The numbers are framing decisions, not physics: a 187 m half-height at the default zoom holds the
// ship at roughly a fifth of the way off centre when it is at speed, so the depth reads as parallax
// while the hull stays well inside the frame and clear of the corner blocks. A bigger lead parks the
// ship under the top-left block; a deadzone any larger welds it to the centre and the depth goes.
inline constexpr double LEAD_SECONDS = 0.15;
inline constexpr double LEAD_MAX = 18.0;
inline constexpr double FOLLOW_DEADZONE = 12.0;
inline constexpr double FOLLOW_TAU = 0.5;
/** Past this separation the follow snaps instead of springing: a reset, a warp jump, a handoff. */
inline constexpr double FOLLOW_SNAP = 640.0;

// The hand on the camera: Ctrl and the mouse. The pitch range is the settings screen's own, so a
// value dragged here is the value that comes back from the file. The pan is a look, not a move: it
// reaches half a frame and walks back the moment the key is let go.
inline constexpr float LOOK_PITCH_MIN_DEG = 17.0f;
inline constexpr float LOOK_PITCH_MAX_DEG = 88.0f;
inline constexpr double LOOK_PAN_FRACTION = 0.5;   // of the frame's half-height
inline constexpr double LOOK_RELEASE_TAU = 0.25;   // seconds back to centre

// Rendering. 4x MSAA: low-poly silhouettes on a near-black field alias badly. 1 disables it.
inline constexpr int MSAA_SAMPLES = 4;

// Models are authored in metres and scaled to the sim's hull.
inline constexpr float SHIP_SCALE = 1.3f;

// Mining cutter, from AstraWars combat.ts: damage 46 x rockBonus 3.2, range 210 m, arc +-0.50 rad.
inline constexpr double CUTTER_RANGE = 210.0;
inline constexpr double CUTTER_ARC = 0.50;
inline constexpr double CUTTER_DAMAGE = 46.0 * 3.2;
inline constexpr double CUTTER_DRAW = 9.0;
inline constexpr double CUTTER_HEAT = 0.340;

}  // namespace opra::config
