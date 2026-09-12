// Assert suite for the world's own behaviour: the berthing path (approach, capture, hard dock,
// undock) as the simulation actually runs it, not as the docking math evaluates in isolation.
#include "sim/world_tests.h"

#include <cmath>

#include "core/file.h"
#include "orbit/transfer.h"
#include "selftest.h"
#include "sim/world.h"

namespace opra {
namespace {

using selftest::check;
using selftest::check_close;

constexpr Real kStep = 1.0 / 120.0;

/** The ship's own port, in the hull's frame: 44 m aft of the centre, as the Kestrel's is. */
inline constexpr Real SHIP_PORT_OFFSET = 44.0;
/** The station's port sits on its ring's face, 81 m out along +y. */
inline constexpr Real STATION_PORT_OFFSET = 81.0;

/** A world whose sector berths at one port on the station's own hull face, class M. */
World berthing_world() {
    // Port A sits at the ring's face, 81 m out along +y in the exported model, so a corridor along
    // its normal never crosses the station's own collision shapes - which is what makes it a port.
    World world;
    world.set_ports({Port{"A", {0.0, STATION_PORT_OFFSET}, {0.0, 1.0}, 'M'}},
                    Port{"aft", {0.0, -SHIP_PORT_OFFSET}, {0.0, -1.0}, 'M'});
    return world;
}

/**
 * Places the ship in the corridor: its *port* `axial` metres out along the port's normal, closing
 * on it. The hull's own length matters here - a long hull hovering too close clips the station's
 * ring, which is a real collision and not a docking failure.
 */
void place_in_corridor(World &world, Real axial, Real closing, Real lateral = 0.0) {
    world.ship.position = {STATION.x + lateral,
                           STATION.y + STATION_PORT_OFFSET + axial + SHIP_PORT_OFFSET};
    world.ship.velocity = {0.0, -closing};
    // Nose +y: the hull's own port then faces -y, straight down the corridor at the station's port.
    world.ship.angle = 0.0;
    world.ship.angularVelocity = 0.0;
}

void test_capture_path() {
    World world = berthing_world();
    place_in_corridor(world, 30.0, 0.8, 1.5);

    // Half a second of a clean approach is not a capture: the hold is 0.4 s of the terms holding,
    // and the capture that follows takes two seconds to reach hard dock.
    for (int i = 0; i < 60; ++i) world.step(FlightInput{}, kStep);
    check(world.gate.all_ok(), "world: a clean approach passes every gate term");
    check(world.capture_started >= 0.0 || world.docked, "world: the gate closed the capture");

    for (int i = 0; i < 400; ++i) world.step(FlightInput{}, kStep);
    check(world.docked, "world: the capture ends in hard dock");
    check(world.docked_for >= 0.0, "world: and the clock shows it");
    // The station turns while the capture runs, so the berth is wherever the port is *now*: the
    // ship rides it, which is the point of a port that is a hardpoint rather than a fixed point.
    check_close(world.ship.position.x, world.target_state.position.x, 0.5,
                "world: hard dock sits on the port (x)");
    check_close(world.ship.position.y, world.target_state.position.y, 0.5,
                "world: hard dock sits on the port (y)");
    const Real speed = std::hypot(world.ship.velocity.x, world.ship.velocity.y);
    check(speed < 4.0, "world: a docked ship moves with the station, not with its old velocity");

    // The pilot has no controls while the ship is hard docked: a burn changes nothing.
    const Vec2 before = world.ship.position;
    const FlightInput burn{1.0, 0.0, 0.0, false, true};
    for (int i = 0; i < 30; ++i) world.step(burn, kStep);
    check_close(world.ship.position.x, before.x, 3.0, "world: thrust does nothing while docked");
    check(world.docked, "world: and the ship is still docked");
}

void test_undock() {
    World world = berthing_world();
    place_in_corridor(world, 30.0, 0.8);
    for (int i = 0; i < 500; ++i) world.step(FlightInput{}, kStep);
    check(world.docked, "world: docked before the undock test");

    // The impulse is the difference the release makes: the ship keeps the station's rotation it
    // was riding, and gains 0.5 m/s along the port's outward normal on top of it.
    const Vec2 before = world.ship.velocity;
    world.undock();
    const Vec2 normal = world.target_state.normal;
    check(!world.docked, "world: undocking releases the ship");
    check(world.docked_for < 0.0, "world: and clears the berth clock");
    check_close(world.ship.velocity.x - before.x, UNDOCK_SPEED * normal.x, 1e-9,
                "world: the undock push is 0.5 m/s out along the port's normal (x)");
    check_close(world.ship.velocity.y - before.y, UNDOCK_SPEED * normal.y, 1e-9,
                "world: the undock push is 0.5 m/s out along the port's normal (y)");
}

void test_fast_approach_is_refused() {
    World world = berthing_world();
    place_in_corridor(world, 40.0, 8.0);  // far too fast: the gate's closing limit is 1.2 m/s
    for (int i = 0; i < 240; ++i) world.step(FlightInput{}, kStep);
    check(!world.docked, "world: an eight metre per second approach is never captured");
    check(!world.gate.closing_ok, "world: the closing term is the one that says so");
}

void test_no_ports_no_docking() {
    // A world with no exported ports simply never berths; nothing divides by an empty list.
    World world;
    place_in_corridor(world, 30.0, 0.5);
    for (int i = 0; i < 600; ++i) world.step(FlightInput{}, kStep);
    check(!world.docked, "world: without ports there is nothing to dock to");
    check(world.target_port < 0, "world: and no port is being approached");
}

/**
 * The planner: a node in the book must change the predicted conic, the warp to it must land on the
 * node rather than near it, and the burn must leave the ship on the orbit the prediction drew.
 */
void test_planned_burn() {
    const SystemDef system = load_system(asset_path("assets/systems/nereid.json"));
    World world;
    world.attach_system(system, system.index_of("wayfarer"));

    const orbit::Elements before = world.planned_conic();
    check(before.a > 0.0, "planner: the ship starts on a conic about its primary");

    const double burn_at = 120.0;
    world.nodes.push_back(orbit::Node{burn_at, 300.0, 0.0});
    const orbit::Elements planned = world.planned_conic();
    check(planned.a > before.a, "planner: a prograde burn raises the predicted semi-major axis");

    check(world.warp_to_next_node(), "planner: entering the next node is a real transition");
    check(world.nodes.empty(), "planner: the burn is consumed, not left in the book");
    check_close(world.elapsed, burn_at, 1e-6, "planner: the warp lands on the node's own time");

    const orbit::Elements after = world.planned_conic();
    check_close(after.a, planned.a, std::fabs(planned.a) * 1e-6,
                "planner: the orbit flown is the orbit predicted");
    check(after.e > before.e, "planner: and it is no longer the orbit it started on");
    check(!world.warp_to_next_node(), "planner: an empty book has nothing to advance to");
}

}  // namespace

int world_tests() {
    const int before = selftest::failures();
    test_capture_path();
    test_undock();
    test_fast_approach_is_refused();
    test_no_ports_no_docking();
    test_planned_burn();
    return selftest::failures() - before;
}

}  // namespace opra
