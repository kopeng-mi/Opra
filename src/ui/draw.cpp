#include "ui/draw.h"

#include <algorithm>
#include <cmath>

namespace opra::ui {

glm::vec4 with_alpha(const glm::vec4 &color, float alpha) {
    return {color.r, color.g, color.b, alpha};
}

glm::vec4 lerp_color(const glm::vec4 &a, const glm::vec4 &b, float t) {
    return {a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t, a.b + (b.b - a.b) * t,
            a.a + (b.a - a.a) * t};
}

float clamp01(float value) { return value < 0.0f ? 0.0f : value > 1.0f ? 1.0f : value; }

void push_quad(UIBatch &batch, const glm::vec2 &a, const glm::vec2 &b, const glm::vec2 &c,
               const glm::vec2 &d, const glm::vec4 &color) {
    // Solid marks sample the glyph atlas's white block, which lives at its origin: any other uv
    // would land on transparent texels and draw nothing.
    const glm::vec2 uv(0.0f, 0.0f);
    batch.solid.push_back({a, uv, color});
    batch.solid.push_back({b, uv, color});
    batch.solid.push_back({c, uv, color});
    batch.solid.push_back({a, uv, color});
    batch.solid.push_back({c, uv, color});
    batch.solid.push_back({d, uv, color});
}

void push_triangle(UIBatch &batch, const glm::vec2 &a, const glm::vec2 &b, const glm::vec2 &c,
                   const glm::vec4 &color) {
    // Solid marks sample the glyph atlas's white block, which lives at its origin: any other uv
    // would land on transparent texels and draw nothing.
    const glm::vec2 uv(0.0f, 0.0f);
    batch.solid.push_back({a, uv, color});
    batch.solid.push_back({b, uv, color});
    batch.solid.push_back({c, uv, color});
}

void push_rect(UIBatch &batch, const glm::vec2 &origin, const glm::vec2 &size,
               const glm::vec4 &color) {
    push_quad(batch, origin, {origin.x + size.x, origin.y},
              {origin.x + size.x, origin.y + size.y}, {origin.x, origin.y + size.y}, color);
}

void push_line(UIBatch &batch, const glm::vec2 &from, const glm::vec2 &to, float thickness,
               const glm::vec4 &color) {
    const glm::vec2 delta = to - from;
    const float len = std::sqrt(delta.x * delta.x + delta.y * delta.y);
    if (len < 1e-4f) return;
    const glm::vec2 normal(-delta.y / len * thickness * 0.5f, delta.x / len * thickness * 0.5f);
    push_quad(batch, from - normal, to - normal, to + normal, from + normal, color);
}

void push_arc(UIBatch &batch, const glm::vec2 &centre, float radius, float from, float to,
              float thickness, const glm::vec4 &color) {
    const float span = std::abs(to - from);
    const int steps = std::max(6, static_cast<int>(span * radius / 6.0f));
    const float step = (to - from) / static_cast<float>(steps);
    for (int i = 0; i < steps; ++i) {
        const float a0 = from + step * static_cast<float>(i);
        const float a1 = a0 + step;
        const glm::vec2 p0(centre.x + std::cos(a0) * radius, centre.y + std::sin(a0) * radius);
        const glm::vec2 p1(centre.x + std::cos(a1) * radius, centre.y + std::sin(a1) * radius);
        push_line(batch, p0, p1, thickness, color);
    }
}

void push_disc(UIBatch &batch, const glm::vec2 &centre, float radius, const glm::vec4 &color) {
    constexpr int kSides = 12;
    for (int i = 0; i < kSides; ++i) {
        const float a0 = 6.2831853f * static_cast<float>(i) / kSides;
        const float a1 = 6.2831853f * static_cast<float>(i + 1) / kSides;
        push_triangle(batch, centre,
                      {centre.x + std::cos(a0) * radius, centre.y + std::sin(a0) * radius},
                      {centre.x + std::cos(a1) * radius, centre.y + std::sin(a1) * radius}, color);
    }
}

void push_text(UIBatch &batch, const char *text, float px, const glm::vec2 &at, TextAlign align,
               const glm::vec4 &color, TextFace face) {
    if (!text || !*text) return;
    TextDraw draw;
    draw.text = text;
    draw.px = px;
    draw.at = at;
    draw.align = align;
    draw.face = face;
    draw.color = color;
    batch.texts.push_back(std::move(draw));
}

}  // namespace opra::ui
