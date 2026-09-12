// Assert suite for the warp rail: the steps, the railed boundary, and every automatic drop.
#include "game/warp_tests.h"

#include <cmath>

#include "game/warp.h"
#include "selftest.h"

namespace opra {
namespace {

using selftest::check;
using selftest::check_close;

void test_rail_steps() {
    Warp warp;
    check_close(warp.rate(), 1.0, 0.0, "warp: it starts at real time");
    check(!warp.railed(), "warp: real time is integrated, not railed");

    for (int i = 0; i < WARP_RATE_COUNT - 1; ++i) warp.request(1);
    check_close(warp.rate(), 1.0e5, 0.0, "warp: the top of the rail is 100 000x");
    warp.request(1);
    check_close(warp.rate(), 1.0e5, 0.0, "warp: the rail cannot go above its top step");
    check(warp.railed(), "warp: 100 000x rides the conic");

    // The boundary is above 10x, not at it: 10x is the fastest rate the integrator still owns.
    Warp at_ten;
    at_ten.request(1);
    check_close(at_ten.rate(), 10.0, 0.0, "warp: the second step is 10x");
    check(!at_ten.railed(), "warp: 10x is still integrated");
    at_ten.request(1);
    check(at_ten.railed(), "warp: 100x rides the conic");

    for (int i = 0; i < WARP_RATE_COUNT; ++i) warp.request(-1);
    check_close(warp.rate(), 1.0, 0.0, "warp: the rail cannot go below real time");
}

void test_auto_drops() {
    const auto at_top = []() {
        Warp warp;
        for (int i = 0; i < WARP_RATE_COUNT - 1; ++i) warp.request(1);
        return warp;
    };

    Warp thrust = at_top();
    check(thrust.update(true, false, false, false), "warp: thrusting drops the warp");
    check_close(thrust.rate(), 1.0, 0.0, "warp: the thrust drop lands at real time");
    check(thrust.drop == Warp::Drop::Thrust, "warp: the drop names thrust as its reason");

    Warp contact = at_top();
    check(contact.update(false, true, false, false), "warp: a contact drops the warp");
    check(contact.drop == Warp::Drop::Contact, "warp: the drop names the contact");

    Warp sphere = at_top();
    check(sphere.update(false, false, true, false), "warp: switching sphere of influence drops the warp");
    check(sphere.drop == Warp::Drop::Sphere, "warp: the drop names the sphere switch");

    // Air: plan 3.3. A ship inside an atmosphere is not on a conic, so the rail cannot advance it
    // exactly and warp comes down to real time like any other reason.
    Warp air = at_top();
    check(air.update(false, false, false, true), "warp: air drops the warp");
    check(air.drop == Warp::Drop::Air, "warp: the drop names the atmosphere");

    // No reason, no drop: an ordinary frame at warp stays where it is.
    Warp steady = at_top();
    check(!steady.update(false, false, false, false), "warp: a quiet frame does not drop the warp");
    check_close(steady.rate(), 1.0e5, 0.0, "warp: and the rate is unchanged");

    // A drop already at real time reports nothing: there is no transition to announce.
    Warp already;
    check(!already.update(true, false, false, false), "warp: a drop at real time is not a transition");
    check(already.rate() == 1.0, "warp: and it stays at real time");

    // The manual drop is for the planner: editing a node puts the ship back under the pilot.
    Warp manual = at_top();
    check(manual.drop_to_real_time(Warp::Drop::Manual), "warp: a manual drop leaves the rail");
    check_close(manual.rate(), 1.0, 0.0, "warp: the manual drop lands at real time");
    check(manual.drop == Warp::Drop::Manual, "warp: the manual drop names itself");
    check(!manual.drop_to_real_time(Warp::Drop::Manual), "warp: dropping twice is not a transition");
}

void test_rate_ordering() {
    // Guard the invariant the rail's index arithmetic depends on, so a new rate cannot be inserted
    // out of order or below 1x without failing here.
    for (int i = 1; i < WARP_RATE_COUNT; ++i) {
        check(WARP_RATES[i] > WARP_RATES[i - 1], "warp: the rates increase along the rail");
    }
    check(WARP_RATES[0] == 1.0, "warp: the rail starts at real time");
    check(WARP_RAIL_ABOVE == WARP_RATES[1], "warp: the railed boundary is the second step");
}

}  // namespace

int warp_tests() {
    const int before = selftest::failures();
    test_rail_steps();
    test_auto_drops();
    test_rate_ordering();
    return selftest::failures() - before;
}

}  // namespace opra
