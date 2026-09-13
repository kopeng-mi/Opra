p = 'src/sim/combat.cpp'
s = open(p, encoding='utf-8').read()

old = """        const Real rx = dx / range, ry = dy / range;
        const Real rvx = target_velocity.x - torpedo.velocity.x;
        const Real rvy = target_velocity.y - torpedo.velocity.y;
        // The command opposes the LOS rate: lambda_dot positive (LOS swinging to the left of the
        // flight) demands leftward authority. (-ry, rx) is the LOS's own left normal, so the sign
        // is the plain product - and a target that turns hard late outruns the clamp (s5.3).
        Real a_cmd = TORPEDO_NAV_GAIN * lambda_dot * closing;"""
old_actual = """        const Real rx = dx / range, ry = dy / range;
        const Real rvx = target_velocity.x - torpedo.velocity.x;
        const Real rvy = target_velocity.y - torpedo.velocity.y;
        const Real lambda_dot = (rx * rvy - ry * rvx) / std::max(range * range, 1.0);
        const Real closing = -(rx * rvx + ry * rvy);
        // The command opposes the LOS rate: lambda_dot positive (LOS swinging to the left of the
        // flight) demands leftward authority. (-ry, rx) is the LOS's own left normal, so the sign
        // is the plain product - and a target that turns hard late outruns the clamp (s5.3).
        Real a_cmd = TORPEDO_NAV_GAIN * lambda_dot * closing;"""
new = """        const Real rx = dx / range, ry = dy / range;
        const Real rvx = target_velocity.x - torpedo.velocity.x;
        const Real rvy = target_velocity.y - torpedo.velocity.y;
        // lambda_dot = (r x v) / r.r, with r and v raw: the LOS's angular rate in rad/s. Dividing
        // the normalized cross by r.r a second time was the quiet factor-of-range that left the
        // torpedo nudging at millimetres per second squared.
        const Real lambda_dot = (dx * rvy - dy * rvx) / std::max(range * range, 1.0);
        const Real closing = -(rx * rvx + ry * rvy);
        // The command opposes the LOS rate: lambda_dot positive (LOS swinging to the left of the
        // flight) demands leftward authority. (-ry, rx) is the LOS's own left normal, so the sign
        // is the plain product - and a target that turns hard late outruns the clamp (s5.3).
        Real a_cmd = TORPEDO_NAV_GAIN * lambda_dot * closing;"""
assert s.count(old_actual) == 1, 'pn'
s = s.replace(old_actual, new)

# muzzle spawn: add the march that clears the hull's own compound shapes
old = """Vec2 muzzle_position(const Vec2 &mount, const Vec2 &direction, Real hull_radius) {
    return {mount.x + direction.x * hull_radius, mount.y + direction.y * hull_radius};
}"""
new = """Vec2 muzzle_position(const Vec2 &mount, const Vec2 &direction, Real hull_radius) {
    // s5.5: at the muzzle plus the hull radius along the barrel, never at the turret origin - and
    // then marched clear, because a midship mount firing aft would otherwise spawn its round
    // inside its own hull no matter what the radius was.
    Vec2 at{mount.x + direction.x * hull_radius, mount.y + direction.y * hull_radius};
    return at;
}

/**
 * The spawn point the sim actually uses: `muzzle_position`, then advanced along the barrel until
 * the point is outside every one of the firing ship's own compound shapes. True when the point
 * ended clear; the caller fires from it.
 */
Vec2 clear_muzzle(const Collider &own, const Vec2 &ship_position, Real ship_angle,
                  const Vec2 &mount, const Vec2 &direction, Real hull_radius) {
    Vec2 at = muzzle_position(mount, direction, hull_radius);
    const Real c = std::cos(ship_angle), s = std::sin(ship_angle);
    const auto inside = [&](const Vec2 &p) {
        for (const Shape &shape : own.shapes) {
            if (shape.kind == Shape::Kind::Circle) {
                const Real wx = ship_position.x + shape.local_pos.x * c - shape.local_pos.y * s;
                const Real wy = ship_position.y + shape.local_pos.x * s + shape.local_pos.y * c;
                if (point_in_circle(Circle{wx, wy, 0.0}, p.x, p.y, shape.radius)) return true;
            } else {
                const Real wx = ship_position.x + shape.local_pos.x * c - shape.local_pos.y * s;
                const Real wy = ship_position.y + shape.local_pos.x * s + shape.local_pos.y * c;
                if (point_in_box(Box{wx, wy, shape.half_length, shape.half_width,
                                     shape.local_angle + ship_angle},
                                 p.x, p.y)) {
                    return true;
                }
            }
        }
        return false;
    };
    if (own.shapes.empty()) return at;
    for (int march = 0; march < 32 && inside(at); ++march) {
        at.x += direction.x * hull_radius * 0.25;
        at.y += direction.y * hull_radius * 0.25;
    }
    return at;
}"""
assert s.count(old) == 1, 'muzzle'
s = s.replace(old, new)
open(p, 'w', encoding='utf-8', newline='\n').write(s)
print('combat.cpp fixed')

# point_in_circle signature check fix: it takes (Circle, x, y) - fix my helper usage
s = open(p, encoding='utf-8').read()
old = "                if (point_in_circle(Circle{wx, wy, 0.0}, p.x, p.y, shape.radius)) return true;"
new = "                if (point_in_circle(Circle{wx, wy, shape.radius}, p.x, p.y)) return true;"
assert s.count(old) == 1, 'circle call'
s = s.replace(old, new)
open(p, 'w', encoding='utf-8', newline='\n').write(s)
print('circle call fixed')

# world.cpp: use clear_muzzle when firing
p = 'src/sim/world.cpp'
s = open(p, encoding='utf-8').read()
old = "            const Vec2 muzzle = muzzle_position(mount, aim, ship.bounds.halfLength + 2.0);"
new = """            const Vec2 muzzle = clear_muzzle(ship.collider, ship.position, ship.angle, mount, aim,
                                             ship.bounds.halfLength + 2.0);"""
assert s.count(old) == 1, 'world muzzle'
s = s.replace(old, new)
open(p, 'w', encoding='utf-8', newline='\n').write(s)
print('world.cpp muzzle ok')

# combat.h: declare clear_muzzle
p = 'src/sim/combat.h'
s = open(p, encoding='utf-8').read()
old = """/**
 * s5.2's own-hull occlusion:"""
new = """/**
 * The spawn point the sim fires from: `muzzle_position` marched along the barrel until it is
 * outside every one of the firing ship's own compound shapes (s5.5's spawn rule, made robust to a
 * midship mount firing across its own hull).
 */
Vec2 clear_muzzle(const Collider &own, const Vec2 &ship_position, Real ship_angle, const Vec2 &mount,
                  const Vec2 &direction, Real hull_radius);

/**
 * s5.2's own-hull occlusion:"""
assert s.count(old) == 1, 'combat.h'
s = s.replace(old, new)
open(p, 'w', encoding='utf-8', newline='\n').write(s)
print('combat.h ok')

# test: use clear_muzzle for the spawn check
p = 'src/sim/combat_tests.cpp'
s = open(p, encoding='utf-8').read()
old = """            const Vec2 muzzle = muzzle_position(mount, direction, ship.bounds.halfLength + 2.0);
            bool inside_any = false;"""
new = """            const Vec2 muzzle =
                clear_muzzle(ship.collider, Vec2{0.0, 0.0}, 0.0, mount, direction,
                             ship.bounds.halfLength + 2.0);
            bool inside_any = false;"""
assert s.count(old) == 1, 'test muzzle'
s = s.replace(old, new)
open(p, 'w', encoding='utf-8', newline='\n').write(s)
print('test muzzle ok')
