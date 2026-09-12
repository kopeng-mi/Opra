#include "sim/collision.h"

#include <cmath>
#include <limits>

namespace opra {

bool obb_circle_out(const Box &box, Real cx, Real cy, Real radius, Vec2 &out) {
    const Real cos = std::cos(box.angle), sin = std::sin(box.angle);
    const Real dx = cx - box.x, dy = cy - box.y;
    // Circle centre in box-local space: local +y is forward, local +x is starboard.
    const Real localX = dx * cos + dy * sin;
    const Real localY = -dx * sin + dy * cos;
    const Real clampX = localX < -box.halfWidth ? -box.halfWidth : localX > box.halfWidth ? box.halfWidth : localX;
    const Real clampY =
        localY < -box.halfLength ? -box.halfLength : localY > box.halfLength ? box.halfLength : localY;
    Real nx = localX - clampX, ny = localY - clampY;
    const Real distance2 = std::hypot(nx, ny);
    Real push;
    if (distance2 > 1e-6) {
        if (distance2 >= radius) return false;
        push = radius - distance2;
        nx /= distance2;
        ny /= distance2;
    } else {
        // Centre is inside the box: leave along the shallowest face.
        const Real overshootX = box.halfWidth - std::abs(localX);
        const Real overshootY = box.halfLength - std::abs(localY);
        if (overshootX < overshootY) {
            nx = localX < 0 ? -1 : 1;
            ny = 0;
            push = overshootX + radius;
        } else {
            nx = 0;
            ny = localY < 0 ? -1 : 1;
            push = overshootY + radius;
        }
    }
    // Back to world space. Local->world is the transpose of the rotation used above.
    out.x = (nx * cos - ny * sin) * push;
    out.y = (nx * sin + ny * cos) * push;
    return true;
}

namespace {

Real project_extent(const Box &box, Real axisX, Real axisY) {
    const Real forward =
        std::abs(-std::sin(box.angle) * axisX + std::cos(box.angle) * axisY) * box.halfLength;
    const Real starboard =
        std::abs(std::cos(box.angle) * axisX + std::sin(box.angle) * axisY) * box.halfWidth;
    return forward + starboard;
}

}  // namespace

bool obb_obb_out(const Box &a, const Box &b, Vec2 &out) {
    const Real axes[4][2] = {
        {std::cos(a.angle), std::sin(a.angle)},
        {-std::sin(a.angle), std::cos(a.angle)},
        {std::cos(b.angle), std::sin(b.angle)},
        {-std::sin(b.angle), std::cos(b.angle)},
    };
    const Real dx = b.x - a.x, dy = b.y - a.y;
    Real best = std::numeric_limits<Real>::infinity();
    Real bestX = 0, bestY = 0;
    for (const auto &axis : axes) {
        const Real extentA = project_extent(a, axis[0], axis[1]);
        const Real extentB = project_extent(b, axis[0], axis[1]);
        const Real distance = dx * axis[0] + dy * axis[1];
        const Real overlap = extentA + extentB - std::abs(distance);
        if (overlap <= 0) return false;
        if (overlap < best) {
            best = overlap;
            const Real sign = distance < 0 ? -1 : 1;
            bestX = axis[0] * sign;
            bestY = axis[1] * sign;
        }
    }
    out.x = bestX * best;
    out.y = bestY * best;
    return true;
}

bool point_in_circle(const Circle &circle, Real x, Real y) {
    return std::hypot(x - circle.x, y - circle.y) <= circle.radius;
}

bool point_in_box(const Box &box, Real x, Real y) {
    const Real cos = std::cos(box.angle), sin = std::sin(box.angle);
    const Real dx = x - box.x, dy = y - box.y;
    const Real localX = dx * cos + dy * sin;
    const Real localY = -dx * sin + dy * cos;
    return std::abs(localX) <= box.halfWidth && std::abs(localY) <= box.halfLength;
}

bool segment_circle_hit(Real x0, Real y0, Real x1, Real y1, const Circle &circle, Real radius,
                        Real &out_t) {
    const Real dx = x1 - x0, dy = y1 - y0;
    const Real fx = x0 - circle.x, fy = y0 - circle.y;
    const Real c = fx * fx + fy * fy - radius * radius;
    if (c <= 0) {
        out_t = 0;
        return true;
    }
    const Real a = dx * dx + dy * dy;
    if (a < 1e-12) return false;
    const Real b = 2 * (fx * dx + fy * dy);
    const Real discriminant = b * b - 4 * a * c;
    if (discriminant < 0) return false;
    const Real root = std::sqrt(discriminant);
    const Real t = (-b - root) / (2 * a);
    if (t < 0 || t > 1) return false;
    out_t = t;
    return true;
}

}  // namespace opra
