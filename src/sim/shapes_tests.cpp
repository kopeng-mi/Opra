// P4's checks: compound colliders, single-MTV resolution and the per-jet RCS solution. Every number
// below is derived on paper in the comment beside it, not captured from a run. The game wires this
// into run_selftest(); it returns its own failure count, like every module test file.
#include <cmath>
#include <utility>

#include "selftest.h"
#include "sim/physics.h"
#include "sim/shapes.h"
#include "sim/shapes_tests.h"

namespace opra {
namespace {

// The shared vocabulary, unqualified: these cases read as assertions, not as calls.
using selftest::check;
using selftest::check_close;

Shape box_at(Real x, Real y, Real half_length, Real half_width, Real angle = 0) {
    Shape shape;
    shape.kind = Shape::Kind::Box;
    shape.local_pos = {x, y};
    shape.local_angle = angle;
    shape.half_length = half_length;
    shape.half_width = half_width;
    return shape;
}

Shape circle_at(Real x, Real y, Real radius) {
    Shape shape;
    shape.kind = Shape::Kind::Circle;
    shape.local_pos = {x, y};
    shape.radius = radius;
    return shape;
}

Collider hull_of(std::vector<Shape> shapes, Real bounds_radius) {
    Collider collider;
    collider.shapes = std::move(shapes);
    collider.bounds_radius = bounds_radius;
    return collider;
}

/** A rock at (x, y) that is whole and on the flight plane, so resolve_collision will act on it. */
Obstacle rock_at(Real x, Real y, Real radius) {
    Obstacle rock;
    rock.x = x;
    rock.y = y;
    rock.radius = radius;
    rock.hp = 100;
    return rock;
}

ShipState ship_with(const Collider &hull, Real vx = 0, Real vy = 0) {
    ShipState ship = create_ship(ShipClass::Kestrel, hull);
    ship.angle = 0;
    ship.position = {0, 0};
    ship.velocity = {vx, vy};
    return ship;
}

/**
 * Only the near shape may resolve the contact. Two 20 x 40 boxes 60 m apart, the rock past the
 * starboard one's bow: box B spans x 20..40, so its face is at 40 and the rock's centre at 47 is
 * 7 m out, leaving 9.2 - 7 = 2.2 m of MTV. Box A is 77 m away and cannot see it.
 */
void test_single_shape_contact() {
    const Collider hull = hull_of({box_at(-30, 0, 20, 10), box_at(30, 0, 20, 10)}, 44.8);
    ShipState ship = ship_with(hull, 20, 0);
    const Obstacle rock = rock_at(47, 0, 10);
    const Real damage = resolve_collision(ship, rock);

    check_close(ship.position.x, -2.2, 1e-9, "collision: only the near shape moves the hull");
    check_close(ship.position.y, 0.0, 1e-12, "collision: a head-on contact pushes along x alone");
    // e = 1.3 leaves 0.3 of the closing speed: 20 m/s in, 6 m/s out along the contact normal.
    check_close(ship.velocity.x, -6.0, 1e-9, "collision: rebound is 0.3 of the closing speed");
    check(std::hypot(ship.velocity.x, ship.velocity.y) <= 6.0 + 1e-9,
          "collision: one MTV never amplifies the rebound");
    check(damage > 0 && ship.hull < 100.0, "collision: a 14 m/s closing speed costs hull");
}

/**
 * The deepest MTV is the one applied, and it is applied once. A plus-sign hull: the upright box
 * spans x +-8 and the crossbar y +-8, so a rock at (18, 12) pokes 1.04 m through the upright's
 * starboard face and 7.04 m through the crossbar's top face. Only the crossbar's MTV may move the
 * hull, and the reflection is a single one about -y: (20, 20) -> (20, -6), not double-counted.
 */
void test_deepest_mtv_only() {
    const Collider hull = hull_of({box_at(0, 0, 40, 8), box_at(0, 0, 8, 40)}, 41.0);
    ShipState ship = ship_with(hull, 20, 20);
    const Obstacle rock = rock_at(18, 12, 12);
    resolve_collision(ship, rock);

    check_close(ship.position.x, 0.0, 1e-12, "collision: the shallower MTV is discarded");
    check_close(ship.position.y, -7.04, 1e-9, "collision: the deepest MTV moves the hull");
    check_close(ship.velocity.x, 20.0, 1e-9, "collision: the tangential component is untouched");
    check_close(ship.velocity.y, -6.0, 1e-9, "collision: one reflection about the deepest normal");
}

/**
 * Containment: the rock's centre is inside the 8 x 20 box at local (0, 18). The shallowest way out
 * is forward (20 - 18 = 2 m) rather than starboard (8 m), so the rock leaves along +y and the hull
 * is pushed 2 + 2.76 = 4.76 m the other way.
 */
void test_containment() {
    const Collider hull = hull_of({box_at(0, 0, 20, 8)}, 21.5);
    ShipState ship = ship_with(hull);
    const Obstacle rock = rock_at(0, 18, 3);
    resolve_collision(ship, rock);

    check_close(ship.position.x, 0.0, 1e-12, "collision: containment does not push sideways");
    check_close(ship.position.y, -4.76, 1e-9, "collision: containment leaves by the shallowest face");
}

/**
 * Broad phase, and the reason the shape set exists at all. Two boxes with a 40 m gap: the rock at
 * (0, 30) is well inside the bounding circle but in open space, so nothing resolves. Then the same
 * hull's far corner: a rock touching it sits 53 m out, past `bounds_radius`, and the bound must
 * still let it through - a broad phase that culls a real contact is a hull that flies through rock.
 */
void test_broad_phase() {
    const Collider hull = hull_of({box_at(-30, 0, 20, 10), box_at(30, 0, 20, 10)}, 44.8);
    check(collider_clear(hull, Vec2{0, 0}, 400, 0, 9.2), "broad phase: a rock 400 m away is rejected");
    Vec2 push;
    // In the gap the broad phase cannot help: the narrow phase has to say no. This is the Mule's
    // open chassis, which the single box it replaced collided straight across.
    check(!collider_clear(hull, Vec2{0, 0}, 0, 30, 9.2),
          "broad phase: a rock in the gap is inside the bound, not rejected");
    check(!collider_circle_out(hull, Vec2{0, 0}, 0, 0, 30, 9.2, push),
          "collision: a rock in the hull's gap does not touch");
    check(collider_circle_out(hull, Vec2{0, 0}, 0, 47, 0, 9.2, push),
          "collision: a rock at the bow still touches through the broad phase");

    const Vec2 corner{-40, 20};
    const Real corner_range = std::hypot(corner.x, corner.y);
    const Vec2 outward{corner.x / corner_range, corner.y / corner_range};
    const Real centre = hull.bounds_radius + 9.2 - 1.0;
    check(!collider_clear(hull, Vec2{0, 0}, outward.x * centre, outward.y * centre, 9.2),
          "broad phase: a corner contact past the bound radius is not rejected");
    check(collider_circle_out(hull, Vec2{0, 0}, 0, outward.x * centre, outward.y * centre, 9.2,
                              push),
          "broad phase: the narrow phase still finds that corner contact");
}

/** Circle shapes collide too: the Mule's nose sphere and the Needle's stern sphere are circles. */
void test_circle_shape() {
    const Collider hull = hull_of({circle_at(0, 30, 12)}, 42.0);
    ShipState ship = ship_with(hull);
    const Obstacle rock = rock_at(0, 50, 10);
    resolve_collision(ship, rock);

    check_close(ship.position.x, 0.0, 1e-12, "collision: a circle shape pushes along the centre line");
    check_close(ship.position.y, -1.2, 1e-9, "collision: circle circle MTV is the overlap depth");
}

/**
 * The jet mapping. A jet's plume points outboard, so thrust is inboard and the torque a jet makes
 * about z is +1 for [starboard-fore, port-aft] and -1 for the other pair. A pure torque command
 * therefore lights exactly those two, at full authority, and nothing else.
 */
void test_rcs_jets() {
    ShipState torque = ship_with(box_collider(59, 34), 0, 0);
    torque.angularVelocity = 0;
    FlightInput turn;
    turn.turn = 1;
    step_ship(torque, turn, 1.0 / 120.0);
    check_close(torque.rcsJet[0], 1.0, 1e-12, "rcs: +torque lights the starboard-fore jet");
    check_close(torque.rcsJet[1], 0.0, 0.0, "rcs: +torque leaves the port-fore jet cold");
    check_close(torque.rcsJet[2], 0.0, 0.0, "rcs: +torque leaves the starboard-aft jet cold");
    check_close(torque.rcsJet[3], 1.0, 1e-12, "rcs: +torque lights the port-aft jet");

    ShipState negative = ship_with(box_collider(59, 34), 0, 0);
    FlightInput reverse;
    reverse.turn = -1;
    step_ship(negative, reverse, 1.0 / 120.0);
    check_close(negative.rcsJet[0], 0.0, 0.0, "rcs: -torque leaves the starboard-fore jet cold");
    check_close(negative.rcsJet[1], 1.0, 1e-12, "rcs: -torque lights the port-fore jet");
    check_close(negative.rcsJet[2], 1.0, 1e-12, "rcs: -torque lights the starboard-aft jet");
    check_close(negative.rcsJet[3], 0.0, 0.0, "rcs: -torque leaves the port-aft jet cold");

    // A lateral translation is the other couple: straight to starboard is thrust +x, which only the
    // port jets make, and it must be torque-neutral.
    ShipState strafe = ship_with(box_collider(59, 34), 0, 0);
    FlightInput slide;
    slide.strafe = 1;
    step_ship(strafe, slide, 1.0 / 120.0);
    check_close(strafe.rcsJet[0], 0.0, 0.0, "rcs: a strafe leaves the starboard-fore jet cold");
    check_close(strafe.rcsJet[1], 1.0, 1e-12, "rcs: a strafe lights the port-fore jet");
    check_close(strafe.rcsJet[2], 0.0, 0.0, "rcs: a strafe leaves the starboard-aft jet cold");
    check_close(strafe.rcsJet[3], 1.0, 1e-12, "rcs: a strafe lights the port-aft jet");
    check_close(strafe.angularVelocity, 0.0, 1e-12, "rcs: the lateral pair makes no torque");

    // Coasting with no command: nothing fires, so a parked ship shows no jets.
    ShipState coasting = ship_with(box_collider(59, 34), 0, 0);
    step_ship(coasting, FlightInput{}, 1.0 / 120.0);
    for (int i = 0; i < 4; ++i) {
        check_close(coasting.rcsJet[i], 0.0, 0.0, "rcs: coasting lights no jet");
    }
}

}  // namespace

int shapes_tests() {
    const int before = selftest::failures();
    test_single_shape_contact();
    test_deepest_mtv_only();
    test_containment();
    test_broad_phase();
    test_circle_shape();
    test_rcs_jets();
    return selftest::failures() - before;
}

}  // namespace opra
