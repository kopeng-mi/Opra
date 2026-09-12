// Patched-conic encounter prediction (PLAN-03 §3.6): walk the ship's conic forward, find the first
// crossing of a body's sphere of influence, swing through the body's frame, and hand back the
// coast that leaves it. Gravity assists need no new physics - patched conics already bend the
// path - only this prediction, which is what makes an assist aimable.
//
// The result is one coasting leg per stretch of the trajectory plus the post-encounter elements of
// each swing. The swing itself is not a drawn arc: a leg ends where the ship enters a sphere and
// the next begins where it leaves, so the map shows the bend across the disc and the conic that
// leaves it.
//
// The header names sim's SystemDef and BodyState only as incomplete types: the one translation
// unit that knows their layout is encounter.cpp.
#pragma once

#include <vector>

#include <glm/glm.hpp>

#include "orbit/kepler.h"

namespace opra {
struct SystemDef;
struct BodyState;
}  // namespace opra

namespace opra::orbit {

/** One stretch of the predicted trajectory, in the system frame, double metres. */
struct EncounterLeg {
    /** The body whose sphere this leg ends at, or -1 for the final coast. */
    int body = -1;
    /** True for a leg that starts after an encounter: it rides the post-encounter conic. */
    bool post = false;
    /** The coasting arc about the ship's primary, from this leg's start to its end. */
    std::vector<glm::dvec2> points;
    /** The conic the leg rides: the ship's own for the first leg, the post-encounter conic after. */
    Elements elements{};
};

/**
 * The first `depth` encounters on the ship's conic, and the coast that leaves the last one.
 *
 * `system` supplies the spheres and the bodies' own conics; `bodies` is the caller's snapshot at
 * `t`, which anchors the first leg's frame; `ship_conic` is the ship's conic about the body whose
 * mu it carries (the star at the root). The search samples at one hundredth of the conic's period
 * and bisects the sign change, so a crossing time is exact to the bisection, not to the sample.
 *
 * Returns nothing when no sphere is crossed - there is no encounter to predict. Otherwise the last
 * leg always has `body == -1` and carries the final post-encounter conic, unless the ship is
 * captured by the body it met (`e < 1`), where the chain simply stops at that body.
 */
std::vector<EncounterLeg> predict_encounters(const SystemDef &system,
                                             const std::vector<BodyState> &bodies,
                                             const Elements &ship_conic, double t, int depth);

}  // namespace opra::orbit
