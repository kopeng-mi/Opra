#include "hud/hud.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include "hud/blocks.h"
#include "ui/draw.h"

namespace opra {

using ui::Rect;

namespace {

constexpr float TAU = 6.28318530718f;
constexpr float DEG = 0.01745329252f;

using ui::tokens::DRIVE;
using ui::tokens::ETCH;
using ui::tokens::ETCH_DIM;
using ui::tokens::NAV;
using ui::tokens::THREAT;

// The one draw language, in scope for every block: a block that had to spell out `ui::push_text`
// would be the only place in the file that does.
using ui::push_arc;
using ui::push_disc;
using ui::push_line;
using ui::push_quad;
using ui::push_rect;
using ui::push_text;
using ui::push_triangle;
using ui::with_alpha;

// Collar geometry, from collar.ts.
constexpr float TOP = -1.5707963f;
constexpr float BOTTOM = 1.5707963f;
constexpr float RING_WIDTH = 1.0f;
constexpr float RING_ALPHA = 0.4f;
constexpr float GAP_HALF = 20.0f * DEG;
constexpr float ARC_HALF = 70.0f * DEG;
constexpr float ARC_WIDTH = 3.0f;
constexpr float ARC_ALPHA = 0.3f;
constexpr float THRUST_MAX = 1.65f;
constexpr float RETRO_MAX = 0.28f;
constexpr float OVERHEAT = 0.92f;
constexpr float PULSE_RATE = TAU / 1.6f;
constexpr float PULSE_FLOOR = 0.55f;

constexpr float LADDER_HALF = 60.0f * DEG;
constexpr float LADDER_STEP = 10.0f * DEG;
constexpr int LADDER_MAJOR_EVERY = 3;
constexpr float LADDER_MINOR_LEN = 3.0f;
constexpr float LADDER_MAJOR_LEN = 6.0f;
constexpr float LADDER_ALPHA = 0.5f;
constexpr float LADDER_LABEL_RADIUS = 10.0f;

constexpr float VELOCITY_LENGTH = 12.0f;
constexpr float VELOCITY_BASE = 3.4f;
constexpr float TARGET_OUT = 3.5f;
constexpr float TARGET_ARM = 4.5f;
constexpr float TARGET_WIDTH = 1.6f;
constexpr float HOSTILE_MIN = 6.0f;
constexpr float HOSTILE_SPAN = 14.0f;
constexpr float HOSTILE_WIDTH = 1.6f;
constexpr float CONTACT_RADIUS = 2.0f;
constexpr float ORE_RADIUS = 1.3f;

constexpr float FAR_RANGE = 2000.0f;
constexpr float FAR_ALPHA = 0.35f;
constexpr float LABEL_RADIUS = 15.0f;

constexpr float RANGE_RING_METRES[2] = {250.0f, 500.0f};
constexpr float RANGE_RING_ALPHA = 0.22f;
constexpr float RANGE_RING_LABEL_ALPHA = 0.45f;
constexpr float RANGE_RING_LABEL_ANGLE = 45.0f * DEG;
constexpr float RANGE_RING_LABEL_GAP = 3.0f;

std::string readout(float value, int decimals, const char *suffix) {
    char buffer[64];
    std::snprintf(buffer, sizeof buffer, "%.*f%s", decimals, static_cast<double>(value), suffix);
    return buffer;
}

std::string clock_string(float seconds) {
    const int total = static_cast<int>(std::max(0.0f, seconds));
    char buffer[32];
    std::snprintf(buffer, sizeof buffer, "T+ %02d:%02d", total / 60, total % 60);
    return buffer;
}

glm::vec4 weight_color(Weight weight) {
    switch (weight) {
        case Weight::Live: return ETCH;
        case Weight::Critical: return THREAT;
        case Weight::Dormant: default: return ETCH_DIM;
    }
}

float weight_opacity(Weight weight) { return weight == Weight::Dormant ? 0.55f : 1.0f; }

Weight bar_weight(float fraction, bool inverted) {
    const bool critical = inverted ? fraction > 0.85f : fraction < 0.30f;
    if (critical) return Weight::Critical;
    const bool live = inverted ? fraction > 0.55f : fraction < 0.60f;
    return live ? Weight::Live : Weight::Dormant;
}

glm::vec4 bar_color(Weight weight) {
    switch (weight) {
        case Weight::Critical: return THREAT;
        case Weight::Live: return DRIVE;
        case Weight::Dormant: default: return NAV;
    }
}

glm::vec4 status_color(StatusDot status) {
    switch (status) {
        case StatusDot::Paused: return DRIVE;
        case StatusDot::UnderFire: return THREAT;
        case StatusDot::Nominal: default: return NAV;
    }
}

}  // namespace

bool bar_value_visible(float fraction, bool inverted) {
    return inverted ? fraction > 0.55f : fraction < 0.60f;
}

namespace {
// The corner blocks. Each one draws inside the rect the layouter gave it: the block does not know
// where it is on screen and does not care, which is what stops a cluster from placing itself at a
// y value someone worked out by hand (R-1).
void draw_mark(UIBatch &batch, const HudFrame &frame, const Rect &at) {
    using ui::push_rect;
    using ui::push_text;
    using ui::with_alpha;
    push_text(batch, "OPRA", 22.0f, {at.x, at.y}, TextAlign::Left, ETCH);
    if (frame.contractId && *frame.contractId) {
        push_text(batch, frame.contractId, 13.0f, {at.x, at.y + 40.0f}, TextAlign::Left, ETCH_DIM,
                  TextFace::Label);
    }
    if (frame.objective && *frame.objective) {
        push_text(batch, frame.objective, 15.0f, {at.x + 62.0f, at.y + 38.0f}, TextAlign::Left,
                  ETCH, TextFace::Label);
    }
    // The progress bar: dormant track, then the filled part over it.
    push_rect(batch, {at.x, at.y + 66.0f}, {232.0f, 1.0f}, with_alpha(ETCH, 0.15f));
    push_rect(batch, {at.x, at.y + 66.0f}, {232.0f * ui::clamp01(frame.progress), 1.0f}, NAV);
}

void draw_session(UIBatch &batch, const HudFrame &frame, const Rect &at) {
    const float right = at.x + at.w;
    // The status dot trails the clock, so the clock stops six pixels short of the inset edge.
    push_text(batch, clock_string(frame.sessionSeconds).c_str(), 13.0f, {right - 6.0f, at.y},
              TextAlign::Right, ETCH);
    push_disc(batch, {right - 2.0f, at.y + 6.0f}, 2.0f, status_color(frame.status));
    if (frame.chartOpen) {
        push_text(batch, "sector chart  M", 12.0f, {right, at.y + 20.0f}, TextAlign::Right, NAV,
                  TextFace::Label);
    }
}

void draw_bars(UIBatch &batch, const HudFrame &frame, const Rect &at) {
    struct BarRow {
        const char *label;
        float fraction;
        float value;
        bool inverted;
    };
    const BarRow rows[3] = {
        {"hull", frame.hullFrac, frame.hullValue, false},
        {"prop", frame.fuelFrac, frame.fuelValue, false},
        {"heat", frame.heatFrac, frame.heatValue, true},
    };
    // R-1: the first row's value readout starts at the block's own top edge and the layouter placed
    // the block above the vessel line, so "heat" can no longer draw through the ship's name. The
    // cluster's old y values (778/812/846 with the name at 852) were the whole of the bug.
    for (int i = 0; i < 3; ++i) {
        const BarRow &row = rows[i];
        const float y = at.y + 8.0f + static_cast<float>(i) * 34.0f;
        const Weight weight = bar_weight(row.fraction, row.inverted);
        const float alpha = weight_opacity(weight);
        push_text(batch, row.label, 11.0f, {at.x, y}, TextAlign::Left, with_alpha(ETCH_DIM, alpha),
                  TextFace::Label);
        push_rect(batch, {at.x + 42.0f, y + 8.0f}, {80.0f, 3.0f}, with_alpha(ETCH, 0.12f));
        push_rect(batch, {at.x + 42.0f, y + 8.0f}, {80.0f * ui::clamp01(row.fraction), 3.0f},
                  with_alpha(bar_color(weight), alpha));
        if (bar_value_visible(row.fraction, row.inverted)) {
            push_text(batch, readout(row.value, 0, "").c_str(), 25.0f, {at.x + 132.0f, y - 8.0f},
                      TextAlign::Left, weight_color(weight));
        }
    }
}

void draw_vessel(UIBatch &batch, const HudFrame &frame, const Rect &at) {
    push_text(batch, frame.shipName, 13.0f, {at.x, at.y}, TextAlign::Left, ETCH, TextFace::Label);
    push_text(batch, frame.assist ? "flight assist  F" : "assist off  F", 11.0f,
              {at.x + 120.0f, at.y + 2.0f}, TextAlign::Left, frame.assist ? NAV : ETCH_DIM,
              TextFace::Label);
    push_text(batch, "kill velocity  X", 11.0f, {at.x + 240.0f, at.y + 2.0f}, TextAlign::Left,
              frame.braking ? DRIVE : ETCH_DIM, TextFace::Label);
}

void draw_guns(UIBatch &batch, const HudFrame &frame, const Rect &at) {
    const float right = at.x + at.w;
    for (int i = 0; i < frame.gunCount; ++i) {
        const float x = right - 12.0f - static_cast<float>(frame.gunCount - 1 - i) * 26.0f;
        const float y = at.y + 6.0f;
        const bool ready = i < frame.gunsReady;
        const glm::vec4 color = frame.gunsHot ? THREAT : (ready ? NAV : ETCH_DIM);
        if (ready) {
            push_quad(batch, {x, y - 4.0f}, {x + 4.0f, y}, {x, y + 4.0f}, {x - 4.0f, y},
                      with_alpha(color, 1.0f));
        } else {
            push_line(batch, {x - 4.0f, y - 4.0f}, {x + 4.0f, y - 4.0f}, 1.0f,
                      with_alpha(color, 0.7f));
            push_line(batch, {x + 4.0f, y - 4.0f}, {x + 4.0f, y + 4.0f}, 1.0f,
                      with_alpha(color, 0.7f));
            push_line(batch, {x + 4.0f, y + 4.0f}, {x - 4.0f, y + 4.0f}, 1.0f,
                      with_alpha(color, 0.7f));
            push_line(batch, {x - 4.0f, y + 4.0f}, {x - 4.0f, y - 4.0f}, 1.0f,
                      with_alpha(color, 0.7f));
        }
    }
    push_text(batch, "AC-20  cutter", 11.0f, {right, at.y + 18.0f}, TextAlign::Right, ETCH_DIM,
              TextFace::Label);
    if (frame.oreHeld > 0.0) {
        push_text(batch, readout(static_cast<float>(frame.oreHeld), 0, " ore").c_str(), 12.0f,
                  {right, at.y + 34.0f}, TextAlign::Right, NAV, TextFace::Label);
    }
}

/**
 * The world under the ship (plan 3.2-3.7): altitude, air, the rate of descent, the base on the pad
 * it is standing on, and what the survey and the satellites have to say. A vacuum above a body with
 * no air still gets the altitude row, because that is the number a descent is flown on.
 */
void draw_worlds(UIBatch &batch, const HudFrame &frame, const Rect &at) {
    float y = at.y;
    const auto row = [&](const char *label, const std::string &value, const glm::vec4 &color) {
        push_text(batch, label, 11.0f, {at.x, y}, TextAlign::Left, ETCH_DIM, TextFace::Label);
        push_text(batch, value.c_str(), 17.0f, {at.x + 74.0f, y - 4.0f}, TextAlign::Left, color);
        y += 22.0f;
    };

    char buffer[64];
    std::snprintf(buffer, sizeof buffer, "%.1f km", frame.altitude / 1000.0);
    row("alt", buffer, ETCH);
    if (frame.airDensity > 0.0) {
        std::snprintf(buffer, sizeof buffer, "%.2e", frame.airDensity);
        row("rho", buffer, ETCH_DIM);
        // The flux in the unit a pilot reads: a megawatt per square metre is a hot entry.
        std::snprintf(buffer, sizeof buffer, "%.2f MW/m2", frame.airFlux / 1.0e6);
        row("q", buffer, frame.airFlux > HEAT_FLUX_FULL * 0.5 ? THREAT : ETCH_DIM);
        std::snprintf(buffer, sizeof buffer, "%.0f m/s", frame.descentRate);
        row("descent", buffer, frame.descentRate > 4.0 ? DRIVE : ETCH_DIM);
    }
    if (frame.landed) {
        row("landed", frame.baseName && *frame.baseName ? frame.baseName : "open ground",
            frame.baseName && *frame.baseName ? NAV : ETCH);
    }
    if (frame.surveyFraction > 0.0) {
        std::snprintf(buffer, sizeof buffer, "%.0f%%", frame.surveyFraction * 100.0);
        row("survey", buffer, ETCH_DIM);
    }
    if (frame.satellitesUp + frame.satellitesPending > 0) {
        std::snprintf(buffer, sizeof buffer, "%d up  %d due", frame.satellitesUp,
                      frame.satellitesPending);
        row("sats", buffer, ETCH_DIM);
    }
}

void draw_motion(UIBatch &batch, const HudFrame &frame, const Rect &at) {
    const float right = at.x + at.w;
    push_text(batch, readout(static_cast<float>(frame.accelerationG), 2, " g").c_str(), 25.0f,
              {right, at.y}, TextAlign::Right, ETCH);
    char heading[16];
    std::snprintf(heading, sizeof heading, "%03d\u00b0",
                  static_cast<int>(std::lround(frame.headingDeg)) % 360);
    push_text(batch, heading, 25.0f, {right, at.y + 32.0f}, TextAlign::Right, ETCH);
}

void draw_systems(UIBatch &batch, const HudFrame &frame, const Rect &at) {
    // Density 3: the numbers a pilot checks when something is wrong rather than when something is
    // happening. The plan's per-thruster and mass-flow rows land here as the sim grows them.
    const float right = at.x + at.w;
    char line[64];
    std::snprintf(line, sizeof line, "zoom %.2f", static_cast<double>(frame.zoom));
    push_text(batch, line, 11.0f, {right, at.y}, TextAlign::Right, ETCH_DIM, TextFace::Label);
    std::snprintf(line, sizeof line, "thrust %+.2f", static_cast<double>(frame.thrust));
    push_text(batch, line, 11.0f, {right, at.y + 14.0f}, TextAlign::Right, ETCH_DIM,
              TextFace::Label);
    std::snprintf(line, sizeof line, "warp %gx", frame.warpRate);
    push_text(batch, line, 11.0f, {right, at.y + 28.0f}, TextAlign::Right, ETCH_DIM,
              TextFace::Label);
}

}  // namespace

void build_flight_hud(UIBatch &batch, const HudFrame &frame) {
    const float width = frame.screen.x;
    const float height = frame.screen.y;
    const glm::vec2 centre = frame.shipScreen;

    // D-18: one safe-area inset for the whole layout - 28 px, or 3% of the short edge, whichever
    // is larger. Every block hangs inside it; no block carries a margin of its own.
    const float inset = std::max(28.0f, std::min(width, height) * 0.03f);
    const Rect safe{inset, inset, width - inset * 2.0f, height - inset * 2.0f};
    // The columns are fractions of the safe area, so a narrow window has the same four stacks with
    // less room rather than two stacks drawn through each other.
    const float left_column = std::min(440.0f, safe.w * 0.5f);
    const float right_column = std::min(300.0f, safe.w * 0.42f);

    float radius = frame.radius > 0.0f ? frame.radius
                                       : std::clamp(std::min(width, height) * 0.29f, 150.0f, 260.0f);
    radius = std::min(radius, std::min(width, height) * 0.5f - LABEL_RADIUS - 4.0f);

    // The collar and the centre marks are not blocks: a bearing ring has to sit on the ship, and
    // the ship is wherever the follow put it. They are drawn from `centre` and are exempt from the
    // layout pass by omission (hud/blocks.h says why).
    if (!frame.hideCollar && radius >= 12.0f) {
        // The ring: a full circle less the deliberate 40 degree gap at the top.
        push_arc(batch, centre, radius, TOP + GAP_HALF, TOP - GAP_HALF + TAU, RING_WIDTH,
                 with_alpha(ETCH_DIM, RING_ALPHA));

        // Range rings: the collar's only scale, FAR_RANGE landing exactly on the ring.
        for (float metres : RANGE_RING_METRES) {
            const float r = radius * (metres / FAR_RANGE);
            push_arc(batch, centre, r, 0.0f, TAU, 1.0f, with_alpha(ETCH_DIM, RANGE_RING_ALPHA));
            const std::string label = readout(metres, 0, " m");
            const glm::vec2 at(
                centre.x + std::cos(RANGE_RING_LABEL_ANGLE) * (r + RANGE_RING_LABEL_GAP),
                centre.y + std::sin(RANGE_RING_LABEL_ANGLE) * (r + RANGE_RING_LABEL_GAP) - 6.0f);
            push_text(batch, label.c_str(), 12.0f, at, TextAlign::Left,
                      with_alpha(ETCH_DIM, RANGE_RING_LABEL_ALPHA));
        }

        // The bearing ladder: a tick every 10 degrees, a numbered major every 30.
        const int ladder_steps = static_cast<int>((LADDER_HALF * 2.0f) / LADDER_STEP + 0.5f);
        for (int k = 0; k <= ladder_steps; ++k) {
            const float angle = TOP - LADDER_HALF + static_cast<float>(k) * LADDER_STEP;
            if (angle > TOP - GAP_HALF && angle < TOP + GAP_HALF) continue;
            const bool major = (k % LADDER_MAJOR_EVERY) == 0;
            const float len = major ? LADDER_MAJOR_LEN : LADDER_MINOR_LEN;
            const glm::vec2 dir(std::cos(angle), std::sin(angle));
            push_line(batch, centre + dir * radius, centre + dir * (radius + len), RING_WIDTH,
                      with_alpha(ETCH_DIM, major ? LADDER_ALPHA : LADDER_ALPHA * 0.6f));
            if (!major) continue;
            const int bearing = ((static_cast<int>(std::lround(-angle / DEG)) % 360) + 360) % 360;
            char label[8];
            std::snprintf(label, sizeof label, "%03d", bearing);
            push_text(batch, label, 12.0f,
                      centre + dir * (radius + LADDER_LABEL_RADIUS) - glm::vec2(9.0f, 6.0f),
                      TextAlign::Left, with_alpha(ETCH_DIM, LADDER_ALPHA));
        }

        // The drive arc: dormant scale, thrust clockwise, retro counter-clockwise.
        const float heat = ui::clamp01(frame.heat);
        const float forward = ui::clamp01(frame.thrust / THRUST_MAX);
        const float retro = ui::clamp01(-frame.thrust / RETRO_MAX);
        push_arc(batch, centre, radius, BOTTOM - ARC_HALF, BOTTOM + ARC_HALF, ARC_WIDTH,
                 with_alpha(ETCH_DIM, ARC_ALPHA));
        if (forward > 0.0f || retro > 0.0f) {
            float alpha = 1.0f;
            if (heat > OVERHEAT && !frame.reducedMotion) {
                alpha = PULSE_FLOOR + (1.0f - PULSE_FLOOR) *
                                          (0.5f + 0.5f * std::sin(frame.time * PULSE_RATE));
            }
            const glm::vec4 arc_color = with_alpha(ui::lerp_color(DRIVE, THREAT, heat), alpha);
            if (forward > 0.0f) {
                push_arc(batch, centre, radius, BOTTOM, BOTTOM + forward * ARC_HALF, ARC_WIDTH,
                         arc_color);
            }
            if (retro > 0.0f) {
                push_arc(batch, centre, radius, BOTTOM, BOTTOM - retro * ARC_HALF, ARC_WIDTH,
                         arc_color);
            }
        }

        // The marks.
        for (const CollarMark &mark : frame.marks) {
            // World is +x right / +y up, the HUD is y-down: a bearing b is HUD angle -b.
            const glm::vec2 u(std::cos(-mark.bearing), std::sin(-mark.bearing));
            const float strength = ui::clamp01(mark.strength);
            const float far = mark.range > FAR_RANGE ? FAR_ALPHA : 1.0f;
            switch (mark.kind) {
                case MarkKind::Velocity: {
                    const glm::vec2 t(-u.y, u.x);
                    push_triangle(batch, centre + u * (radius + VELOCITY_LENGTH),
                                  centre + u * radius + t * VELOCITY_BASE,
                                  centre + u * radius - t * VELOCITY_BASE,
                                  with_alpha(ETCH, (0.55f + 0.45f * strength) * far));
                    break;
                }
                case MarkKind::Target: {
                    const glm::vec2 t(-u.y, u.x);
                    const glm::vec2 tip = centre + u * (radius + TARGET_OUT);
                    const glm::vec2 base = centre + u * (radius - TARGET_OUT);
                    const glm::vec4 color = with_alpha(NAV, 0.7f + 0.3f * strength);
                    push_line(batch, base + t * TARGET_ARM, tip, TARGET_WIDTH, color);
                    push_line(batch, tip, base - t * TARGET_ARM, TARGET_WIDTH, color);
                    const std::string range = readout(mark.range, 0, " m");
                    push_text(batch, range.c_str(), 25.0f,
                              centre + u * (radius + LABEL_RADIUS) - glm::vec2(0.0f, 14.0f),
                              TextAlign::Center, with_alpha(NAV, far));
                    break;
                }
                case MarkKind::Hostile: {
                    const float len = HOSTILE_MIN + HOSTILE_SPAN * strength;
                    push_line(batch, centre + u * radius, centre + u * (radius + len), HOSTILE_WIDTH,
                              with_alpha(THREAT, (0.45f + 0.55f * strength) * far));
                    break;
                }
                case MarkKind::Contact:
                    push_disc(batch, centre + u * radius, CONTACT_RADIUS,
                              with_alpha(NAV, (0.4f + 0.55f * strength) * far));
                    break;
                case MarkKind::Ore:
                    push_disc(batch, centre + u * radius, ORE_RADIUS, with_alpha(NAV, 0.4f * far));
                    break;
            }
        }
    }

    // The director's cross (G8): where the cue wants the nose, drawn at the cue's bearing on the
    // collar's own ring, in the frame every mark uses. It is placed by the collar's geometry and is
    // not part of it.
    if (!frame.hideCollar && radius >= 12.0f && frame.director.active) {
        constexpr float CROSS_OUT = 10.0f;
        constexpr float CROSS_ARM = 6.0f;
        const float bearing = static_cast<float>(frame.director.heading);
        const glm::vec2 u(std::cos(-bearing), std::sin(-bearing));
        const glm::vec2 at = centre + u * (radius + CROSS_OUT);
        const glm::vec2 across(-u.y, u.x);
        const glm::vec4 cross_color = frame.director.throttle > 0.0f ? DRIVE : ETCH;
        push_line(batch, at - across * CROSS_ARM, at + across * CROSS_ARM, 1.5f,
                  with_alpha(cross_color, 0.9f));
        push_line(batch, at - u * CROSS_ARM, at + u * CROSS_ARM, 1.5f,
                  with_alpha(cross_color, 0.9f));
        push_arc(batch, at, CROSS_ARM + 2.0f, 0.0f, TAU, 1.0f, with_alpha(cross_color, 0.55f));
    }

    // ---- the four corners, each one a stack of blocks
    BlockLayouter top_left({safe.x, safe.y, left_column, safe.h}, true);
    BlockLayouter top_right({safe.x + safe.w - right_column, safe.y, right_column, safe.h}, true);
    BlockLayouter bottom_left({safe.x, safe.y, left_column, safe.h}, false);
    BlockLayouter bottom_right({safe.x + safe.w - right_column, safe.y, right_column, safe.h},
                               false);

    const Density density = frame.density;
    draw_mark(batch, frame, top_left.place("mark", 72.0f));
    draw_session(batch, frame, top_right.place("session", frame.chartOpen ? 40.0f : 26.0f));
    // F5: the minimap is one block in one corner at one footprint, its contents set by the mode.
    if (frame.minimap.active && density_at_least(density, Density::Two)) {
        build_minimap(batch, frame.minimap,
                      top_right.place("minimap", minimap_block_height(frame.minimap)));
    }
    draw_motion(batch, frame, bottom_right.place("motion", 70.0f));
    // Level 2 adds the guns and the ship's own line ("Kestrel  flight assist  kill velocity").
    // Mining forces the guns on at any level: the ore count is the readout of the job in hand.
    if (density_at_least(density, Density::Two) || frame.forceGuns) {
        draw_guns(batch, frame, bottom_right.place("guns", 56.0f));
    }
    // The director's panel (G8) and the orbit block are level 2, and either can be forced back on
    // by the context: a failing gate is the readout, and an SOI change is the moment the conic
    // changes under the ship (plan 4.6, F10).
    if (frame.director.active && (density_at_least(density, Density::Two) || frame.forceProgram)) {
        build_program_block(batch, frame, bottom_right.place("program", program_block_height(frame)));
    }
    if (frame.orbit.valid && (density_at_least(density, Density::Two) || frame.forceOrbit)) {
        build_orbit_block(batch, frame, bottom_right.place("orbit", orbit_block_height(frame)));
    }
    if (density_at_least(density, Density::Three)) {
        draw_systems(batch, frame, bottom_right.place("systems", 46.0f));
    }
    if (frame.worldsValid &&
        (density_at_least(density, Density::Two) || frame.landed || frame.airDensity > 0.0)) {
        // The rows it can offer decide its height, so the cursor does the same job here as it does
        // for every other block: nothing is placed by hand.
        const int rows = 1 + (frame.airDensity > 0.0 ? 3 : 0) + (frame.landed ? 1 : 0) +
                         (frame.surveyFraction > 0.0 ? 1 : 0) +
                         (frame.satellitesUp + frame.satellitesPending > 0 ? 1 : 0);
        draw_worlds(batch, frame, bottom_left.place("worlds", static_cast<float>(rows) * 22.0f));
    }
    if (density_at_least(density, Density::Two)) {
        draw_vessel(batch, frame, bottom_left.place("vessel", 26.0f));
    }
    draw_bars(batch, frame, bottom_left.place("bars", 104.0f));

    // ---- centre: the two vectors the pilot flies by, then the readouts under the ship
    // Velocity: a line from the ship along where it is actually going, scaled by speed, with a
    // tick across its tip. The nose may point anywhere.
    if (frame.speed > 0.5) {
        const float length_px = std::min(24.0f + static_cast<float>(frame.speed) * 1.35f, 190.0f);
        const glm::vec2 tip = frame.shipScreen + frame.velocityDir * length_px;
        push_line(batch, frame.shipScreen, tip, 1.5f, with_alpha(NAV, 0.85f));
        const glm::vec2 across(-frame.velocityDir.y, frame.velocityDir.x);
        push_line(batch, tip - across * 5.0f, tip + across * 5.0f, 1.5f, with_alpha(NAV, 0.85f));
    }
    // Nose: a short tick ahead of the hull so the facing is never in doubt.
    if (!frame.wrecked) {
        const glm::vec2 nose_tip = frame.shipScreen + frame.noseDir * (frame.shipRadiusPx + 22.0f);
        push_line(batch, frame.shipScreen + frame.noseDir * (frame.shipRadiusPx + 6.0f), nose_tip,
                  1.5f, with_alpha(ETCH, 0.7f));
    }
    if (frame.wrecked) {
        push_text(batch, "HULL LOST", 46.0f, {centre.x, centre.y - 150.0f}, TextAlign::Center,
                  THREAT);
        push_text(batch, "R restarts the run", 17.0f, {centre.x, centre.y - 108.0f},
                  TextAlign::Center, ETCH, TextFace::Label);
    } else {
        const float under_ship = centre.y + frame.shipRadiusPx + 14.0f;
        if (frame.speed > 0.05) {
            push_text(batch, readout(static_cast<float>(frame.speed), 1, "").c_str(), 46.0f,
                      {centre.x, under_ship}, TextAlign::Center, ETCH);
            push_text(batch, "m/s", 11.0f, {centre.x, under_ship + 50.0f}, TextAlign::Center,
                      ETCH_DIM, TextFace::Label);
        }
        if (frame.context && *frame.context) {
            push_text(batch, frame.context, 12.0f,
                      {centre.x, under_ship + (frame.speed > 0.05 ? 70.0f : 8.0f)},
                      TextAlign::Center, frame.contextColor, TextFace::Label);
        }
    }

    // The layout assertion pass (plan 5.2): under --debug every block is checked against the safe
    // area, against its siblings and against the texts anchored inside it. This is the check that
    // would have caught R-1 on the frame it was written.
    if (frame.debug) {
        std::vector<BlockRect> blocks;
        for (const BlockLayouter *corner : {&top_left, &top_right, &bottom_left, &bottom_right}) {
            blocks.insert(blocks.end(), corner->blocks().begin(), corner->blocks().end());
        }
        check_block_layout(blocks, batch, safe, true);
    }
}

void build_dock_overlay(UIBatch &batch, const HudFrame &frame, const DockFrame &dock) {
    const glm::vec2 screen = frame.screen;
    const float w = screen.x;
    const float h = screen.y;
    const float safe = ui::tokens::safe_inset(w, h);
    const glm::vec2 centre = frame.shipScreen;

    // The corridor, seen down the axis: two rails, rungs, the port ring at the top, the hull at the
    // bottom. Everything is drawn in the mark language - no surfaces, no panels.
    const float half_width = 46.0f;
    const float top = centre.y - 250.0f;
    const float bottom = centre.y + 250.0f;
    const glm::vec4 rail = ui::tokens::ETCH_DIM;
    ui::push_line(batch, {centre.x - half_width, top}, {centre.x - half_width, bottom}, 1.0f, rail);
    ui::push_line(batch, {centre.x + half_width, top}, {centre.x + half_width, bottom}, 1.0f, rail);
    for (int rung = 1; rung <= 5; ++rung) {
        const float y = top + (bottom - top) * static_cast<float>(rung) / 6.0f;
        ui::push_line(batch, {centre.x - half_width, y}, {centre.x + half_width, y}, 1.0f,
                      ui::tokens::VELLUM_RULE);
    }
    // The berth: a ring with the port's own name under it.
    ui::push_arc(batch, {centre.x, top}, 26.0f, 0.0f, 6.2831853f, 2.0f,
                 dock.held ? ui::tokens::NAV : ui::tokens::ETCH_DIM);
    if (dock.port && *dock.port) {
        ui::push_text(batch, dock.port, 13.0f, {centre.x + half_width + 20.0f, top - 8.0f},
                  TextAlign::Left, ui::tokens::ETCH_DIM, TextFace::Label);
    }
    // The hull's own mark, sitting in the corridor at the axial fraction the gate reports.
    const float axial_fraction = std::max(0.0f, std::min(1.0f, dock.axial / 60.0f));
    const float hull_y = top + (bottom - top) * (1.0f - axial_fraction * 0.55f);
    ui::push_arc(batch, {centre.x, hull_y}, 18.0f, 0.0f, 6.2831853f, 2.0f, ui::tokens::ETCH);
    ui::push_line(batch, {centre.x, hull_y - 12.0f}, {centre.x, hull_y + 12.0f}, 2.0f,
                  ui::tokens::ETCH);

    // The terms: out of tolerance in threat, in tolerance at dormant weight, which is the whole
    // point of the overlay - the eye goes to the one that is wrong and nothing else moves.
    const float column =
        std::min(centre.x + half_width + 20.0f, w - safe - static_cast<float>(w) * 0.02f);
    float y = top + 44.0f;
    const auto term = [&](const char *name, const char *value, bool ok) {
        const glm::vec4 color = ok ? ui::tokens::ETCH_DIM : ui::tokens::THREAT;
        ui::push_text(batch, name, 13.0f, {column, y}, TextAlign::Left, color, TextFace::Label);
        ui::push_text(batch, value, 20.0f, {column + 120.0f, y - 4.0f}, TextAlign::Left, color);
        y += 34.0f;
    };
    term("lateral", readout(dock.lateral, 1, " m").c_str(), dock.lateral_ok);
    term("closing", readout(dock.closing, 1, " m/s").c_str(), dock.closing_ok);
    term("align", readout(dock.alignment, 0, " deg").c_str(), dock.alignment_ok);
    term("rate", readout(dock.rate, 1, " deg/s").c_str(), dock.rate_ok);

    // The hold, then the range: what the reader needs to close the last metre.
    y += 8.0f;
    const float hold_fraction =
        dock.hold_required > 0.0f ? std::min(1.0f, dock.hold / dock.hold_required) : 0.0f;
    ui::push_rect(batch, {column, y}, {120.0f, 3.0f}, ui::tokens::VELLUM_RULE);
    ui::push_rect(batch, {column, y}, {120.0f * hold_fraction, 3.0f},
                  dock.held ? ui::tokens::NAV : ui::tokens::DRIVE);
    y += 30.0f;
    ui::push_text(batch, readout(dock.range, 0, " m").c_str(), 25.0f, {column, y}, TextAlign::Left,
              ui::tokens::ETCH);

    if (dock.docked) {
        ui::push_text(batch, "hard dock", 20.0f, {centre.x, bottom + 28.0f}, TextAlign::Center,
                  ui::tokens::NAV, TextFace::Label);
        ui::push_text(batch, "R to release", 13.0f, {centre.x, bottom + 52.0f}, TextAlign::Center,
                  ui::tokens::ETCH_DIM, TextFace::Label);
    } else if (dock.held) {
        ui::push_text(batch, "capture", 20.0f, {centre.x, bottom + 28.0f}, TextAlign::Center,
                  ui::tokens::DRIVE, TextFace::Label);
    }
}

}  // namespace opra
