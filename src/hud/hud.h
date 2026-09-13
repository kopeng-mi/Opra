// Flight HUD: the collar and the four corner clusters, ported from the DRIFT design
// (AstraWars PLAN-HUD.md, collar.ts, style.css).
//
// The HUD paints marks, never surfaces: no panel, no fill, no radius. A mark is drawn
// only where data exists, so a nominal flight is nearly bare glass.
//
// The primitives are public so the app can draw its own screens (chart, manual) in the
// same language.
#pragma once

#include <string>
#include <vector>

#include <glm/glm.hpp>

#include "hud/director.h"
#include "hud/minimap.h"
#include "sim/physics.h"
#include "ui/draw.h"
#include "ui/tokens.h"

namespace opra {

enum class MarkKind { Velocity, Target, Hostile, Contact, Ore };

/** A bearing and range from the ship, with the strength that drives tick length and opacity. */
struct CollarMark {
    float bearing = 0.0f;  // radians, world frame
    float range = 0.0f;    // metres
    MarkKind kind = MarkKind::Contact;
    float strength = 1.0f;
};

/** Readout weight: dormant at reduced opacity, live at full, critical in threat coral. */
enum class Weight { Dormant, Live, Critical };

enum class StatusDot { Nominal, Paused, UnderFire };

/** F10: three densities on a key, with a context free to force a block back on. */
enum class Density { One = 1, Two = 2, Three = 3 };

/** The next level, wrapping back to One. */
Density next_density(Density density);
/** The name the log and the toast use for a level. */
const char *density_name(Density density);
/** True when `have` shows at least as much as `need`: the gate every optional block reads. */
bool density_at_least(Density have, Density need);

/**
 * The readouts the HUD's data blocks share, in the largest unit that keeps the figure short
 * (`412.6 km`, `1 h 48 m`, `2.41 km/s`). One implementation, so the orbit block, the director and
 * the minimap cannot print the same quantity three different ways.
 */
std::string distance_readout(double metres);
std::string clock_readout(double seconds);
std::string speed_readout(double metres_per_second);

/** Plan 4.6's orbit block: the conic's own figures, copied out of the world by game/scene.cpp. */
struct OrbitFrame {
    bool valid = false;
    const char* body = "";
    double altitude = 0.0;     // metres above the body's mean radius
    double periapsis = 0.0;    // likewise, and negative when the conic is inside the body
    double apoapsis = 0.0;
    double eccentricity = 0.0;
    double period = 0.0;       // seconds, zero on an escape
    double dvPlanned = 0.0;    // m/s still to be spent by the plan's node list
    double toPeriapsis = 0.0;  // seconds, the T- countdown the mock prints beside the figure
    double toApoapsis = 0.0;
    bool elliptic = false;
};

struct HudFrame {
    glm::vec2 screen{0.0f, 0.0f};
    glm::vec2 shipScreen{0.0f, 0.0f};
    /** Half the ship's drawn length on screen, in px: the centre readouts hang below it. */
    float shipRadiusPx = 0.0f;
    std::vector<CollarMark> marks;
    float thrust = 0.0f;  // signed, -0.28 .. 1.65
    float heat = 0.0f;    // 0..1
    float radius = 0.0f;  // 0 derives it from the viewport
    bool reducedMotion = false;
    float time = 0.0f;
    /** True while the collar should stay off the glass (chart, manual, cinematic). */
    bool hideCollar = false;

    // Top-left: the contract.
    const char *contractId = "";
    const char *objective = "";
    float progress = 0.0f;

    // Top-right: the session.
    float sessionSeconds = 0.0f;
    StatusDot status = StatusDot::Nominal;
    bool chartOpen = false;

    // Bottom-left: the vessel.
    const char *shipName = "";
    float hullFrac = 1.0f;
    float fuelFrac = 1.0f;
    float heatFrac = 0.0f;
    float hullValue = 100.0f;
    float fuelValue = 0.0f;
    float heatValue = 0.0f;
    bool assist = true;
    bool braking = false;

    // Bottom-right: the guns, ore held, and the two numerals that are always true.
    int gunCount = 0;
    int gunsReady = 0;
    bool gunsHot = false;
    double oreHeld = 0.0;
    double accelerationG = 0.0;
    double headingDeg = 0.0;
    double speed = 0.0;
    double zoom = 1.35;
    /** Travel direction in screen space (y down), unit length, and the nose direction. */
    glm::vec2 velocityDir{0.0f, -1.0f};
    glm::vec2 noseDir{0.0f, -1.0f};
    /** No HUD mark reads right with a dead hull: the banner replaces the centre readouts. */
    bool wrecked = false;

    // ---- plan 05 s4.3: the readout maths, computed in the game layer, printed by the HUD
    double verticalSpeed = 0.0;    // (r . v)/|r| - closing on the primary is negative
    double horizontalSpeed = 0.0;  // sqrt(|v|^2 - v_vert^2)
    double massTonnes = 0.0;       // wet mass
    double twr = 0.0;              // thrust / (mass . g); NaN prints as the plan's dash
    bool twrValid = false;
    double deltaV = 0.0;           // Isp . g0 . ln(m_wet/m_dry); 0 when the tanks are empty
    double burnSeconds = 0.0;      // dv . m / thrust, ship time
    double closingRate = 0.0;      // -(r_rel . v_rel)/|r_rel| to the tracked contact
    bool targetValid = false;
    const char *targetName = "";
    double targetRange = 0.0;
    /** The warp rail's suggestion (s2.7), shown on the time block, never applied silently. */
    double warpSuggest = 1.0;
    /** The first planned node, when one exists: the NODE block reads these. */
    bool nodeValid = false;
    double nodeDeltaV = 0.0;
    double nodeBurnSeconds = 0.0;
    double nodeTMinus = 0.0;
    /** Incoming fire (s4.5, s5): forces WEAPONS and the threat block on at any density. */
    bool underFire = false;
    /** The ship's own throttle, 0..1, for the bottom strip's bar. */
    float throttle = 0.0f;

    // Centre: the one contextual line.
    const char *context = nullptr;
    glm::vec4 contextColor{1.0f, 1.0f, 1.0f, 1.0f};

    /** F10: how much of the HUD is on. Level 1 is the flight minimum; each level adds blocks. */
    Density density = Density::One;
    /** Mining forces the guns block on: the ore count matters while the beam is biting. */
    bool forceGuns = false;
    /** An SOI change forces the orbit block on for ten seconds (plan 4.6). */
    bool forceOrbit = false;
    /** A gate out of tolerance forces the director's panel on: the failing term is the readout. */
    bool forceProgram = false;
    /** The orbit block's figures, the director's cue and the minimap's contents, all copied out of
     *  the world so the HUD layer never sees one. */
    OrbitFrame orbit;
    DirectorFrame director;
    MinimapFrame minimap;
    // Bottom-left, above the vessel: the world the ship is in (plan 3.2-3.7). Everything here is
    // zero or null in vacuum, and the block only appears when there is something to say.
    bool worldsValid = false;   // a body with air or ground is under the ship
    const char *worldsBody = "";
    double altitude = 0.0;      // metres above the ground
    double airDensity = 0.0;    // kg/m^3
    double airFlux = 0.0;       // W/m^2 at the stagnation point
    double descentRate = 0.0;   // m/s, closing on the ground
    bool landed = false;
    const char *baseName = nullptr;   // the pad's base, when one is servicing the ship
    double surveyFraction = 0.0;      // 0..1 of the primary's circumference mapped
    int satellitesUp = 0;
    int satellitesPending = 0;

    /** --debug: run the layout assertion pass over the frame's blocks (plan 5.2). */
    bool debug = false;
    /** The warp rail's rate, for the systems block at density 3. */
    double warpRate = 1.0;
};

/** True when the value should light up, per the design's thresholds. */
bool bar_value_visible(float fraction, bool inverted);

/**
 * The docking overlay's inputs, copied from the sim so the HUD never sees a World. The four gate
 * terms are the flight HUD's only live procedure: out-of-tolerance terms are the one thing in the
 * frame painted in `threat`, and everything in tolerance stays at dormant weight.
 */
struct DockFrame {
    bool active = false;
    float axial = 0.0f;      // metres down the corridor
    float lateral = 0.0f;    // metres off axis
    float closing = 0.0f;    // m/s toward the port
    float alignment = 0.0f;  // degrees off the corridor
    float rate = 0.0f;       // degrees per second of relative spin
    bool axial_ok = false;
    bool lateral_ok = false;
    bool closing_ok = false;
    bool alignment_ok = false;
    bool rate_ok = false;
    bool held = false;
    float hold = 0.0f;
    float hold_required = 0.4f;
    const char *port = "";
    float range = 0.0f;
    bool docked = false;
    float dockedSeconds = 0.0f;
};

/** Builds the whole flight HUD for one frame. */
void build_flight_hud(UIBatch &batch, const HudFrame &frame);

/** The orbit block's height, measured from the rows it will draw (plan 4.6, blocks.cpp). */
float orbit_block_height(const HudFrame &frame);
/** Draws the orbit block inside `at`, which the block layouter placed. */
void build_orbit_block(UIBatch &batch, const HudFrame &frame, const ui::Rect &at);

/** True when the panel draws every gate term rather than only the failing ones (F10, blocks.cpp). */
bool program_full_gates(const HudFrame &frame);
/** The director panel's block, measured and drawn from frame.director (plan 4.6, blocks.cpp). */
float program_block_height(const HudFrame &frame);
void build_program_block(UIBatch &batch, const HudFrame &frame, const ui::Rect &at);

/**
 * The corridor ladder: the docking axis seen end-on, with the gate's terms as live readouts
 * (plan §4.4). Drawn in place of the collar while a port is being approached.
 */
void build_dock_overlay(UIBatch &batch, const HudFrame &frame, const DockFrame &dock);

}  // namespace opra
