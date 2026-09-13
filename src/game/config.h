// Tunables. One place to change how the game feels, none of them buried in a function body.
#pragma once

namespace opra::config {

// Camera framing (plan 05 S-3, s3.1). FLIGHT_HALF is the half-height of the view at zoom 1, in
// metres; the default zoom lands on the 150 m home framing, which is 3.00 px per metre at 900 px
// tall - the kestrel's 46 m hull is 138 px long there, double its old 82, and the identity bands
// clear the two-pixel floor they used to average away. The wheel now runs anywhere from hull to
// system scale (render/camera.h carries the limits), so these numbers are only the home framing
// the `0` key and the startup state return to.
inline constexpr float FLIGHT_HALF = 200.0f;     // half-height of the flight view at zoom 1, metres
inline constexpr float CINEMATIC_HALF = 300.0f;  // pulled-back framing
// 200 / 150: zoom 1 holds the flight framing, and the default sits on the 150 m home half-height.
inline constexpr float ZOOM_DEFAULT = FLIGHT_HALF / 150.0f;
inline constexpr float ZOOM_MIN = 0.6f;
inline constexpr float ZOOM_MAX = 3.0f;
inline constexpr float ZOOM_STEP = 0.08f;
inline constexpr float ZOOM_SMOOTHING = 6.0f;

// The camera's follow (F1): the camera moves only a little, and the ship moves across the frame.
// The framing numbers: a 150 m half-height holds the ship at roughly a fifth of the way off centre
// when it is at speed, so the depth reads as parallax while the hull stays well inside the frame
// and clear of the corner blocks. A bigger lead parks the ship under the top-left block; a deadzone
// any larger welds it to the centre and the depth goes.
inline constexpr double LEAD_SECONDS = 0.15;
inline constexpr double LEAD_MAX = 18.0;
inline constexpr double FOLLOW_DEADZONE = 12.0;
inline constexpr double FOLLOW_TAU = 0.5;
/** Past this separation the follow snaps instead of springing: a reset, a warp jump, a handoff. */
inline constexpr double FOLLOW_SNAP = 640.0;

// The camera's freedom (plan 05 J1): Ctrl and the mouse swing the eye inside a cone about the home
// axis, and only inside it - thirty degrees in any direction, then the clamp holds. On release the
// angle walks back to home, so a look is never a new home; 0.15 s lands within a degree in 0.5 s,
// which is the return speed the camera gate asks for.
inline constexpr double LOOK_CONE_DEG = 30.0;
inline constexpr double LOOK_RELEASE_TAU = 0.15;          // seconds back to centre
inline constexpr double LOOK_RADIANS_PER_PIXEL = 0.0035;  // 30 degrees is ~150 px of drag

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
