#include "hud/minimap.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>

#include "hud/hud.h"
#include "ui/tokens.h"

namespace opra {
namespace {

using ui::Rect;
using ui::tokens::DRIVE;
using ui::tokens::ETCH;
using ui::tokens::ETCH_DIM;
using ui::tokens::NAV;
using ui::tokens::VELLUM_RULE;

/** The scope is square and the same square in every mode: one corner, one footprint (F5). */
constexpr float SIDE = 176.0f;
constexpr float HEADER_ROW = 20.0f;
constexpr float CAPTION_GAP = 4.0f;
constexpr float CAPTION_ROW = 18.0f;
constexpr float PAD_BOTTOM = 4.0f;

/** The corridor plan's own scale: the lateral limit takes this fraction of the half-box. */
constexpr float CORRIDOR_LATERAL_FRACTION = 0.55f;

/** The ground is only on the altitude scale when it is this many spans below the conic's low end. */
constexpr double GROUND_SPANS = 3.0;

std::string metres(double value) { return distance_readout(value); }

/** The berth ring, the rails and the corridor axis: the plan view's whole furniture. */
void draw_corridor(UIBatch& batch, const MinimapFrame& minimap, const Rect& box) {
    const glm::vec2 centre(box.x + SIDE * 0.5f, box.y + SIDE * 0.5f);
    const float scale = CORRIDOR_LATERAL_FRACTION * (SIDE * 0.5f) / std::max(1.0f, minimap.lateralLimit);
    // North is up: a world direction is drawn as (x, -y), which is the collar's own mapping.
    const glm::vec2 axis(std::cos(minimap.berthBearing), -std::sin(minimap.berthBearing));
    const glm::vec2 side(-axis.y, axis.x);
    const float rail = minimap.lateralLimit * scale;
    // Clip the corridor's lines to the box by construction: the longest run along the axis that
    // still ends inside it.
    const float run = 0.95f * (SIDE * 0.5f) /
                      std::max(std::fabs(axis.x), std::fabs(axis.y)) - rail;
    ui::push_line(batch, centre - axis * run, centre + axis * run, 1.0f, ui::with_alpha(ETCH, 0.25f));
    ui::push_line(batch, centre + side * rail - axis * run, centre + side * rail + axis * run, 1.0f,
                  VELLUM_RULE);
    ui::push_line(batch, centre - side * rail - axis * run, centre - side * rail + axis * run, 1.0f,
                  VELLUM_RULE);
    ui::push_arc(batch, centre, 10.0f, 0.0f, 6.2831853f, 1.5f, NAV);
    const glm::vec2 ship = centre + glm::vec2(minimap.berthOffset.x, -minimap.berthOffset.y) * scale;
    const float reach = SIDE * 0.5f - 7.0f;
    const glm::vec2 offset = ship - centre;
    const float length = glm::length(offset);
    const glm::vec2 shown = length > reach ? centre + offset * (reach / length) : ship;
    ui::push_disc(batch, shown, 3.0f, length > reach ? ui::with_alpha(ETCH, 0.5f) : ETCH);
}

void draw_radar(UIBatch& batch, const MinimapFrame& minimap, const Rect& box) {
    const glm::vec2 centre(box.x + SIDE * 0.5f, box.y + SIDE * 0.5f);
    const float outer = SIDE * 0.5f - 8.0f;
    ui::push_arc(batch, centre, outer, 0.0f, 6.2831853f, 1.0f, ui::with_alpha(ETCH, 0.32f));
    ui::push_arc(batch, centre, outer * 0.5f, 0.0f, 6.2831853f, 1.0f, ui::with_alpha(ETCH, 0.12f));
    ui::push_line(batch, {centre.x, centre.y - outer}, {centre.x, centre.y + outer}, 1.0f,
                  ui::with_alpha(ETCH, 0.12f));
    ui::push_line(batch, {centre.x - outer, centre.y}, {centre.x + outer, centre.y}, 1.0f,
                  ui::with_alpha(ETCH, 0.12f));
    ui::push_text(batch, "N", 10.0f, {centre.x - 2.0f, centre.y - outer + 3.0f}, TextAlign::Left,
                  ETCH_DIM, TextFace::Label);

    const float reach = outer - 4.0f;
    const float scale = reach / std::max(1.0f, minimap.range);
    for (const MinimapFrame::Blip& blip : minimap.blips) {
        const glm::vec2 offset(std::cos(blip.bearing) * blip.range * scale,
                               -std::sin(blip.bearing) * blip.range * scale);
        const float length = glm::length(offset);
        const glm::vec2 shown = length > reach ? centre + offset * (reach / length) : centre + offset;
        const float alpha = length > reach ? 0.45f : 0.9f;
        if (blip.tracked) {
            ui::push_line(batch, shown + glm::vec2(-5.0f, 0.0f), shown + glm::vec2(5.0f, 0.0f), 1.4f,
                          ui::with_alpha(NAV, alpha));
            ui::push_line(batch, shown + glm::vec2(0.0f, -5.0f), shown + glm::vec2(0.0f, 5.0f), 1.4f,
                          ui::with_alpha(NAV, alpha));
        } else {
            ui::push_disc(batch, shown, 2.5f, ui::with_alpha(NAV, alpha));
        }
    }
    // The ship, at the scope's own centre: the radar is relative by construction.
    ui::push_disc(batch, centre, 2.5f, ETCH);
    ui::push_line(batch, centre + glm::vec2(0.0f, -6.0f), centre + glm::vec2(0.0f, 6.0f), 1.0f,
                  ui::with_alpha(ETCH, 0.6f));
    ui::push_line(batch, centre + glm::vec2(-6.0f, 0.0f), centre + glm::vec2(6.0f, 0.0f), 1.0f,
                  ui::with_alpha(ETCH, 0.6f));
}

void draw_strip(UIBatch& batch, const MinimapFrame& minimap, const Rect& box) {
    const float axis_x = box.x + 24.0f;
    const float top = box.y + 14.0f;
    const float bottom = box.y + SIDE - 16.0f;
    double low = std::min(minimap.periapsis, minimap.altitude);
    double high = minimap.elliptic ? std::max(minimap.apoapsis, minimap.altitude)
                                   : std::max(minimap.altitude, minimap.periapsis);
    // The ground joins the scale when it is close enough to read with the conic; a ship 25 Gm out
    // would otherwise have its whole orbit squeezed into the last few pixels of a 0-based axis.
    if (low > 0.0 && low <= GROUND_SPANS * (high - low)) low = 0.0;
    if (!(high > low)) high = low + 1.0;
    const auto y_at = [&](double metres_above_datum) {
        return bottom + static_cast<float>((metres_above_datum - low) / (high - low)) * (top - bottom);
    };

    ui::push_line(batch, {axis_x, top}, {axis_x, bottom}, 1.0f, ui::with_alpha(ETCH, 0.3f));
    const auto tick = [&](double value, const char* text, const glm::vec4& color, bool caret) {
        const float y = std::clamp(y_at(value), top, bottom);
        ui::push_line(batch, {axis_x - 4.0f, y}, {axis_x + 4.0f, y}, 1.0f, color);
        if (caret) {
            ui::push_triangle(batch, {axis_x - 5.0f, y}, {axis_x - 11.0f, y - 4.0f},
                              {axis_x - 11.0f, y + 4.0f}, color);
        }
        ui::push_text(batch, text, 12.0f, {axis_x + 8.0f, y - 7.0f}, TextAlign::Left, color);
    };
    if (low == 0.0) tick(0.0, "surface", ui::with_alpha(ETCH_DIM, 0.7f), false);
    const std::string peri = metres(minimap.periapsis);
    const std::string height = metres(minimap.altitude);
    tick(minimap.periapsis, peri.c_str(), ETCH_DIM, false);
    tick(minimap.altitude, height.c_str(), ETCH, true);
    if (minimap.elliptic) {
        const std::string apo = metres(minimap.apoapsis);
        tick(minimap.apoapsis, apo.c_str(), ETCH_DIM, false);
    } else {
        ui::push_text(batch, "escape", 12.0f, {axis_x + 8.0f, top - 8.0f}, TextAlign::Left,
                      ui::with_alpha(ETCH_DIM, 0.7f), TextFace::Label);
    }
}

}  // namespace

MinimapMode next_minimap_mode(MinimapMode mode) {
    switch (mode) {
        case MinimapMode::Contact: return MinimapMode::Orbit;
        case MinimapMode::Orbit: return MinimapMode::Corridor;
        case MinimapMode::Corridor: break;
    }
    return MinimapMode::Contact;
}

const char* minimap_mode_name(MinimapMode mode) {
    switch (mode) {
        case MinimapMode::Contact: return "contacts";
        case MinimapMode::Orbit: return "orbit";
        case MinimapMode::Corridor: return "corridor";
    }
    return "contacts";
}

MinimapMode minimap_context_mode(bool berth_in_range, bool contact_in_range) {
    if (berth_in_range) return MinimapMode::Corridor;
    return contact_in_range ? MinimapMode::Contact : MinimapMode::Orbit;
}

MinimapChoice& minimap_choice() {
    // The HUD's own memory of the key: App carries the world, and this is not world state. One
    // definition, so the key in game/frame.cpp and the resolution in game/scene.cpp cannot drift.
    static MinimapChoice choice;
    return choice;
}

MinimapMode resolve_minimap_mode(bool berth_in_range, bool contact_in_range) {
    MinimapChoice& choice = minimap_choice();
    const MinimapMode context = minimap_context_mode(berth_in_range, contact_in_range);
    // The context owns the mode until the pilot takes it with the key, and the corridor on
    // approach outranks both: that is the one moment the plan says the context forces the view.
    if (!choice.manual || context == MinimapMode::Corridor) choice.mode = context;
    return choice.mode;
}

float minimap_block_height(const MinimapFrame& minimap) {
    if (!minimap.active) return 0.0f;
    return HEADER_ROW + SIDE + CAPTION_GAP + CAPTION_ROW + PAD_BOTTOM;
}

void build_minimap(UIBatch& batch, const MinimapFrame& minimap, const ui::Rect& at) {
    if (!minimap.active) return;
    const Rect box{at.x + (at.w - SIDE) * 0.5f, at.y + HEADER_ROW, SIDE, SIDE};
    ui::push_text(batch, minimap_mode_name(minimap.mode), 12.0f, {at.x, at.y + 2.0f},
                  TextAlign::Left, ETCH, TextFace::Label);
    ui::push_text(batch, "north up", 11.0f, {at.x + at.w, at.y + 3.0f}, TextAlign::Right, ETCH_DIM,
                  TextFace::Label);

    char caption[96];
    switch (minimap.mode) {
        case MinimapMode::Contact:
            std::snprintf(caption, sizeof caption, "%zu contacts   %s", minimap.blips.size(),
                          metres(minimap.range).c_str());
            draw_radar(batch, minimap, box);
            break;
        case MinimapMode::Orbit:
            std::snprintf(caption, sizeof caption, "%s  orbit", minimap.primary);
            draw_strip(batch, minimap, box);
            break;
        case MinimapMode::Corridor:
            std::snprintf(caption, sizeof caption, "axial %s   lateral %s   closing %s",
                          metres(minimap.axial).c_str(), metres(minimap.lateral).c_str(),
                          speed_readout(minimap.closing).c_str());
            draw_corridor(batch, minimap, box);
            break;
    }
    ui::push_text(batch, caption, 11.0f, {at.x, box.y + SIDE + CAPTION_GAP}, TextAlign::Left,
                  ETCH_DIM, TextFace::Label);
}

}  // namespace opra
