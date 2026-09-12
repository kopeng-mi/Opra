// The flight director: nine programs, each one a read-only solution over the world, drawn as a cue.
// F9 says the pilot always flies, and the way to keep that true is to make the alternative not
// compile: every entry point takes `const World&`, so a program that wanted to write to the ship
// would have no state to write to. A director that mutates is an autopilot, and there is none.
#pragma once

#include <string>
#include <vector>

#include "core/units.h"
#include "sim/world.h"

namespace opra {

enum class Program {
    None,
    Circularize,
    MatchOrbit,
    Approach,
    Dock,
    Deorbit,
    Land,
    KillRelative,
    Transfer,
};

/** One term of a gate: what it is, what it reads, what it may read, and whether it holds. */
struct GateTerm {
    const char* name;
    Real value;
    Real limit;
    const char* unit;
    bool ok;
};

/**
 * What the pilot flies by. The director draws it and writes nothing.
 *
 * `heading` is a world bearing in the collar's own frame - `atan2` of the direction the nose should
 * take - so the cross lands where a mark at the same bearing lands. `throttle` is the setting the
 * described burn wants, drawn against the one the pilot is actually holding; where the burn is
 * still ahead, `burn_in` is the countdown to it and is negative only in a program whose burn the
 * world itself performs, namely a planned node once its time has passed.
 */
struct Cue {
    bool active = false;
    Real heading = 0;
    Real throttle = 0;
    Real burn_in = 0;
    Real burn_for = 0;
    Real dv_remaining = 0;
    const char* step = "";
    std::vector<GateTerm> gates;
};

/** The name the panel's header prints: the program's own, in caps. */
const char* program_name(Program program);

/** What the program is working on, for the panel's header: "wayfarer.A", "Tessera Downport". */
std::string program_target(Program program, const World& world);

/**
 * The solution for `program` at time `t`, read from the world and written nowhere. Every row is
 * derived from real geometry: the ship's conic about the body that holds it, the deployed gate, the
 * node list, and the body data the system file carries. A row whose geometry the world does not
 * have returns an inactive cue rather than a fabricated one.
 */
Cue evaluate(Program program, const World& world, Real t);

}  // namespace opra
