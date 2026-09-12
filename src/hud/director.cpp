#include "hud/director.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

#include "hud/hud.h"
#include "ui/tokens.h"

namespace opra {
namespace {

using ui::Rect;
using ui::tokens::DRIVE;
using ui::tokens::ETCH;
using ui::tokens::ETCH_DIM;
using ui::tokens::NAV;
using ui::tokens::THREAT;

// One row per kind, each tall enough for the tallest text it carries: the layout assertion checks
// that a text anchored in a block fits in it, so the row's height is the block's own measure.
constexpr float HEADER_ROW = 20.0f;
constexpr float GATE_ROW = 20.0f;
constexpr float STEP_ROW = 22.0f;
constexpr float THROTTLE_ROW = 34.0f;
constexpr float DELTA_ROW = 20.0f;
constexpr float BURN_ROW = 18.0f;
constexpr float PAD_TOP = 2.0f;
constexpr float PAD_BOTTOM = 4.0f;

/** The mark column, and the values' own width budget inside the block. */
constexpr float MARK_WIDTH = 10.0f;
constexpr float BAR_WIDTH = 104.0f;
constexpr float BAR_HEIGHT = 3.0f;

/** A gate line's numbers, as `value / limit unit` - or the value alone where there is no limit. */
std::string gate_readout(const DirectorFrame::Gate& gate) {
    const bool to_km = std::string(gate.unit) == "m" &&
                       std::max(std::fabs(static_cast<double>(gate.value)),
                                std::fabs(static_cast<double>(gate.limit))) >= 10000.0;
    const double scale = to_km ? 1e-3 : 1.0;
    const char* unit = to_km ? "km" : gate.unit;
    char buffer[64];
    if (gate.limit != 0.0) {
        std::snprintf(buffer, sizeof buffer, "%.1f / %.1f %s", gate.value * scale,
                      gate.limit * scale, unit);
    } else {
        std::snprintf(buffer, sizeof buffer, "%.1f %s", gate.value * scale, unit);
    }
    return buffer;
}

float clamp01(double value) { return static_cast<float>(std::clamp(value, 0.0, 1.0)); }

enum class RowKind { Header, Gate, Step, Throttle, DeltaV, Burn };

struct Row {
    RowKind kind = RowKind::Header;
    const char* name = "";
    std::string value;
    bool ok = true;
    /** The throttle pair: commanded and actual, drawn as two bars. */
    float commanded = 0.0f;
    float actual = 0.0f;
};

float row_height(RowKind kind) {
    switch (kind) {
        case RowKind::Header: return HEADER_ROW;
        case RowKind::Gate: return GATE_ROW;
        case RowKind::Step: return STEP_ROW;
        case RowKind::Throttle: return THROTTLE_ROW;
        case RowKind::DeltaV: return DELTA_ROW;
        case RowKind::Burn: return BURN_ROW;
    }
    return GATE_ROW;
}

/**
 * The rows the panel draws, in order. `full_gates` is density 3: at level 2 only the terms that are
 * failing are worth a line, which is the same rule the collar's marks follow - a mark where there
 * is something to say.
 */
std::vector<Row> panel_rows(const DirectorFrame& director, bool full_gates) {
    std::vector<Row> rows;
    Row header;
    header.kind = RowKind::Header;
    header.value = director.program;
    if (!director.target.empty()) header.value += "  " + director.target;
    rows.push_back(header);

    for (const DirectorFrame::Gate& gate : director.gates) {
        if (!full_gates && gate.ok) continue;
        Row row;
        row.kind = RowKind::Gate;
        row.name = gate.name;
        row.value = gate_readout(gate);
        row.ok = gate.ok;
        rows.push_back(row);
    }

    Row step;
    step.kind = RowKind::Step;
    step.name = director.step;
    step.ok = director.all_ok();
    rows.push_back(step);

    Row throttle;
    throttle.kind = RowKind::Throttle;
    char pair[32];
    std::snprintf(pair, sizeof pair, "%.2f / %.2f", static_cast<double>(director.throttle),
                  static_cast<double>(director.actual));
    throttle.value = pair;
    throttle.commanded = clamp01(director.throttle);
    throttle.actual = clamp01(director.actual);
    rows.push_back(throttle);

    if (director.dvRemaining > 0.0) {
        Row dv;
        dv.kind = RowKind::DeltaV;
        dv.value = speed_readout(director.dvRemaining);
        rows.push_back(dv);
    }
    if (director.burn) {
        Row burn;
        burn.kind = RowKind::Burn;
        burn.value = director.burnIn > 0.0 ? "T- " + clock_readout(director.burnIn) + "  burn " +
                                                clock_readout(director.burnFor)
                                          : "burn " + clock_readout(director.burnFor);
        rows.push_back(burn);
    }
    return rows;
}

/** The tick, the cross and the caret. Geometry, not glyphs: the fonts carry no check mark, and the
 *  HUD has never drawn a character it could have drawn as a mark. */
void tick_mark(UIBatch& batch, const glm::vec2& at, const glm::vec4& color) {
    ui::push_line(batch, at, at + glm::vec2(3.0f, 4.0f), 1.4f, color);
    ui::push_line(batch, at + glm::vec2(3.0f, 4.0f), at + glm::vec2(8.0f, -3.0f), 1.4f, color);
}

void cross_mark(UIBatch& batch, const glm::vec2& at, const glm::vec4& color) {
    ui::push_line(batch, at, at + glm::vec2(7.0f, 7.0f), 1.4f, color);
    ui::push_line(batch, at + glm::vec2(7.0f, 0.0f), at + glm::vec2(0.0f, 7.0f), 1.4f, color);
}

void caret_mark(UIBatch& batch, const glm::vec2& at, const glm::vec4& color) {
    ui::push_triangle(batch, at, at + glm::vec2(0.0f, 8.0f), at + glm::vec2(6.0f, 4.0f), color);
}

}  // namespace

bool DirectorFrame::all_ok() const {
    for (const Gate& gate : gates) {
        if (!gate.ok) return false;
    }
    return true;
}

float director_panel_height(const DirectorFrame& director, bool full_gates) {
    if (!director.active) return 0.0f;
    float height = PAD_TOP + PAD_BOTTOM;
    for (const Row& row : panel_rows(director, full_gates)) height += row_height(row.kind);
    return height;
}

void build_director_panel(UIBatch& batch, const DirectorFrame& director, const ui::Rect& at,
                          bool full_gates) {
    if (!director.active) return;
    const float right = at.x + at.w;
    float y = at.y + PAD_TOP;
    for (const Row& row : panel_rows(director, full_gates)) {
        switch (row.kind) {
            case RowKind::Header:
                ui::push_text(batch, row.value.c_str(), 13.0f, {at.x, y}, TextAlign::Left, ETCH,
                              TextFace::Label);
                break;
            case RowKind::Gate:
                if (row.ok) {
                    tick_mark(batch, {at.x + 1.0f, y + 5.0f}, NAV);
                } else {
                    cross_mark(batch, {at.x + 2.0f, y + 5.0f}, THREAT);
                }
                ui::push_text(batch, row.name, 13.0f, {at.x + MARK_WIDTH + 2.0f, y}, TextAlign::Left,
                              row.ok ? ETCH_DIM : THREAT, TextFace::Label);
                ui::push_text(batch, row.value.c_str(), 15.0f, {right, y - 2.0f}, TextAlign::Right,
                              row.ok ? ETCH : THREAT);
                break;
            case RowKind::Step:
                caret_mark(batch, {at.x + 1.0f, y + 6.0f}, row.ok ? NAV : THREAT);
                ui::push_text(batch, row.name, 13.0f, {at.x + MARK_WIDTH + 2.0f, y}, TextAlign::Left,
                              row.ok ? NAV : THREAT, TextFace::Label);
                break;
            case RowKind::Throttle: {
                ui::push_text(batch, "throttle", 12.0f, {at.x, y}, TextAlign::Left, ETCH_DIM,
                              TextFace::Label);
                ui::push_text(batch, row.value.c_str(), 15.0f, {right, y - 1.0f}, TextAlign::Right,
                              ETCH);
                const float bar_y = y + 20.0f;
                ui::push_rect(batch, {at.x, bar_y}, {BAR_WIDTH, BAR_HEIGHT},
                              ui::with_alpha(ETCH, 0.12f));
                ui::push_rect(batch, {at.x, bar_y}, {BAR_WIDTH * row.commanded, BAR_HEIGHT}, DRIVE);
                ui::push_rect(batch, {at.x, bar_y + BAR_HEIGHT + 3.0f}, {BAR_WIDTH, BAR_HEIGHT},
                              ui::with_alpha(ETCH, 0.12f));
                ui::push_rect(batch, {at.x, bar_y + BAR_HEIGHT + 3.0f}, {BAR_WIDTH * row.actual, BAR_HEIGHT},
                              NAV);
                break;
            }
            case RowKind::DeltaV:
                ui::push_text(batch, "\u0394v", 12.0f, {at.x, y}, TextAlign::Left, ETCH_DIM,
                              TextFace::Label);
                ui::push_text(batch, row.value.c_str(), 15.0f, {right, y - 1.0f}, TextAlign::Right,
                              ETCH);
                break;
            case RowKind::Burn:
                ui::push_text(batch, row.value.c_str(), 12.0f, {right, y}, TextAlign::Right,
                              DRIVE, TextFace::Label);
                break;
        }
        y += row_height(row.kind);
    }
}

}  // namespace opra
