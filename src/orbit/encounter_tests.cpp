#include "orbit/encounter_tests.h"

#include <cmath>
#include <vector>

#include <glm/glm.hpp>

#include "orbit/encounter.h"
#include "orbit/kepler.h"
#include "selftest.h"
#include "sim/system.h"

// Every expected number below is derived on paper from PLAN-03 §3.6 and written into the comment
// beside it; the fly-by constants come from a straight replica of the patching in Python, not from
// this run. Run through `Opra.exe --selftest` once the case is registered there.
namespace opra::orbit {
namespace {

using selftest::check;
using selftest::check_close;

constexpr double kMuNereid = 2.07e19;
constexpr double kMuTarget = 1.9e13;
constexpr double kTargetA = 2.0e10;
constexpr double kTargetSoi = 2.0e8;
/** The target sits at the ship's own outbound crossing minus a lead of 0.0023 rad, which puts the
 *  closest approach at 4.37e7 m: a real fly-by, clear of the surface, well inside the sphere. */
constexpr double kTargetPhase = 0.5613401972069523;

/** The star and one planet whose sphere the ship's conic crosses. Built by hand: no asset, no SDL. */
SystemDef encounter_system() {
    SystemDef system;
    system.name = "encounter-test";

    Body star;
    star.id = "nereid";
    star.name = "Nereid";
    star.kind = BodyClass::Star;
    star.mu = kMuNereid;
    star.radius = 3.4e8;
    system.bodies.push_back(star);

    Body target;
    target.id = "target";
    target.name = "Target";
    target.parent = 0;
    target.kind = BodyClass::Planet;
    target.mu = kMuTarget;
    target.radius = 1.4e6;
    target.soi = kTargetSoi;
    target.elements.a = kTargetA;
    target.elements.e = 0.0;
    target.elements.omega = 0.0;
    target.elements.M0 = kTargetPhase;
    target.elements.t0 = 0.0;
    target.elements.mu = kMuNereid;
    system.bodies.push_back(target);
    return system;
}

/** a = 1.85e10 m, e = 0.2, periapsis on +x at t = 0: crosses the target's 2.0e10 m orbit. */
Elements crossing_ship() {
    Elements ship;
    ship.a = 1.85e10;
    ship.e = 0.2;
    ship.omega = 0.0;
    ship.M0 = 0.0;
    ship.t0 = 0.0;
    ship.mu = kMuNereid;
    return ship;
}

/** The first sphere entry by a scan a hundred times finer than the predictor's, then bisection. */
double brute_force_entry(const SystemDef &system, const Elements &conic) {
    const double span = period(conic);
    const int samples = 20000;
    std::vector<BodyState> states;
    const auto gap = [&](double t) {
        propagate(system, t, states);
        return glm::length(position_at(conic, t) - states[1].position) - system.bodies[1].soi;
    };
    double previous = gap(0.0);
    double previous_time = 0.0;
    for (int k = 1; k <= samples; ++k) {
        const double t = span * static_cast<double>(k) / static_cast<double>(samples);
        const double value = gap(t);
        if (previous > 0.0 && value <= 0.0) {
            double lo = previous_time;
            double hi = t;
            for (int i = 0; i < 80; ++i) {
                const double mid = 0.5 * (lo + hi);
                if (gap(mid) > 0.0) lo = mid;
                else hi = mid;
            }
            return hi;
        }
        previous = value;
        previous_time = t;
    }
    return -1.0;
}

void test_encounter_patching() {
    const SystemDef system = encounter_system();
    const Elements ship = crossing_ship();
    std::vector<BodyState> bodies;
    propagate(system, 0.0, bodies);

    const std::vector<EncounterLeg> legs = predict_encounters(system, bodies, ship, 0.0, 3);
    check(legs.size() >= 2, "encounter: a crossing yields a leg and a final coast");
    if (legs.size() < 2) return;
    check(legs.size() <= 4, "encounter: three encounters deep is the cap");
    check(legs.front().body == 1 && !legs.front().post,
          "encounter: the first leg ends at the target");
    check(legs.front().elements.a == ship.a && legs.front().elements.e == ship.e,
          "encounter: the first leg rides the ship's own conic");
    check(legs.back().body == -1 && legs.back().post,
          "encounter: the last leg is the final coast, marked post");

    // The crossing, against the fine scan: the leg ends at the entry, so its last point is the
    // entry position, and at that point the ship is on the sphere (hand value: 9.637 676e5 s).
    const double entry = brute_force_entry(system, ship);
    check(entry > 0.0, "encounter: the brute-force scan finds a crossing");
    const glm::dvec2 brute = position_at(ship, entry);
    check_close(glm::length(brute - legs.front().points.back()), 0.0, 1.0,
                "encounter: the leg's end is the brute-force entry to a metre");
    propagate(system, entry, bodies);
    check_close(glm::length(brute - bodies[1].position), kTargetSoi, 1.0e-3,
                "encounter: that point is exactly on the sphere");

    // The swing. Hand-computed: entry at 9.637 676e5 s, exit at 1.028 335e6 s (the hyperbolic
    // Kepler equation mirrored about periapsis), and the conic that leaves is a = 1.864 014e10 m,
    // e = 0.197 660 7 - the assist, with no physics beyond the frame change.
    check_close(legs[1].elements.t0, 1028335.4369565349, 1.0, "encounter: the exit time");
    check_close(legs[1].elements.a, 1.8640144061e10, 1.0e3, "encounter: the post-encounter a");
    check_close(legs[1].elements.e, 0.1976607178, 1e-6, "encounter: the post-encounter e");
    check(std::fabs(legs[1].elements.a - ship.a) > 1.0e8,
          "encounter: the encounter actually bent the orbit");
    check(legs[1].post, "encounter: the leg after the swing is marked post");
    propagate(system, legs[1].elements.t0, bodies);
    check_close(glm::length(legs[1].points.front() - bodies[1].position), kTargetSoi, 1.0e-3,
                "encounter: the post leg starts on the sphere, on the far side");
}

void test_no_crossing() {
    const SystemDef system = encounter_system();
    std::vector<BodyState> bodies;
    propagate(system, 0.0, bodies);

    Elements quiet;
    quiet.a = 1.0e10;
    quiet.e = 0.0;
    quiet.mu = kMuNereid;
    check(predict_encounters(system, bodies, quiet, 0.0, 3).empty(),
          "encounter: a conic that never nears a sphere predicts nothing");
    check(predict_encounters(system, bodies, crossing_ship(), 0.0, 0).empty(),
          "encounter: depth zero predicts nothing");
}

}  // namespace

int encounter_tests() {
    const int before = selftest::failures();
    test_encounter_patching();
    test_no_crossing();
    return selftest::failures() - before;
}

}  // namespace opra::orbit
