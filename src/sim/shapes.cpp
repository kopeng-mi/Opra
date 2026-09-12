#include "sim/shapes.h"

#include <algorithm>
#include <cmath>

namespace opra {
namespace {

/** Rotates `v` by cos/sin, the hull's local -> world basis. */
Vec2 rotate(const Vec2 &v, Real cos, Real sin) {
    return {v.x * cos - v.y * sin, v.x * sin + v.y * cos};
}

/** The shape's centre in world space under a hull pose. */
Vec2 posed_centre(const Shape &shape, const Vec2 &position, Real cos, Real sin) {
    const Vec2 offset = rotate(shape.local_pos, cos, sin);
    return {position.x + offset.x, position.y + offset.y};
}

/**
 * The MTV that pushes a world circle out of one posed shape, flipped into the hull's favour: the
 * push moves the hull away from the circle. False when the two do not overlap.
 */
bool shape_versus_circle(const Shape &shape, const Vec2 &centre, Real angle, Real cx, Real cy,
                         Real radius, Vec2 &out) {
    if (shape.kind == Shape::Kind::Box) {
        const Box box{centre.x, centre.y, shape.half_length, shape.half_width,
                      angle + shape.local_angle};
        Vec2 circle_push;
        if (!obb_circle_out(box, cx, cy, radius, circle_push)) return false;
        out = {-circle_push.x, -circle_push.y};
        return true;
    }
    const Real dx = centre.x - cx, dy = centre.y - cy;
    const Real distance = std::hypot(dx, dy);
    const Real reach = shape.radius + radius;
    if (distance >= reach) return false;
    const Real penetration = reach - distance;
    // Concentric circles have no separating direction: leave along the hull's own x, which stays
    // deterministic when two identical shapes are stacked on the same centre.
    const Vec2 direction = distance > 1e-9 ? Vec2{dx / distance, dy / distance}
                                           : rotate(Vec2{1, 0}, std::cos(angle), std::sin(angle));
    out = {direction.x * penetration, direction.y * penetration};
    return true;
}

/** The MTV that pushes a world box off one posed shape, in the hull's favour. */
bool shape_versus_box(const Shape &shape, const Vec2 &centre, Real angle, const Box &box, Vec2 &out) {
    if (shape.kind == Shape::Kind::Circle) {
        Vec2 circle_push;
        if (!obb_circle_out(box, centre.x, centre.y, shape.radius, circle_push)) return false;
        out = {-circle_push.x, -circle_push.y};
        return true;
    }
    const Box own{centre.x, centre.y, shape.half_length, shape.half_width, angle + shape.local_angle};
    return obb_obb_out(box, own, out);
}

/**
 * Keeps the deepest MTV of the two, so a compound hull resolves a contact once. `deepest` < 0 means
 * no shape has reported yet.
 */
void keep_deepest(Real &deepest, Vec2 &best, const Vec2 &candidate) {
    const Real penetration = std::hypot(candidate.x, candidate.y);
    if (penetration <= deepest) return;
    deepest = penetration;
    best = candidate;
}

}  // namespace

Collider box_collider(Real half_length, Real half_width) {
    Collider collider;
    Shape box;
    box.kind = Shape::Kind::Box;
    box.half_length = half_length;
    box.half_width = half_width;
    collider.shapes.push_back(box);
    collider.bounds_radius = std::hypot(half_length, half_width);
    return collider;
}

HullBoxes collider_bounds(const Collider &collider) {
    HullBoxes bounds{0, 0};
    for (const Shape &shape : collider.shapes) {
        Real extent_x = shape.radius;
        Real extent_y = shape.radius;
        if (shape.kind == Shape::Kind::Box) {
            const Real cos = std::abs(std::cos(shape.local_angle));
            const Real sin = std::abs(std::sin(shape.local_angle));
            extent_x = cos * shape.half_width + sin * shape.half_length;
            extent_y = sin * shape.half_width + cos * shape.half_length;
        }
        bounds.halfWidth = std::max(bounds.halfWidth, std::abs(shape.local_pos.x) + extent_x);
        bounds.halfLength = std::max(bounds.halfLength, std::abs(shape.local_pos.y) + extent_y);
    }
    return bounds;
}

bool collider_clear(const Collider &collider, const Vec2 &position, Real cx, Real cy, Real radius) {
    if (collider.shapes.empty()) return true;
    return std::hypot(cx - position.x, cy - position.y) > collider.bounds_radius + radius;
}

bool collider_circle_out(const Collider &collider, const Vec2 &position, Real angle, Real cx, Real cy,
                         Real radius, Vec2 &out) {
    if (collider_clear(collider, position, cx, cy, radius)) return false;
    const Real cos = std::cos(angle), sin = std::sin(angle);
    Real deepest = -1;
    Vec2 best{};
    for (const Shape &shape : collider.shapes) {
        const Vec2 centre = posed_centre(shape, position, cos, sin);
        Vec2 candidate{};
        if (shape_versus_circle(shape, centre, angle, cx, cy, radius, candidate)) {
            keep_deepest(deepest, best, candidate);
        }
    }
    if (deepest < 0) return false;
    out = best;
    return true;
}

bool collider_box_out(const Collider &collider, const Vec2 &position, Real angle, const Box &box,
                      Vec2 &out) {
    const Real reach = collider.bounds_radius + std::hypot(box.halfLength, box.halfWidth);
    if (std::hypot(box.x - position.x, box.y - position.y) > reach) return false;
    const Real cos = std::cos(angle), sin = std::sin(angle);
    Real deepest = -1;
    Vec2 best{};
    for (const Shape &shape : collider.shapes) {
        const Vec2 centre = posed_centre(shape, position, cos, sin);
        Vec2 candidate{};
        if (shape_versus_box(shape, centre, angle, box, candidate)) {
            keep_deepest(deepest, best, candidate);
        }
    }
    if (deepest < 0) return false;
    out = best;
    return true;
}

}  // namespace opra
