p = 'src/sim/world.cpp'
s = open(p, encoding='utf-8').read()

old = """constexpr Real PDC_ENGAGE_RANGE = 700.0;  // the release the mount opens fire inside

}  // namespace"""
new = """constexpr Real PDC_ENGAGE_RANGE = 700.0;  // the release the mount opens fire inside

/**
 * The compound shape a swept segment struck first, or -1. The round's whole segment is tested
 * against each shape - a circle by the segment-circle hit, a box by stepping the point test along
 * the segment - and the earliest strike wins, because that is the shape the damage lands on (s5.4).
 */
int swept_ship_shape(const Collider &collider, const Vec2 &ship_position, Real ship_angle,
                     const Vec2 &from, const Vec2 &to) {
    const Real c = std::cos(ship_angle), sn = std::sin(ship_angle);
    int best = -1;
    Real best_t = 2.0;
    for (int i = 0; i < static_cast<int>(collider.shapes.size()); ++i) {
        const Shape &shape = collider.shapes[static_cast<size_t>(i)];
        const Real wx = ship_position.x + shape.local_pos.x * c - shape.local_pos.y * sn;
        const Real wy = ship_position.y + shape.local_pos.x * sn + shape.local_pos.y * c;
        if (shape.kind == Shape::Kind::Circle) {
            Real t = 2.0;
            if (segment_circle_hit(from.x, from.y, to.x, to.y, Circle{wx, wy, 0.0}, shape.radius,
                                   t) &&
                t < best_t) {
                best_t = t;
                best = i;
            }
            continue;
        }
        const Box box{wx, wy, shape.half_length, shape.half_width, shape.local_angle + ship_angle};
        for (int k = 0; k <= 6; ++k) {
            const Real f = static_cast<Real>(k) / 6.0;
            if (point_in_box(box, from.x + (to.x - from.x) * f, from.y + (to.y - from.y) * f)) {
                if (f < best_t) {
                    best_t = f;
                    best = i;
                }
                break;
            }
        }
    }
    return best;
}

}  // namespace"""
assert s.count(old) == 1, 'swept helper'
s = s.replace(old, new)

# drop the old shape_is_aft if present (its job moved to combat.cpp's shape_is_drive)
old_aft = """/** Where a shape sits in the ship: aft shapes feed the drives, fore shapes the tanks (s5.4). */
bool shape_is_aft(const Shape &shape, const Collider &collider) {
    Real half_length = 1.0;
    for (const Shape &other : collider.shapes) {
        half_length = std::max<Real>(half_length, std::abs(other.local_pos.y) +
                                                      (other.kind == Shape::Kind::Circle
                                                           ? other.radius
                                                           : other.half_length));
    }
    return shape.local_pos.y < -half_length * 0.15;
}

"""
if old_aft in s:
    s = s.replace(old_aft, '')
    print('old shape_is_aft removed')

open(p, 'w', encoding='utf-8', newline='\n').write(s)
print('world.cpp helper ok')
