// The one draw language: the UI vertex, the batch everything draws into, and the mark
// primitives. HUD, menus and screens all write through here, so there is a single look.
#pragma once

#include <string>
#include <vector>

#include <glm/glm.hpp>

namespace opra {

struct UIVertex {
    glm::vec2 pos;
    glm::vec2 uv;
    glm::vec4 color;
};

enum class TextFace { Display, Readout, Label };
enum class TextAlign { Left, Center, Right };

/** One string to draw. `at` is the top-left of the text box before alignment is applied. */
struct TextDraw {
    std::string text;
    float px = 12.0f;
    glm::vec2 at{0.0f, 0.0f};
    TextAlign align = TextAlign::Left;
    TextFace face = TextFace::Readout;
    glm::vec4 color{1.0f, 1.0f, 1.0f, 1.0f};
};

/** Everything built this frame: solid geometry plus the strings. */
struct UIBatch {
    std::vector<UIVertex> solid;
    std::vector<TextDraw> texts;
};

}  // namespace opra

namespace opra::ui {

glm::vec4 with_alpha(const glm::vec4 &color, float alpha);
glm::vec4 lerp_color(const glm::vec4 &a, const glm::vec4 &b, float t);
float clamp01(float value);

void push_quad(UIBatch &batch, const glm::vec2 &a, const glm::vec2 &b, const glm::vec2 &c,
               const glm::vec2 &d, const glm::vec4 &color);
void push_triangle(UIBatch &batch, const glm::vec2 &a, const glm::vec2 &b, const glm::vec2 &c,
                   const glm::vec4 &color);
void push_rect(UIBatch &batch, const glm::vec2 &origin, const glm::vec2 &size,
               const glm::vec4 &color);
void push_line(UIBatch &batch, const glm::vec2 &from, const glm::vec2 &to, float thickness,
               const glm::vec4 &color);
/** Arc in the HUD's y-down angle convention (0 = right, +pi/2 = down). */
void push_arc(UIBatch &batch, const glm::vec2 &centre, float radius, float from, float to,
              float thickness, const glm::vec4 &color);
void push_disc(UIBatch &batch, const glm::vec2 &centre, float radius, const glm::vec4 &color);
void push_text(UIBatch &batch, const char *text, float px, const glm::vec2 &at, TextAlign align,
               const glm::vec4 &color, TextFace face = TextFace::Readout);

/** 8-segment meter bar with 1 px threshold tick (PLAN-08 §9.3). */
void draw_meter(UIBatch &batch, const struct Rect &at, float value, float threshold,
                const glm::vec4 &ink);

}  // namespace opra::ui
