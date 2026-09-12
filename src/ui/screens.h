// The screens that are not the flight HUD: the sector chart, the manual, and the toast line.
// Everything here takes plain frames, never a World: the UI layer does not know the sim.
#pragma once

#include <string>
#include <vector>

#include <glm/glm.hpp>

#include "ui/draw.h"

namespace opra::ui {

/** One line of plain type under the contract it is about. */
struct Toast {
    std::string text;
    double until = 0.0;
};

/** The chart's inputs, on the chart's own scale. */
struct ChartFrame {
    struct Dot {
        glm::vec2 at;
        float radius;
    };
    struct Mark {
        enum class Kind { Cargo, Station, Relay, Derelict };
        std::string text;
        glm::vec2 at;
        Kind kind = Kind::Cargo;
    };
    std::vector<Dot> rocks;
    std::vector<Dot> fragments;
    std::vector<Dot> ore;
    std::vector<Mark> marks;
    glm::vec2 ship{0.0f};
    glm::vec2 velocity{0.0f};
    float heading = 0.0f;
    glm::vec2 bounds_min{0.0f};  // sector extent, metres
    glm::vec2 bounds_max{0.0f};
};

/** The whole belt, one flat view, on its own scale. */
void build_chart(UIBatch &batch, const ChartFrame &frame, float width, float height);

/** The flight manual: bindings and the one paragraph of doctrine. */
void build_help(UIBatch &batch, float width, float height);

/** Draws the live toasts, oldest first, under the contract line. */
void draw_toasts(UIBatch &batch, const std::vector<Toast> &toasts, double now, float x, float y);

}  // namespace opra::ui
