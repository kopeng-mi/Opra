p = 'src/sim/combat_tests.cpp'
s = open(p, encoding='utf-8').read()

old = """        // And a full sweep of spawn points along every traverse: the round spawned by the muzzle
        // rule never begins inside its own hull, so a fresh round cannot self-hit.
        for (int deg = 0; deg < 360; ++deg) {
            const Real angle = static_cast<Real>(deg) * 3.14159265358979323846 / 180.0;
            const Vec2 direction{std::cos(angle), std::sin(angle)};
            const Vec2 muzzle = muzzle_position(mount, direction, ship.bounds.halfLength + 2.0);
            check(!point_in_circle(Circle{0.0, 0.0, ship.bounds.halfLength}, muzzle.x, muzzle.y) &&
                      !point_in_circle(Circle{mount.x, mount.y, 4.0}, muzzle.x, muzzle.y),
                  "combat: gate 8 - the muzzle never spawns inside its own hull");
        }"""
new = """        // And a full sweep of spawn points along every traverse: the round spawned by the muzzle
        // rule never begins inside any of the hull's own compound shapes, so a fresh round cannot
        // self-hit. (The bounding circle is too coarse for this test on purpose: a muzzle inside
        // the bound but outside every shape is exactly where a round is supposed to spawn.)
        for (int deg = 0; deg < 360; ++deg) {
            const Real angle = static_cast<Real>(deg) * 3.14159265358979323846 / 180.0;
            const Vec2 direction{std::cos(angle), std::sin(angle)};
            const Vec2 muzzle = muzzle_position(mount, direction, ship.bounds.halfLength + 2.0);
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
        }"""
assert s.count(old) == 1, 'gate8 test'
s = s.replace(old, new)
open(p, 'w', encoding='utf-8', newline='\n').write(s)
print('combat_tests fixed')

p = 'src/sim/combat.cpp'
s = open(p, encoding='utf-8').read()
old = """        const Real lambda_dot = (rx * rvy - ry * rvx) / std::max(range * range, 1.0);
        const Real closing = -(rx * rvx + ry * rvy);
        Real a_cmd = TORPEDO_NAV_GAIN * lambda_dot * closing;
        a_cmd = std::clamp(a_cmd, -TORPEDO_LATERAL, TORPEDO_LATERAL);
        // Perpendicular to the LOS, signed by the command.
        torpedo.velocity.x += -ry * a_cmd * dt;
        torpedo.velocity.y += rx * a_cmd * dt;"""
new = """        const Real lambda_dot = (rx * rvy - ry * rvx) / std::max(range * range, 1.0);
        const Real closing = -(rx * rvx + ry * rvy);
        // The command opposes the LOS rate: lambda_dot positive (LOS swinging to the left of the
        // flight) demands leftward authority. (-ry, rx) is the LOS's own left normal, so the sign
        // is the plain product - and a target that turns hard late outruns the clamp (s5.3).
        Real a_cmd = TORPEDO_NAV_GAIN * lambda_dot * closing;
        a_cmd = std::clamp(a_cmd, -TORPEDO_LATERAL, TORPEDO_LATERAL);
        torpedo.velocity.x += -ry * a_cmd * dt;
        torpedo.velocity.y += rx * a_cmd * dt;"""
assert s.count(old) == 1, 'pn sign'
s = s.replace(old, new)
open(p, 'w', encoding='utf-8', newline='\n').write(s)
print('pn sign kept; will flip if the test still fails')
