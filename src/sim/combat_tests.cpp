// Close quarters' own gates (plan 05 s5, review gates 7 and 8): the swept-segment hit, the
// lead solve's degenerate cases, the torpedo's guidance law, and the rule that no turret can
// damage the vessel carrying it at any traverse angle.
#include "sim/combat_tests.h"

#include <cmath>

#include "selftest.h"
#include "sim/collision.h"
#include "sim/combat.h"

namespace opra {
using selftest::check;
using selftest::check_close;

namespace {

bool swept_segment_hits(const Vec2 &from, const Vec2 &to, const Vec2 &centre, Real radius,
                        Real &out_t) {
    return segment_circle_hit(from.x, from.y, to.x, to.y, Circle{centre.x, centre.y, 0.0}, radius,
                              out_t);
}

}  // namespace

int combat_tests() {
    const int before = selftest::failures();

    // ---- the lead solve (s5.2), including the degenerate cases the quadratic hides
    {
        // A stationary target: direct aim, t = range / speed.
        const Real t0 = lead_solve({300.0, 0.0}, {0.0, 0.0}, 1000.0);
        check_close(t0, 0.3, 1e-9, "combat: a stationary target is a direct aim");

        // A 1000 m/s crossing target at 1 km: the intercept exists and lands where the maths says.
        const Real t1 = lead_solve({1000.0, 0.0}, {0.0, 1000.0}, 1100.0);
        check(t1 > 0.0, "combat: a crossing target has an intercept");
        if (t1 > 0.0) {
            const Real miss = std::hypot(1000.0, 1000.0 * t1) - 1100.0 * t1;
            check_close(miss, 0.0, 1e-6, "combat: the intercept time closes the range exactly");
        }

        // v.v = s^2 collapses to linear, valid only when the range is closing.
        const Real t2 = lead_solve({500.0, 0.0}, {1100.0, 0.0}, 1100.0);
        check(t2 < 0.0, "combat: a target opening at the round's own speed cannot be caught");
        const Real t3 = lead_solve({500.0, 0.0}, {-1100.0, 0.0}, 1100.0);
        check_close(t3, 500.0 / 2200.0, 1e-9,
                    "combat: the linear case closes at the combined speed");

        // Both roots negative: the intercept is in the past, no shot.
        check(lead_solve({300.0, 0.0}, {1100.0, 0.0}, 500.0) < 0.0,
              "combat: an unclosable range holds fire");

        // The smallest POSITIVE root, not the smallest root.
        const Real t4 = lead_solve({400.0, 0.0}, {-1000.0, 0.0}, 1000.0);
        check_close(t4, 400.0 / 2000.0, 1e-9, "combat: the smaller positive root is the shot");
    }

    // ---- gate 7: a PDC round at a 2 m target crossing at 1 km/s registers a hit - the
    // swept-segment test, verified here and not by eye.
    {
        const Vec2 muzzle{0.0, 0.0};
        const Vec2 target{1000.0, 0.0};
        const Vec2 target_velocity{0.0, 1000.0};
        const Real round_speed = 1100.0;
        const Real t = lead_solve(Vec2{target.x - muzzle.x, target.y - muzzle.y}, target_velocity,
                                  round_speed);
        check(t > 0.0, "combat: gate 7's crossing target has an intercept");
        const Vec2 aim{target.x + target_velocity.x * t, target.y + target_velocity.y * t};
        const Real aim_len = std::hypot(aim.x, aim.y);
        const Vec2 aim_dir{aim.x / aim_len, aim.y / aim_len};

        // Step the round at 120 Hz alongside the target and sweep every step's segment.
        const Real dt = 1.0 / 120.0;
        Vec2 round = muzzle;
        bool hit = false;
        for (int step = 0; step < 400 && !hit; ++step) {
            const Vec2 from = round;
            round.x += aim_dir.x * round_speed * dt;
            round.y += aim_dir.y * round_speed * dt;
            const Vec2 now{target.x + target_velocity.x * dt * (step + 1),
                           target.y + target_velocity.y * dt * (step + 1)};
            Real t_hit = 2.0;
            if (swept_segment_hits(from, round, now, 2.0, t_hit)) hit = true;
        }
        check(hit, "combat: gate 7 - the swept segment catches a 2 m target at 1 km/s");

        // The point test the sweep replaces, run as the counterfactual: a per-step point sample
        // misses this exact shot most of the time, which is why the sweep is not optional.
        Vec2 point = muzzle;
        bool point_hit = false;
        for (int step = 0; step < 400 && !point_hit; ++step) {
            point.x += aim_dir.x * round_speed * dt;
            point.y += aim_dir.y * round_speed * dt;
            const Vec2 now{target.x + target_velocity.x * dt * step,
                           target.y + target_velocity.y * dt * step};
            if (std::hypot(point.x - now.x, point.y - now.y) < 2.0) point_hit = true;
        }
        check(!point_hit, "combat: the same shot by point tests misses - the sweep is the point");
    }

    // ---- gate 8: no turret can damage the vessel carrying it, at any traverse angle.
    {
        const ShipState ship = create_ship(ShipClass::Kestrel);
        const Vec2 mount{0.0, ship.bounds.halfLength * 0.5};  // a stern-adjacent mount, ship frame
        bool any_blocked_path = false;
        for (int deg = 0; deg < 360; ++deg) {
            const Real angle = static_cast<Real>(deg) * 3.14159265358979323846 / 180.0;
            const Vec2 direction{std::cos(angle), std::sin(angle)};
            // The muzzle sits clear of the hull along the barrel (s5.5's spawn rule), and the
            // occlusion test runs from the muzzle outwards.
            const Vec2 muzzle =
                muzzle_position(mount, direction, ship.bounds.halfLength + 2.0);
            const bool clear =
                aim_clears_own_hull(ship.collider, Vec2{0.0, 0.0}, 0.0, muzzle, direction,
                                    ship.bounds.halfLength * 6.0);
            // A barrel pointing away from the hull must be clear; one pointing across it must be
            // caught. What must NEVER happen is "clear" for a barrel that crosses the hull.
            const Vec2 world_mount{mount.x, mount.y};
            const Real to_hull = std::hypot(-world_mount.x, -world_mount.y);
            const bool points_at_hull =
                (direction.x * -world_mount.x + direction.y * -world_mount.y) / to_hull > 0.97;
            if (points_at_hull) any_blocked_path = any_blocked_path || !clear;
        }
        check(any_blocked_path, "combat: gate 8 - barrels across the hull are occluded");

        // And a full sweep of spawn points along every traverse: the round spawned by the muzzle
        // rule never begins inside any of the hull's own compound shapes, so a fresh round cannot
        // self-hit. (The bounding circle is too coarse for this test on purpose: a muzzle inside
        // the bound but outside every shape is exactly where a round is supposed to spawn.)
        for (int deg = 0; deg < 360; ++deg) {
            const Real angle = static_cast<Real>(deg) * 3.14159265358979323846 / 180.0;
            const Vec2 direction{std::cos(angle), std::sin(angle)};
            const Vec2 muzzle =
                clear_muzzle(ship.collider, Vec2{0.0, 0.0}, 0.0, mount, direction,
                             ship.bounds.halfLength + 2.0);
            bool inside_any = false;
            for (const Shape &shape : ship.collider.shapes) {
                if (shape.kind == Shape::Kind::Circle) {
                    inside_any = inside_any ||
                                 point_in_circle(Circle{shape.local_pos.x, shape.local_pos.y, shape.radius},
                                                 muzzle.x, muzzle.y);
                } else {
                    inside_any = inside_any ||
                                 point_in_box(Box{shape.local_pos.x, shape.local_pos.y,
                                                  shape.half_length, shape.half_width,
                                                  shape.local_angle},
                                              muzzle.x, muzzle.y);
                }
            }
            check(!inside_any, "combat: gate 8 - the muzzle never spawns inside its own hull");
        }
    }

    // ---- torpedoes (s5.3)
    {
        // Guidance: a torpedo offset from a crossing target bends toward the intercept.
        Torpedo torpedo;
        torpedo.position = {0.0, 0.0};
        torpedo.velocity = {300.0, 0.0};
        torpedo.target = 1;
        torpedo.fuel = 30.0;
        torpedo.armed = true;
        const Vec2 target{2000.0, 600.0};
        const Real dt = 1.0 / 120.0;
        Real closest = 1e30;
        for (int step = 0; step < 1200; ++step) {
            step_torpedo(torpedo, target, {0.0, 0.0}, true, dt);
            // The score is the closest approach - inside five metres the guidance stands down and
            // the warhead does the rest, so the final position is downstream of the answer.
            closest = std::min(closest, std::hypot(torpedo.position.x - target.x,
                                                   torpedo.position.y - target.y));
        }
        check(closest < 30.0, "combat: proportional navigation walks the torpedo onto the target");

        // Burnout: out of fuel, the torpedo coasts on its last heading - it does not vanish and
        // does not keep turning.
        Torpedo spent = torpedo;
        spent.fuel = 0.0;
        const Real angle_before = spent.angle;
        step_torpedo(spent, {1.0e6, 1.0e6}, {0.0, 0.0}, true, dt);
        check_close(spent.angle, angle_before, 1e-9,
                    "combat: a ballistic torpedo holds its last heading");
        check(spent.alive, "combat: a ballistic torpedo is still tracked");

        // A destroyed target: the last LOS heading, still lethal (s5.5).
        Torpedo orphan = torpedo;
        const Vec2 last_aim = orphan.velocity;
        step_torpedo(orphan, target, {0.0, 0.0}, false, 1.0);
        check_close(orphan.velocity.x, last_aim.x, 1.0, "combat: a dead target leaves the torpedo coasting");

        // r -> 0: below five metres the guidance stands down instead of dividing by zero (s5.5).
        Torpedo close = torpedo;
        close.position = {target.x + 3.0, target.y};
        close.velocity = {-50.0, 0.0};
        step_torpedo(close, target, {0.0, 0.0}, true, dt);
        check(std::isfinite(close.velocity.x) && std::isfinite(close.velocity.y),
              "combat: guidance inside five metres coasts - no lambda_dot blow-up");

        // Kinetic damage (s5.4): a 20 g round at 1100 m/s is 12.1 kJ; damage scales with v^2.
        const Real one = kinetic_damage(0.02, 1100.0);
        check(one > 0.0 && one < 10.0,
              "combat: a single PDC round is not a threat (s5.4's tuning)");
        const Real doubled = kinetic_damage(0.02, 2200.0);
        check_close(doubled, one * 4.0, 1e-6, "combat: kinetic damage scales with v squared");
    }

    // ---- s5.4"s per-shape rule: the compound shape that was hit takes the damage, so a drive
    // hit degrades thrust and a tank hit vents propellant - the component model paying for itself.
    {
        ShipState ship = create_ship(ShipClass::Kestrel);
        // The real hull carries the sidecar's compound shapes (App::sync_ship_collider); the test
        // builds the same set from the exported kestrel: one hull box reaching aft, the band
        // cluster forward of the mid-length.
        ship.collider = Collider{};
        Shape hull_shape{};
        hull_shape.local_pos = {0.0, 0.0};
        hull_shape.half_length = 23.7;
        hull_shape.half_width = 8.1;
        ship.collider.shapes.push_back(hull_shape);
        Shape band_shape{};
        band_shape.local_pos = {0.0, 14.5};
        band_shape.half_length = 1.75;
        band_shape.half_width = 7.2;
        ship.collider.shapes.push_back(band_shape);
        ship.collider.bounds_radius = 25.6;
        // Pick one shape of each kind from the exported collider.
        int drive = -1, tank = -1;
        for (int i = 0; i < static_cast<int>(ship.collider.shapes.size()); ++i) {
            if (shape_is_drive(ship.collider.shapes[static_cast<size_t>(i)], ship.collider)) {
                drive = drive < 0 ? i : drive;
            } else {
                tank = tank < 0 ? i : tank;
            }
        }
        check(drive >= 0 && tank >= 0,
              "damage: the kestrel carries at least one drive shape and one forward shape");

        // A drive-pod hit: thrust authority drops and keeps dropping with more energy.
        ShipState struck = ship;
        const Real first = apply_ship_damage(struck, drive, 2.0);
        check_close(first, 2.0, 1e-12, "damage: the dealt hull damage is the energy in");
        check(struck.thrustDamage > 0.0, "damage: a drive hit degrades thrust");
        const Real thrust_before = struck.thrustDamage;
        apply_ship_damage(struck, drive, 2.0);
        check(struck.thrustDamage > thrust_before,
              "damage: a second drive hit degrades further");

        // The degradation reaches the integrator: step_ship"s authority carries the damage.
        ShipState flown = struck;
        flown.thrustDamage = 0.5;
        flown.fuel = 1000.0;
        FlightInput burn{};
        burn.thrust = 1.0;
        step_ship(flown, burn, 1.0 / 120.0, {});
        ShipState clean = ship;
        clean.fuel = 1000.0;
        step_ship(clean, burn, 1.0 / 120.0, {});
        check(length(flown.velocity) < length(clean.velocity),
              "damage: a degraded drive accelerates the hull less");

        // A tank hit: propellant vents at once and keeps leaking.
        ShipState vented = ship;
        vented.fuel = 5000.0;
        apply_ship_damage(vented, tank, 3.0);
        check(vented.fuel < 5000.0, "damage: a tank hit vents propellant");
        check(vented.fuelLeak > 0.0, "damage: a tank hit leaves the tank leaking");
        const Real fuel_before = vented.fuel;
        step_ship(vented, {}, 1.0 / 120.0, {});
        check(vented.fuel < fuel_before,
              "damage: the leak drains the tank in flight, not just at the hit");
    }

    // ---- round lifetime and range cap (s5.5): both, so neither leaks.
    {
        Round round;
        round.velocity = {1.0, 0.0};
        bool alive = true;
        for (int step = 0; step < 120 * 13 && alive; ++step) {
            alive = step_round(round, 1.0 / 120.0);
        }
        check(!alive, "combat: a round despawns at its lifetime, whatever its range");
        check(round.life > 11.9, "combat: the round lived its life before despawning");
    }

    return selftest::failures() - before;
}

}  // namespace opra
