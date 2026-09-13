// Close quarters: point-defence cannons, torpedoes and kinetic damage (plan 05 s5).
//
// Manoeuvring is the foundation; weapons are what happens while you do it. Everything here is
// pure sim: no renderer, no input, no clock but the step it is handed. The damage model is
// kinetic - E = 1/2 m v_rel^2 - and it lands on the compound-collider shape that was hit, so a
// hit on a drive pod degrades thrust and a hit on a tank vents propellant (s5.4).
#pragma once

#include <vector>

#include "core/units.h"
#include "sim/physics.h"

namespace opra {

// --------------------------------------------------------------------------- the lead solve

/**
 * Constant-velocity target, constant-speed round (s5.2). `r` is target minus muzzle, `v` the
 * target's velocity relative to the muzzle, `s` the round speed. Returns the smallest positive
 * intercept time, or -1 when there is none: a negative discriminant or two negative roots means
 * the target cannot be caught and the turret holds fire rather than firing uselessly.
 *
 * The degenerate cases the quadratic hides, all handled:
 *  - v.v = s^2 collapses the quadratic to a line, valid only when r.v < 0;
 *  - |v| ~ 0 is a direct aim at t = |r| / s;
 *  - a solution outside the turret's traverse or beyond s*t > range is the caller's "no shot".
 */
Real lead_solve(const Vec2 &r, const Vec2 &v, Real s);

// ----------------------------------------------------------------------------------- rounds

/** A kinetic round in flight. `shape` is the compound-collider index it last struck, -1 while airborne. */
struct Round {
    Vec2 position;
    Vec2 velocity;
    Real life = 0.0;      // seconds since launch
    Real damage = 0.0;    // joules this round carries
    int owner = -1;       // which vessel fired it: its own hull never takes it (s5.2)
    int hit_shape = -1;
};

inline constexpr Real ROUND_LIFETIME = 12.0;  // s5.5: lifetime AND range cap, so neither leaks

/** One step: motion, then the lifetime and range caps. Returns false when the round despawned. */
bool step_round(Round &round, Real dt);

// --------------------------------------------------------------------------------- torpedoes

/** A torpedo: proportional navigation while it has fuel and a target, ballistic after. */
struct Torpedo {
    Vec2 position;
    Vec2 velocity;
    Real angle = 0.0;     // radians, nose convention forward = (-sin, cos)
    int target = -1;      // the vessel it guides on, -1 ballistic
    Real fuel = 0.0;      // seconds of guidance left
    Real age = 0.0;       // since launch: arms at 0.5 s and 50 m (s5.3)
    Real travelled = 0.0;
    bool armed = false;
    Real warhead = 0.0;   // joules, on top of the kinetic term
    bool alive = true;
};

inline constexpr Real TORPEDO_NAV_GAIN = 4.0;    // N (s5.3)
inline constexpr Real TORPEDO_ARM_TIME = 0.5;
inline constexpr Real TORPEDO_ARM_RANGE = 50.0;
inline constexpr Real TORPEDO_MIN_GUIDE_RANGE = 5.0;  // below this, coast - detonation is imminent
/** Lateral authority, m/s^2: what makes a late hard turn a counter, not a script (s5.3). */
inline constexpr Real TORPEDO_LATERAL = 220.0;

/**
 * One guidance step. Proportional navigation about the target's position `target` and velocity
 * `target_velocity`: a_cmd = N * lambda_dot * V_c, clamped to the torpedo's lateral authority. A
 * torpedo with no target, no fuel, a target inside five metres, or a destroyed target coasts on
 * its last heading - still lethal, still tracked (s5.3, s5.5).
 */
void step_torpedo(Torpedo &torpedo, const Vec2 &target, const Vec2 &target_velocity,
                   bool target_alive, Real dt);

// ----------------------------------------------------------------------------------- damage

/**
 * Kinetic damage (s5.4): E = 1/2 m v_rel^2 scaled by the hull factor k. The raw conversion - the
 * per-shape application below is what makes the component model pay for itself.
 */
Real kinetic_damage(Real mass, Real relative_speed);

/**
 * s5.4's rule: damage lands on the compound collider shape that was hit, not on a single hull
 * pool. `shape` indexes the ship's collider. Hull integrity takes the kinetic term, and the
 * struck part degrades by what it is: a hit aft of the hull's mid-length feeds the drives, so
 * thrust authority drops; a hit forward feeds the tanks, so propellant vents and keeps venting -
 * a leak the integrator subtracts every step until the tank is dry.
 *
 * Returns the hull damage dealt. The ship's `thrustDamage` and `fuelLeak` fields carry the
 * component effects; step_ship reads both.
 */
Real apply_ship_damage(ShipState &ship, int shape, Real energy);

/** True when `shape` sits aft of the hull's mid-length: the drive-pod test s5.4 describes. */
bool shape_is_drive(const Shape &shape, const Collider &collider);

/** Where a PDC's round starts: the muzzle plus the hull radius along the barrel, never at the
 *  turret origin - a round spawned inside its own hull would hit it (s5.5). */
Vec2 muzzle_position(const Vec2 &mount, const Vec2 &direction, Real hull_radius);

/**
 * The spawn point the sim fires from: `muzzle_position` marched along the barrel until it is
 * outside every one of the firing ship's own compound shapes (s5.5's spawn rule, made robust to a
 * midship mount firing across its own hull).
 */
Vec2 clear_muzzle(const Collider &own, const Vec2 &ship_position, Real ship_angle, const Vec2 &mount,
                  const Vec2 &direction, Real hull_radius);

/**
 * s5.2's own-hull occlusion: a stern turret must not fire through its own ship. True when the aim
 * segment from `muzzle` toward `direction` out to `range` clears the firing ship's own compound
 * collider (its shapes in the ship frame at `ship_angle`), out to twice the hull radius.
 */
bool aim_clears_own_hull(const Collider &own, const Vec2 &ship_position, Real ship_angle,
                         const Vec2 &muzzle, const Vec2 &direction, Real range);

}  // namespace opra
