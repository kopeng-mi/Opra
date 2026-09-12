#include "ui/screens.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

#include "game/bindings.h"
#include "hud/hud.h"

namespace opra::ui {

void build_chart(UIBatch &batch, const ChartFrame &frame, float width, float height) {
    push_rect(batch, {0.0f, 0.0f}, {width, height}, with_alpha(ui::tokens::PLATE, 0.88f));

    const float margin = 90.0f;
    const float scale_x = (width - margin * 2.0f) / (frame.bounds_max.x - frame.bounds_min.x);
    const float scale_y =
        (height - margin * 2.0f - 60.0f) / (frame.bounds_max.y - frame.bounds_min.y);
    const float scale = std::min(scale_x, scale_y);
    const glm::vec2 origin(width * 0.5f, height * 0.5f + 20.0f);
    const auto to_screen = [&](const glm::vec2 &at) {
        return glm::vec2(origin.x + at.x * scale, origin.y - at.y * scale);
    };

    const glm::vec2 top_left = to_screen({frame.bounds_min.x, frame.bounds_max.y});
    const glm::vec2 bottom_right = to_screen({frame.bounds_max.x, frame.bounds_min.y});
    push_rect(batch, top_left, {bottom_right.x - top_left.x, 1.0f},
              with_alpha(ui::tokens::VELLUM_DIM, 0.35f));
    push_rect(batch, {top_left.x, bottom_right.y}, {bottom_right.x - top_left.x, 1.0f},
              with_alpha(ui::tokens::VELLUM_DIM, 0.35f));
    push_rect(batch, top_left, {1.0f, bottom_right.y - top_left.y},
              with_alpha(ui::tokens::VELLUM_DIM, 0.35f));
    push_rect(batch, {bottom_right.x, top_left.y}, {1.0f, bottom_right.y - top_left.y},
              with_alpha(ui::tokens::VELLUM_DIM, 0.35f));

    for (const ChartFrame::Dot &rock : frame.rocks) {
        push_disc(batch, to_screen(rock.at), 0.9f + rock.radius * 0.05f,
                  with_alpha(ui::tokens::VELLUM_DIM, 0.42f));
    }
    for (const ChartFrame::Dot &fragment : frame.fragments) {
        push_disc(batch, to_screen(fragment.at), 1.2f, with_alpha(ui::tokens::VELLUM_DIM, 0.5f));
    }
    for (const ChartFrame::Dot &chunk : frame.ore) {
        push_disc(batch, to_screen(chunk.at), 1.0f, with_alpha(ui::tokens::NAV, 0.8f));
    }
    for (const ChartFrame::Mark &mark : frame.marks) {
        const glm::vec2 point = to_screen(mark.at);
        switch (mark.kind) {
            case ChartFrame::Mark::Kind::Cargo:
                push_rect(batch, {point.x - 3.0f, point.y - 3.0f}, {6.0f, 6.0f}, ui::tokens::NAV);
                push_text(batch, mark.text.c_str(), 12.0f, {point.x + 10.0f, point.y - 8.0f},
                          TextAlign::Left, ui::tokens::NAV, TextFace::Label);
                break;
            case ChartFrame::Mark::Kind::Station:
                push_arc(batch, point, 9.0f, 0.0f, 6.2831853f, 1.0f, ui::tokens::NAV);
                push_text(batch, mark.text.c_str(), 13.0f, {point.x + 14.0f, point.y - 9.0f},
                          TextAlign::Left, ui::tokens::VELLUM, TextFace::Label);
                break;
            case ChartFrame::Mark::Kind::Relay:
                push_disc(batch, point, 3.0f, ui::tokens::NAV);
                push_text(batch, mark.text.c_str(), 12.0f, {point.x + 9.0f, point.y + 4.0f},
                          TextAlign::Left, ui::tokens::VELLUM_DIM, TextFace::Label);
                break;
            case ChartFrame::Mark::Kind::Derelict:
                push_disc(batch, point, 3.0f, ui::tokens::THREAT);
                push_text(batch, mark.text.c_str(), 12.0f, {point.x + 9.0f, point.y + 4.0f},
                          TextAlign::Left, with_alpha(ui::tokens::THREAT, 0.9f), TextFace::Label);
                break;
        }
    }

    // The ship: a heading triangle with its velocity drawn off the nose.
    const glm::vec2 ship_point = to_screen(frame.ship);
    const glm::vec2 forward(-std::sin(frame.heading), std::cos(frame.heading));
    const glm::vec2 side = glm::vec2(-forward.y, forward.x) * 7.0f;
    push_triangle(batch, ship_point + forward * 11.0f, ship_point + side - forward * 6.0f,
                  ship_point - side - forward * 6.0f, ui::tokens::VELLUM);
    if (glm::length(frame.velocity) > 1.0f) {
        push_line(batch, ship_point, ship_point + frame.velocity * 34.0f, 1.0f,
                  with_alpha(ui::tokens::VELLUM, 0.7f));
    }

    // The chart's own chrome sits *below* the flight HUD's top-left cluster rather than on top of
    // it: the HUD is the persistent instrument, the chart is what it is showing. The close hint is
    // the HUD's own `sector chart  M` line, so it is not repeated here.
    const float inset = ui::tokens::safe_inset(width, height);
    push_text(batch, "Sector chart", ui::tokens::PX_25, {inset, inset + 92.0f}, TextAlign::Left,
              ui::tokens::VELLUM);
    push_text(batch, "Nereid recovery zone  5.2 x 4.2 km", ui::tokens::LABEL,
              {inset, inset + 124.0f}, TextAlign::Left, ui::tokens::VELLUM_DIM, TextFace::Label);
}

void build_help(UIBatch &batch, float width, float height) {
    push_rect(batch, {0.0f, 0.0f}, {width, height}, with_alpha(ui::tokens::PLATE, 0.88f));
    const float left = 120.0f;
    float y = 90.0f;
    push_text(batch, "Flight manual", 32.0f, {left, y}, TextAlign::Left, ui::tokens::VELLUM);
    y += 52.0f;
    push_text(batch, "Space doesn't have brakes.", 15.0f, {left, y}, TextAlign::Left, ui::tokens::NAV,
              TextFace::Label);
    y += 34.0f;

    // Rows come straight from the binding table: the manual cannot drift from the keys.
    for (int i = 0; i < BINDING_COUNT;) {
        const Binding &first = BINDINGS[i];
        char keys[96];
        std::snprintf(keys, sizeof keys, "%s", first.keys);
        const char *note = first.note;
        int next = i + 1;
        while (next < BINDING_COUNT && SDL_strcmp(BINDINGS[next].effect, first.effect) == 0) {
            std::snprintf(keys + SDL_strlen(keys), sizeof keys - SDL_strlen(keys), " / %s",
                          BINDINGS[next].keys);
            if ((!note || !*note) && BINDINGS[next].note && *BINDINGS[next].note) {
                note = BINDINGS[next].note;
            }
            ++next;
        }
        push_text(batch, keys, 13.0f, {left, y}, TextAlign::Left, ui::tokens::VELLUM);
        push_text(batch, first.effect, 15.0f, {left + 190.0f, y - 2.0f}, TextAlign::Left,
                  ui::tokens::VELLUM, TextFace::Label);
        if (note && *note) {
            push_text(batch, note, 12.0f, {left + 470.0f, y}, TextAlign::Left, ui::tokens::VELLUM_DIM,
                      TextFace::Label);
        }
        y += 26.0f;
        i = next;
    }
    push_text(batch, "H or Esc to close", 13.0f, {left, y + 20.0f}, TextAlign::Left, ui::tokens::VELLUM_DIM,
              TextFace::Label);
}

void draw_toasts(UIBatch &batch, const std::vector<Toast> &toasts, double now, float x, float y) {
    for (const Toast &toast : toasts) {
        if (toast.until < now) continue;
        push_text(batch, toast.text.c_str(), 15.0f, {x, y}, TextAlign::Left, ui::tokens::ETCH,
                  TextFace::Label);
        y += 22.0f;
    }
}

}  // namespace opra::ui
