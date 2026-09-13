#include "hud/blocks.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>

#include <SDL3/SDL.h>

namespace opra {
namespace {

using ui::Rect;

Rect inset_rect(const Rect &outer, const Rect &inner) {
    // Positive means `inner` is inside `outer` everywhere.
    return {inner.x - outer.x, inner.y - outer.y,
            (outer.x + outer.w) - (inner.x + inner.w), (outer.y + outer.h) - (inner.y + inner.h)};
}

bool overlaps(const Rect &a, const Rect &b) {
    return a.x < b.x + b.w && b.x < a.x + a.w && a.y < b.y + b.h && b.y < a.y + a.h;
}

/** The width a string occupies, near enough to catch a row of text escaping its block. The real
 *  advance comes from the font; this is deliberately a little generous so the pass cannot miss a
 *  spill by a pixel and is not so tight that it invents one. */
float text_width(const TextDraw &text) {
    return static_cast<float>(text.text.size()) * text.px * 0.58f;
}

/** The box a text draws into, honouring its alignment. */
Rect text_box(const TextDraw &text) {
    const float width = text_width(text);
    const float height = text.px * 1.3f;
    float x = text.at.x;
    if (text.align == TextAlign::Center) x -= width * 0.5f;
    if (text.align == TextAlign::Right) x -= width;
    return {x, text.at.y, width, height};
}

}  // namespace

Density next_density(Density density) {
    switch (density) {
        case Density::One: return Density::Two;
        case Density::Two: return Density::Three;
        case Density::Three: break;
    }
    return Density::One;
}

bool density_at_least(Density have, Density need) {
    return static_cast<int>(have) >= static_cast<int>(need);
}

const char *density_name(Density density) {
    switch (density) {
        case Density::One: return "density 1";
        case Density::Two: return "density 2";
        case Density::Three: return "density 3";
    }
    return "density 1";
}

BlockLayouter::BlockLayouter(const Rect &column, bool from_top)
    : column_(column), from_top_(from_top), cursor_(from_top ? column.y : column.y + column.h) {}

Rect BlockLayouter::place(const char *name, float height) {
    Rect at;
    at.x = column_.x;
    at.w = column_.w;
    at.h = height;
    if (from_top_) {
        at.y = cursor_;
        cursor_ += height;
    } else {
        at.y = cursor_ - height;
        cursor_ -= height;
    }
    blocks_.push_back({name, at});
    return at;
}

void draw_scrim(UIBatch &batch, const ui::Rect &at) {
    // Plan 06 §3.4: FIELD at α 0.72 plus 12 px ring at α 0.36 so the edge feathers
    ui::push_rect(batch, {at.x - 20.0f, at.y - 20.0f}, {at.w + 40.0f, at.h + 40.0f},
                  ui::with_alpha(ui::tokens::FIELD, 0.36f));
    ui::push_rect(batch, {at.x - 8.0f, at.y - 8.0f}, {at.w + 16.0f, at.h + 16.0f},
                  ui::with_alpha(ui::tokens::FIELD, 0.72f));
}

int check_block_layout(const std::vector<BlockRect> &blocks, const UIBatch &batch,
                       const Rect &safe, bool log) {
    int violations = 0;
    // New rule (plan 06 §3.6): no block's rect may intersect the arc's reserved span.
    const float A = std::clamp(safe.w * 0.30f, 300.0f, 520.0f);
    const float y_apex = (safe.y + safe.h) - 96.0f;
    const Rect arc_reserved{safe.x + safe.w * 0.5f - A - 56.0f, y_apex - 24.0f,
                            (A + 56.0f) * 2.0f, 96.0f + 24.0f};
    for (size_t i = 0; i < blocks.size(); ++i) {
        if (overlaps(blocks[i].at, arc_reserved)) {
            ++violations;
            if (log) {
                SDL_Log("[hud] block '%s' intersects arc reserved span", blocks[i].name);
            }
        }
    }
    for (size_t i = 0; i < blocks.size(); ++i) {
        const BlockRect &block = blocks[i];
        const Rect slack = inset_rect(safe, block.at);
        if (slack.x < -0.5f || slack.y < -0.5f || slack.w < -0.5f || slack.h < -0.5f) {
            ++violations;
            if (log) {
                SDL_Log("[hud] block '%s' leaves the safe area by %.1f px (at %.0f,%.0f %.0fx%.0f)",
                        block.name, -std::min({slack.x, slack.y, slack.w, slack.h}), block.at.x,
                        block.at.y, block.at.w, block.at.h);
            }
        }
        for (size_t j = i + 1; j < blocks.size(); ++j) {
            if (!overlaps(block.at, blocks[j].at)) continue;
            ++violations;
            if (log) {
                SDL_Log("[hud] block '%s' overlaps block '%s' (%.0f,%.0f %.0fx%.0f vs %.0f,%.0f "
                        "%.0fx%.0f)",
                        block.name, blocks[j].name, block.at.x, block.at.y, block.at.w, block.at.h,
                        blocks[j].at.x, blocks[j].at.y, blocks[j].at.w, blocks[j].at.h);
            }
        }
        // A text anchored inside this block belongs to it, so it has to fit: this is the rule that
        // would have caught R-1's "heat" label drawing through the ship name's row. The test is
        // strict on the boundary: two stacked blocks share an edge, and the anchor that sits on it
        // belongs to the block that opens there, not to the one that closed.
        for (const TextDraw &text : batch.texts) {
            const bool inside = text.at.x >= block.at.x && text.at.x <= block.at.x + block.at.w &&
                                text.at.y > block.at.y && text.at.y < block.at.y + block.at.h;
            if (!inside) continue;
            const Rect box = text_box(text);
            const Rect over = inset_rect(block.at, box);
            if (over.x >= -0.5f && over.y >= -0.5f && over.w >= -0.5f && over.h >= -0.5f) continue;
            ++violations;
            if (log) {
                SDL_Log("[hud] text '%s' (%.0f px) escapes block '%s' by %.1f px", text.text.c_str(),
                        text.px, block.name, -std::min({over.x, over.y, over.w, over.h}));
            }
        }
    }
    return violations;
}

std::string distance_readout(double metres) {
    char buffer[48];
    const double magnitude = std::fabs(metres);
    if (magnitude < 1000.0) {
        std::snprintf(buffer, sizeof buffer, "%.0f m", metres);
    } else if (magnitude < 1.0e6) {
        std::snprintf(buffer, sizeof buffer, "%.1f km", metres / 1.0e3);
    } else if (magnitude < 1.0e9) {
        std::snprintf(buffer, sizeof buffer, "%.2f Mm", metres / 1.0e6);
    } else {
        std::snprintf(buffer, sizeof buffer, "%.2f Gm", metres / 1.0e9);
    }
    return buffer;
}

std::string clock_readout(double seconds) {
    char buffer[48];
    const double magnitude = std::fabs(seconds);
    if (magnitude >= 86400.0) {
        std::snprintf(buffer, sizeof buffer, "%.0f d %.0f h", std::floor(magnitude / 86400.0),
                      std::floor(std::fmod(magnitude, 86400.0) / 3600.0));
    } else if (magnitude >= 3600.0) {
        std::snprintf(buffer, sizeof buffer, "%.0f h %02.0f m", std::floor(magnitude / 3600.0),
                      std::floor(std::fmod(magnitude, 3600.0) / 60.0));
    } else if (magnitude >= 60.0) {
        std::snprintf(buffer, sizeof buffer, "%.0f m %02.0f s", std::floor(magnitude / 60.0),
                      std::fmod(magnitude, 60.0));
    } else {
        std::snprintf(buffer, sizeof buffer, "%.1f s", magnitude);
    }
    return buffer;
}

std::string speed_readout(double metres_per_second) {
    char buffer[32];
    if (std::fabs(metres_per_second) >= 1000.0) {
        std::snprintf(buffer, sizeof buffer, "%.2f km/s", metres_per_second / 1.0e3);
    } else {
        std::snprintf(buffer, sizeof buffer, "%.1f m/s", metres_per_second);
    }
    return buffer;
}

namespace {

constexpr float ORBIT_ROW = 20.0f;
constexpr float ORBIT_PAD = 6.0f;
constexpr int ORBIT_FIGURES = 6;  // alt, peri, apo, ecc, period, dv - the plan's own block

/** A three-column figure row: the label, the number, and the countdown the mock prints beside it. */
struct OrbitLine {
    const char *label;
    std::string value;
    std::string countdown;
};

}  // namespace

bool orbit_has_plan_dv(const HudFrame &frame) {
    if (!frame.orbit.valid) return false;
    // Plan 06 §3.5: live when differing from node dv by > 0.1 m/s
    return !frame.nodeValid || std::fabs(frame.orbit.dvPlanned - frame.nodeDeltaV) > 0.1;
}

float orbit_block_height(const HudFrame &frame) {
    if (!frame.orbit.valid) return 0.0f;
    const int count = orbit_has_plan_dv(frame) ? 6 : 5;
    return ORBIT_ROW * (count + 1) + ORBIT_PAD;  // the header plus the active figures
}

void build_orbit_block(UIBatch &batch, const HudFrame &frame, const ui::Rect &at) {
    const OrbitFrame &orbit = frame.orbit;
    if (!orbit.valid) return;
    draw_scrim(batch, at);
    const std::string escape = "escape";
    char eccentricity[32];
    std::snprintf(eccentricity, sizeof eccentricity, "%.4f", orbit.eccentricity);
    std::vector<OrbitLine> lines = {
        {"alt", distance_readout(orbit.altitude), ""},
        {"peri", distance_readout(orbit.periapsis),
         orbit.elliptic ? "T- " + clock_readout(orbit.toPeriapsis) : ""},
        {"apo", orbit.elliptic ? distance_readout(orbit.apoapsis) : escape,
         orbit.elliptic ? "T- " + clock_readout(orbit.toApoapsis) : ""},
        {"ecc", eccentricity, ""},
        {"period", orbit.elliptic ? clock_readout(orbit.period) : escape, ""},
    };
    if (orbit_has_plan_dv(frame)) {
        lines.push_back({"plan dv", speed_readout(orbit.dvPlanned), ""});
    }
    ui::push_text(batch, "orbit", 13.0f, {at.x, at.y}, TextAlign::Left, ui::tokens::ETCH_DIM,
                  TextFace::Label);
    ui::push_text(batch, orbit.body, 13.0f, {at.x + 46.0f, at.y}, TextAlign::Left,
                  ui::tokens::ETCH, TextFace::Label);
    // The figures right-align short of the block's edge so the countdown keeps its own column.
    const float right = at.x + at.w;
    const float figure_right = at.x + at.w * 0.62f;
    for (size_t i = 0; i < lines.size(); ++i) {
        const float y = at.y + ORBIT_ROW * static_cast<float>(i + 1);
        ui::push_text(batch, lines[i].label, 12.0f, {at.x + 10.0f, y + 2.0f}, TextAlign::Left,
                      ui::tokens::ETCH_DIM, TextFace::Label);
        ui::push_text(batch, lines[i].value.c_str(), 15.0f, {figure_right, y}, TextAlign::Right,
                      ui::tokens::ETCH);
        if (!lines[i].countdown.empty()) {
            ui::push_text(batch, lines[i].countdown.c_str(), 11.0f, {right, y + 4.0f},
                          TextAlign::Right, ui::tokens::ETCH_DIM, TextFace::Label);
        }
    }
}

bool program_full_gates(const HudFrame &frame) {
    return density_at_least(frame.density, Density::Three);
}

float program_block_height(const HudFrame &frame) {
    return director_panel_height(frame.director, program_full_gates(frame));
}

void build_program_block(UIBatch &batch, const HudFrame &frame, const ui::Rect &at) {
    build_director_panel(batch, frame.director, at, program_full_gates(frame));
}

}  // namespace opra
