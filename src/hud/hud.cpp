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

/**
 * s4.3: thousands grouped with thin spaces, so `1 204.8 km` never changes width as it counts. One
 * implementation for every block; a readout that grows a digit is a readout that moves its
 * neighbours.
 */
std::string grouped(double value, int decimals, const char *suffix = "") {
    char raw[48];
    std::snprintf(raw, sizeof raw, "%.*f", decimals, value);
    std::string digits(raw);
    const size_t dot = digits.find('.');
    const size_t end = dot == std::string::npos ? digits.size() : dot;
    std::string out;
    for (size_t i = 0; i < end; ++i) {
        const bool boundary = i > 0 && (end - i) % 3 == 0;
        const bool negative = digits[0] == '-';
        if (boundary && !(negative && i == 1)) out += "\xe2\x80\x89";  // U+2009 thin space
        out += digits[i];
    }
    if (dot != std::string::npos) out += digits.substr(dot);
    return out + suffix;
}

/** Seconds as s4.3 prints them: `31 d 22:14`, `4:12`, `0.8 s`. */
std::string span_readout(double seconds) {
    const bool negative = seconds < 0.0;
    double magnitude = std::fabs(seconds);
    char buffer[48];
    if (magnitude >= 86400.0) {
        const long long days = static_cast<long long>(magnitude / 86400.0);
        const int hours = static_cast<int>(std::fmod(magnitude, 86400.0) / 3600.0);
        const int minutes = static_cast<int>(std::fmod(magnitude, 3600.0) / 60.0);
        std::snprintf(buffer, sizeof buffer, "%s%lld d %02d:%02d", negative ? "-" : "", days, hours,
                      minutes);
    } else if (magnitude >= 3600.0) {
        const int hours = static_cast<int>(magnitude / 3600.0);
        const int minutes = static_cast<int>(std::fmod(magnitude, 3600.0) / 60.0);
        std::snprintf(buffer, sizeof buffer, "%s%d:%02d h", negative ? "-" : "", hours, minutes);
    } else if (magnitude >= 60.0) {
        const int minutes = static_cast<int>(magnitude / 60.0);
        const int secs = static_cast<int>(std::fmod(magnitude, 60.0));
        std::snprintf(buffer, sizeof buffer, "%s%d:%02d", negative ? "-" : "", minutes, secs);
    } else {
        std::snprintf(buffer, sizeof buffer, "%s%.1f s", negative ? "-" : "", magnitude);
    }
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
/** The named header rule: title, then a hairline to the block's right edge, then an optional
 *  right-aligned suffix. The device that lets density stay readable. */
void block_header(UIBatch &batch, const Rect &at, const char *title, const char *suffix = nullptr) {
    push_text(batch, title, 13.0f, {at.x, at.y}, TextAlign::Left, ETCH_DIM, TextFace::Label);
    float title_w = 0.0f;
    for (const char *c = title; *c; ++c) title_w += 13.0f * 0.58f;
    const float rule_x = at.x + title_w + 8.0f;
    const float rule_end = at.x + at.w - (suffix && *suffix ? 90.0f : 0.0f);
    if (rule_end > rule_x) {
        push_rect(batch, {rule_x, at.y + 9.0f}, {rule_end - rule_x, 1.0f},
                  with_alpha(ETCH, 0.22f));
    }
    if (suffix && *suffix) {
        push_text(batch, suffix, 13.0f, {at.x + at.w, at.y}, TextAlign::Right, ETCH_DIM,
                  TextFace::Label);
    }
}

/** One label/value row of a block: the label dormant, the value at its weight. */
void block_row(UIBatch &batch, const Rect &at, float y, const char *label, const std::string &value,
               Weight weight = Weight::Live) {
    push_text(batch, label, 12.0f, {at.x + 10.0f, y + 2.0f}, TextAlign::Left,
              with_alpha(ETCH_DIM, weight_opacity(weight)), TextFace::Label);
    push_text(batch, value.c_str(), 15.0f, {at.x + at.w - 10.0f, y}, TextAlign::Right,
              with_alpha(weight_color(weight), weight_opacity(weight)));
}

// The three gauges, and the world-under-the-ship rows: shared by the s4.1 blocks.
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

/**
 * The world under the ship (plan 3.2-3.7): altitude, air, the rate of descent, the base on the pad
 * it is standing on, and what the survey and the satellites have to say.
 */
void draw_worlds(UIBatch &batch, const HudFrame &frame, const Rect &at) {
    block_header(batch, at, "WORLD", frame.worldsBody);
    float y = at.y + 22.0f;
    const auto row = [&](const char *label, const std::string &value, const glm::vec4 &color) {
        push_text(batch, label, 11.0f, {at.x + 10.0f, y + 2.0f}, TextAlign::Left, ETCH_DIM,
                  TextFace::Label);
        push_text(batch, value.c_str(), 15.0f, {at.x + at.w - 10.0f, y}, TextAlign::Right, color);
        y += 20.0f;
    };

    row("alt", distance_readout(frame.altitude), ETCH);
    if (frame.airDensity > 0.0) {
        char buffer[64];
        std::snprintf(buffer, sizeof buffer, "%.2e", frame.airDensity);
        row("rho", buffer, ETCH_DIM);
        // The flux in the unit a pilot reads: a megawatt per square metre is a hot entry.
        std::snprintf(buffer, sizeof buffer, "%.2f MW/m2", frame.airFlux / 1.0e6);
        row("q", buffer, frame.airFlux > HEAT_FLUX_FULL * 0.5 ? THREAT : ETCH_DIM);
        row("descent", speed_readout(frame.descentRate),
            frame.descentRate > 4.0 ? DRIVE : ETCH_DIM);
    }
    if (frame.landed) {
        row("landed", frame.baseName && *frame.baseName ? frame.baseName : "open ground",
            frame.baseName && *frame.baseName ? NAV : ETCH);
    }
    if (frame.surveyFraction > 0.0) {
        row("survey", grouped(frame.surveyFraction * 100.0, 0, "%"), ETCH_DIM);
    }
    if (frame.satellitesUp + frame.satellitesPending > 0) {
        char buffer[40];
        std::snprintf(buffer, sizeof buffer, "%d up  %d due", frame.satellitesUp,
                      frame.satellitesPending);
        row("sats", buffer, ETCH_DIM);
    }
}

// ---------------------------------------------------------------- the s4.1 blocks
//
// KSP's lesson (J7): dense information reads when it is grouped into bounded blocks, each answering
// one question, with a named header rule the eye can find - and a block that has nothing to say is
// not drawn at all (s4.2's live()), so the ones below it move up.

constexpr float VESSEL_BARS_H = 96.0f;  // three bar rows
constexpr float VESSEL_ROWS_H = 60.0f;  // mass, TWR, dv

/** The ship's own numbers: the three gauges, then mass, TWR and the dv budget (s4.1's VESSEL). */
float vessel_block_height(const HudFrame &frame) {
    (void)frame;
    return 22.0f + VESSEL_BARS_H + VESSEL_ROWS_H;
}

void build_vessel_block(UIBatch &batch, const HudFrame &frame, const Rect &at) {
    block_header(batch, at, "VESSEL", frame.shipName);
    Rect bars = at;
    bars.y += 22.0f;
    bars.h = VESSEL_BARS_H;
    draw_bars(batch, frame, bars);
    const float rows_y = at.y + 22.0f + VESSEL_BARS_H;
    block_row(batch, at, rows_y, "mass", grouped(frame.massTonnes, 1) + " t", Weight::Dormant);
    // s4.6: a TWR with no dominant body prints the dash, never a zero.
    block_row(batch, at, rows_y + 20.0f, "TWR",
              frame.twrValid ? readout(static_cast<float>(frame.twr), 2, "") : std::string("--"),
              frame.twrValid && frame.twr < 1.0 ? Weight::Critical : Weight::Live);
    block_row(batch, at, rows_y + 40.0f, "\xce\x94v", speed_readout(frame.deltaV),
              frame.deltaV <= 0.01 ? Weight::Critical : Weight::Live);
}

/** Guns and tubes. Mining forces it on (the ore count is the job's readout); incoming fire forces
 *  it harder (s4.5). A magazine that is empty reads dry - it does not silently stop (s5.5). */
float weapons_block_height(const HudFrame &frame) {
    return 22.0f + 20.0f * (static_cast<float>(frame.gunCount) + 1.0f);
}

void build_weapons_block(UIBatch &batch, const HudFrame &frame, const Rect &at) {
    block_header(batch, at, "WEAPONS");
    const float rows_y = at.y + 22.0f;
    for (int i = 0; i < frame.gunCount; ++i) {
        const bool ready = i < frame.gunsReady;
        char mount[8];
        std::snprintf(mount, sizeof mount, "PDC %c", 'A' + i);
        block_row(batch, at, rows_y + 20.0f * static_cast<float>(i), mount,
                  frame.gunsHot ? "hot" : ready ? "ready" : "dry",
                  frame.gunsHot ? Weight::Critical : ready ? Weight::Live : Weight::Dormant);
    }
    block_row(batch, at, rows_y + 20.0f * static_cast<float>(frame.gunCount), "cutter",
              frame.oreHeld > 0.0 ? grouped(frame.oreHeld, 0, " ore")
                                  : std::string(frame.forceGuns ? "engaged" : "stowed"),
              frame.forceGuns ? Weight::Live : Weight::Dormant);
}

/** The first planned node (s4.1's NODE): dv, burn time, and the countdown. Gone when the node is
 *  consumed - it never shows a negative T- (s4.6). */
float node_block_height(const HudFrame &frame) {
    return frame.nodeValid ? 22.0f + 20.0f * 3 : 0.0f;
}

void build_node_block(UIBatch &batch, const HudFrame &frame, const Rect &at) {
    if (!frame.nodeValid) return;
    block_header(batch, at, "NODE", "circularise");
    const float rows_y = at.y + 22.0f;
    block_row(batch, at, rows_y, "\xce\x94v", speed_readout(frame.nodeDeltaV));
    block_row(batch, at, rows_y + 20.0f, "burn", frame.nodeBurnSeconds > 0.0
                                                      ? span_readout(frame.nodeBurnSeconds)
                                                      : std::string("instant"));
    block_row(batch, at, rows_y + 40.0f, "T-", span_readout(std::max(0.0, frame.nodeTMinus)),
              frame.nodeTMinus < 60.0 ? Weight::Critical : Weight::Live);
}

/** Incoming fire (s4.5, s5): the one block the pilot does not get to ignore. */
float threat_block_height(const HudFrame &frame) {
    return frame.underFire ? 22.0f + 20.0f : 0.0f;
}

void build_threat_block(UIBatch &batch, const HudFrame &frame, const Rect &at) {
    if (!frame.underFire) return;
    block_header(batch, at, "THREAT");
    block_row(batch, at, at.y + 22.0f, "hull", readout(frame.hullValue, 0, ""), Weight::Critical);
}

/** The top strip: contract and progress on the left, the world under the ship in the middle, the
 *  session clock and the warp rail on the right. Not a block - it is the frame the blocks sit in. */
void draw_top_strip(UIBatch &batch, const HudFrame &frame, const Rect &safe) {
    const float right = safe.x + safe.w;
    push_text(batch, "OPRA", 22.0f, {safe.x, safe.y}, TextAlign::Left, ETCH);
    if (frame.contractId && *frame.contractId) {
        push_text(batch, frame.contractId, 13.0f, {safe.x + 92.0f, safe.y + 2.0f}, TextAlign::Left,
                  ETCH_DIM, TextFace::Label);
    }
    if (frame.objective && *frame.objective) {
        push_text(batch, frame.objective, 15.0f, {safe.x + 92.0f, safe.y + 20.0f}, TextAlign::Left,
                  ETCH, TextFace::Label);
    }
    push_rect(batch, {safe.x, safe.y + 44.0f}, {232.0f, 1.0f}, with_alpha(ETCH, 0.15f));
    push_rect(batch, {safe.x, safe.y + 44.0f}, {232.0f * ui::clamp01(frame.progress), 1.0f}, NAV);

    // The world under the ship (s4.1's top-middle): altitude and vertical speed, the two numbers a
    // descent is flown on.
    if (frame.worldsValid) {
        const float centre_x = safe.x + safe.w * 0.5f;
        push_text(batch, distance_readout(frame.altitude).c_str(), 25.0f, {centre_x, safe.y},
                  TextAlign::Center, ETCH);
        const Weight vs_weight = frame.verticalSpeed < -4.0   ? Weight::Critical
                                 : frame.verticalSpeed > 4.0 ? Weight::Live
                                                             : Weight::Dormant;
        char vs[48];
        std::snprintf(vs, sizeof vs, "%s %s m/s",
                      frame.verticalSpeed > 0.0 ? "\xe2\x96\xb2" : "\xe2\x96\xbc",
                      grouped(frame.verticalSpeed, 1).c_str());
        push_text(batch, vs, 15.0f, {centre_x, safe.y + 30.0f}, TextAlign::Center,
                  with_alpha(weight_color(vs_weight), weight_opacity(vs_weight)), TextFace::Label);
    }

    char clock[40];
    const int total = static_cast<int>(std::max(0.0f, frame.sessionSeconds));
    std::snprintf(clock, sizeof clock, "T+ %02d:%02d:%02d", total / 3600, total % 3600 / 60,
                  total % 60);
    push_text(batch, clock, 15.0f, {right, safe.y}, TextAlign::Right, ETCH);
    push_disc(batch, {right - 2.0f, safe.y + 6.0f}, 2.0f, status_color(frame.status));
    char warp[48];
    if (frame.warpSuggest > frame.warpRate + 0.01 && frame.warpRate <= 1.0) {
        // The rail's suggestion (s2.7): shown, never applied silently.
        std::snprintf(warp, sizeof warp, "warp %gx  \xe2\x96\xb3 %gx", frame.warpRate,
                      frame.warpSuggest);
    } else {
        std::snprintf(warp, sizeof warp, "warp %gx", frame.warpRate);
    }
    push_text(batch, warp, 12.0f, {right, safe.y + 22.0f}, TextAlign::Right, ETCH_DIM,
              TextFace::Label);
}

/** The bottom strip: throttle, g, heading, and the keys that are always live. */
void draw_bottom_strip(UIBatch &batch, const HudFrame &frame, const Rect &safe) {
    const float y = safe.y + safe.h - 30.0f;
    const float bar_w = 120.0f;
    push_text(batch, "throttle", 11.0f, {safe.x, y + 6.0f}, TextAlign::Left, ETCH_DIM,
              TextFace::Label);
    push_rect(batch, {safe.x + 58.0f, y + 12.0f}, {bar_w, 3.0f}, with_alpha(ETCH, 0.12f));
    push_rect(batch, {safe.x + 58.0f, y + 12.0f}, {bar_w * ui::clamp01(frame.throttle), 3.0f},
              DRIVE);
    push_text(batch, readout(ui::clamp01(frame.throttle), 2, "").c_str(), 15.0f,
              {safe.x + 58.0f + bar_w + 10.0f, y}, TextAlign::Left, ETCH);

    const float centre_x = safe.x + safe.w * 0.5f;
    char g[32];
    std::snprintf(g, sizeof g, "%.2f g", frame.accelerationG);
    push_text(batch, g, 15.0f, {centre_x - 60.0f, y}, TextAlign::Right, ETCH);
    char hdg[16];
    std::snprintf(hdg, sizeof hdg, "%03d\xc2\xb0",
                  static_cast<int>(std::lround(frame.headingDeg)) % 360);
    push_text(batch, hdg, 15.0f, {centre_x + 60.0f, y}, TextAlign::Left, ETCH);

    const float right = safe.x + safe.w;
    push_text(batch, frame.assist ? "assist F" : "assist off F", 11.0f, {right - 210.0f, y + 4.0f},
              TextAlign::Left, frame.assist ? NAV : ETCH_DIM, TextFace::Label);
    push_text(batch, "kill X", 11.0f, {right - 120.0f, y + 4.0f}, TextAlign::Left,
              frame.braking ? DRIVE : ETCH_DIM, TextFace::Label);
    push_text(batch, "warp . ,", 11.0f, {right, y + 4.0f}, TextAlign::Right, ETCH_DIM,
              TextFace::Label);
}

}  // namespace

void build_flight_hud(UIBatch &batch, const HudFrame &frame) {
    const float width = frame.screen.x;
    const float height = frame.screen.y;
    const glm::vec2 centre = frame.shipScreen;

    // D-18: one safe-area inset for the whole layout - 28 px, or 3% of the short edge.
    const float inset = std::max(28.0f, std::min(width, height) * 0.03f);
    const Rect safe{inset, inset, width - inset * 2.0f, height - inset * 2.0f};
    const bool narrow = safe.w < 1000.0f;  // s4.6: the right column collapses under the left
    const float column_w = 300.0f;

    float radius = frame.radius > 0.0f ? frame.radius
                                       : std::clamp(std::min(width, height) * 0.29f, 150.0f, 260.0f);
    radius = std::min(radius, std::min(width, height) * 0.5f - LABEL_RADIUS - 4.0f);

    draw_top_strip(batch, frame, safe);
    draw_bottom_strip(batch, frame, safe);

    // The columns start below the top strip and stop above the bottom strip.
    const Rect columns{safe.x, safe.y + 70.0f, safe.w, safe.h - 110.0f};
    const Rect left_column{columns.x, columns.y, column_w, columns.h};
    const Rect right_base{columns.x + columns.w - column_w, columns.y, column_w, columns.h};

    // Left column, downward. Every block answers live() first (s4.2): the layouter is the cursor,
    // so a block that stays home lets the ones below it move up.
    BlockLayouter left(left_column, true);
    build_vessel_block(batch, frame, left.place("vessel", vessel_block_height(frame)));
    if (density_at_least(frame.density, Density::Two) || frame.forceGuns || frame.underFire) {
        build_weapons_block(batch, frame, left.place("weapons", weapons_block_height(frame)));
    }
    if (frame.director.active &&
        (density_at_least(frame.density, Density::Two) || frame.forceProgram)) {
        build_program_block(batch, frame, left.place("program", program_block_height(frame)));
    }
    if (frame.worldsValid &&
        (density_at_least(frame.density, Density::Two) || frame.landed || frame.airDensity > 0.0)) {
        const int rows = 1 + (frame.airDensity > 0.0 ? 3 : 0) + (frame.landed ? 1 : 0) +
                         (frame.surveyFraction > 0.0 ? 1 : 0) +
                         (frame.satellitesUp + frame.satellitesPending > 0 ? 1 : 0);
        draw_worlds(batch, frame, left.place("worlds", static_cast<float>(rows) * 22.0f + 6.0f));
    }
    if (frame.underFire) {
        build_threat_block(batch, frame, left.place("threat", threat_block_height(frame)));
    }

    // Right column: orbit, node, minimap. Narrow windows stack it under the left column's last
    // block, which is the s4.6 rule - blocks stack rather than overlap.
    Rect right_rect = right_base;
    if (narrow) {
        float used = columns.y;
        for (const BlockRect &block : left.blocks()) {
            used = std::max(used, block.at.y + block.at.h);
        }
        right_rect.y = used + 12.0f;
    }
    BlockLayouter right(right_rect, true);
    if (frame.orbit.valid &&
        (density_at_least(frame.density, Density::Two) || frame.forceOrbit)) {
        build_orbit_block(batch, frame, right.place("orbit", orbit_block_height(frame)));
    }
    if (frame.nodeValid) {
        build_node_block(batch, frame, right.place("node", node_block_height(frame)));
    }
    if (frame.minimap.active && density_at_least(frame.density, Density::Two)) {
        build_minimap(batch, frame.minimap,
                      right.place("minimap", minimap_block_height(frame.minimap)));
    }

    // ---- the collar: the navball analogue (s4.4) ------------------------------------------
    if (!frame.hideCollar && radius >= 12.0f) {
        // The ring: a full circle less the deliberate 40 degree gap at the top.
        push_arc(batch, centre, radius, TOP + GAP_HALF, TOP - GAP_HALF + TAU, RING_WIDTH,
                 with_alpha(ETCH_DIM, RING_ALPHA));

        // Range rings (S-2): the labels sit on the 45-degree diagonals and never on the hull - a
        // ring that would land inside the ship's own screen disc draws neither arc nor label,
        // whatever the zoom is doing. Under a 160 px collar the labels go entirely.
        for (float metres : RANGE_RING_METRES) {
            const float r = radius * (metres / FAR_RANGE);
            const bool under_hull = r < frame.shipRadiusPx + 8.0f;
            if (!under_hull) {
                push_arc(batch, centre, r, 0.0f, TAU, 1.0f, with_alpha(ETCH_DIM, RANGE_RING_ALPHA));
            }
            if (under_hull || radius < 160.0f) continue;
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

        // The navball marks (s4.4): a vector the pilot flies by gets a place on the ring.
        // Prograde style is a circle with a dot; retrograde, the circle with a bar.
        const auto collar_mark = [&](float bearing_rad, const glm::vec4 &color, bool prograde) {
            const glm::vec2 u(std::cos(-bearing_rad), std::sin(-bearing_rad));
            const glm::vec2 t(-u.y, u.x);
            const glm::vec2 at = centre + u * radius;
            push_arc(batch, at, 5.0f, 0.0f, TAU, 1.4f, color);
            if (prograde) {
                push_disc(batch, at, 1.6f, color);
            } else {
                push_line(batch, at - t * 5.0f, at + t * 5.0f, 1.4f, color);
            }
        };
        // Prograde / retrograde from the world-frame velocity, target / anti-target from the
        // tracked contact's bearing.
        if (frame.speed > 0.5) {
            const float bearing =
                static_cast<float>(std::atan2(frame.velocityDir.y, frame.velocityDir.x) -
                                   1.5707963267948966);
            collar_mark(bearing, NAV, true);
            collar_mark(bearing + 3.14159265f, with_alpha(NAV, 0.55f), false);
        }
        if (frame.targetValid) {
            float rel_bearing = 0.0f;
            for (const CollarMark &mark : frame.marks) {
                if (mark.kind == MarkKind::Target) rel_bearing = mark.bearing;
            }
            collar_mark(rel_bearing, with_alpha(ETCH, 0.9f), true);
            collar_mark(rel_bearing + 3.14159265f, with_alpha(ETCH, 0.45f), false);
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

    // The director's cross (G8): where the cue wants the nose, on the collar's own ring.
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

    // ---- centre: the two vectors the pilot flies by, then the readouts under the ship
    if (frame.speed > 0.5) {
        const float length_px = std::min(24.0f + static_cast<float>(frame.speed) * 1.35f, 190.0f);
        const glm::vec2 tip = frame.shipScreen + frame.velocityDir * length_px;
        push_line(batch, frame.shipScreen, tip, 1.5f, with_alpha(NAV, 0.85f));
        const glm::vec2 across(-frame.velocityDir.y, frame.velocityDir.x);
        push_line(batch, tip - across * 5.0f, tip + across * 5.0f, 1.5f, with_alpha(NAV, 0.85f));
    }
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
        if (frame.targetValid) {
            // The tracked contact's range: the collar's target mark says where, this says how far.
            push_text(batch, distance_readout(frame.targetRange).c_str(), 17.0f,
                      {centre.x, under_ship + (frame.speed > 0.05 ? 66.0f : 0.0f)},
                      TextAlign::Center, with_alpha(NAV, 0.9f), TextFace::Label);
        }
        if (frame.context && *frame.context) {
            push_text(batch, frame.context, 12.0f,
                      {centre.x, under_ship + (frame.speed > 0.05 ? 92.0f : 26.0f)},
                      TextAlign::Center, frame.contextColor, TextFace::Label);
        }
    }

    // The layout assertion pass (plan 5.2): under --debug every block is checked against the safe
    // area, against its siblings and against the texts anchored inside it.
    if (frame.debug) {
        std::vector<BlockRect> blocks;
        for (const BlockLayouter *corner : {&left, &right}) {
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
