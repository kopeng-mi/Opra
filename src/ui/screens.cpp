#include "ui/screens.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

#include "game/bindings.h"
#include "game/scene.h"
#include "hud/hud.h"
#include "render/camera.h"
#include "render/gltf.h"
#include "render/orrery.h"
#include "render/scene.h"

namespace opra::ui {

void build_chart(UIBatch &batch, const ChartFrame &frame, float width, float height,
                ChartState *state) {
    // Full-bleed surface (plan 06 §4.5)
    push_rect(batch, {0.0f, 0.0f}, {width, height}, with_alpha(ui::tokens::PLATE, 0.94f));

    const float margin = 90.0f;
    const float span_x = std::max(100.0f, frame.bounds_max.x - frame.bounds_min.x);
    const float span_y = std::max(100.0f, frame.bounds_max.y - frame.bounds_min.y);
    const float base_scale = std::min((width - margin * 2.0f) / span_x,
                                      (height - margin * 2.0f - 60.0f) / span_y);
    const float scale = base_scale * (state ? state->zoom : 1.0f);
    const glm::vec2 pan = state ? state->pan : glm::vec2(0.0f);
    const glm::vec2 origin(width * 0.5f + pan.x, height * 0.5f + 20.0f + pan.y);
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
        push_disc(batch, to_screen(rock.at), std::max(0.8f, (0.9f + rock.radius * 0.05f) * (state ? state->zoom : 1.0f)),
                  with_alpha(ui::tokens::VELLUM_DIM, 0.42f));
    }
    for (const ChartFrame::Dot &fragment : frame.fragments) {
        push_disc(batch, to_screen(fragment.at), 1.2f, with_alpha(ui::tokens::VELLUM_DIM, 0.5f));
    }
    for (const ChartFrame::Dot &chunk : frame.ore) {
        push_disc(batch, to_screen(chunk.at), 1.0f, with_alpha(ui::tokens::NAV, 0.8f));
    }
    for (size_t i = 0; i < frame.marks.size(); ++i) {
        const ChartFrame::Mark &mark = frame.marks[i];
        const glm::vec2 point = to_screen(mark.at);
        const bool selected = state && (state->selected_mark == static_cast<int>(i));
        if (selected) {
            push_arc(batch, point, 18.0f, 0.0f, 6.2831853f, 1.5f, ui::tokens::DRIVE);
        }
        switch (mark.kind) {
            case ChartFrame::Mark::Kind::Cargo:
                push_rect(batch, {point.x - 3.0f, point.y - 3.0f}, {6.0f, 6.0f}, ui::tokens::NAV);
                push_text(batch, mark.text.c_str(), 12.0f, {point.x + 10.0f, point.y - 8.0f},
                          TextAlign::Left, selected ? ui::tokens::DRIVE : ui::tokens::NAV, TextFace::Label);
                break;
            case ChartFrame::Mark::Kind::Station:
                push_arc(batch, point, 9.0f, 0.0f, 6.2831853f, 1.0f, ui::tokens::NAV);
                push_text(batch, mark.text.c_str(), 13.0f, {point.x + 14.0f, point.y - 9.0f},
                          TextAlign::Left, selected ? ui::tokens::DRIVE : ui::tokens::VELLUM, TextFace::Label);
                break;
            case ChartFrame::Mark::Kind::Relay:
                push_disc(batch, point, 3.0f, ui::tokens::NAV);
                push_text(batch, mark.text.c_str(), 12.0f, {point.x + 9.0f, point.y + 4.0f},
                          TextAlign::Left, selected ? ui::tokens::DRIVE : ui::tokens::VELLUM_DIM, TextFace::Label);
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

    const float inset = ui::tokens::safe_inset(width, height);
    push_text(batch, "Sector chart", ui::tokens::PX_25, {inset, inset + 20.0f}, TextAlign::Left,
              ui::tokens::VELLUM);
    push_text(batch, "Nereid recovery zone  5.2 x 4.2 km", ui::tokens::LABEL,
              {inset, inset + 52.0f}, TextAlign::Left, ui::tokens::VELLUM_DIM, TextFace::Label);
    push_text(batch, "drag to pan   wheel to zoom   click marker to target   N plans transfer   M or Esc closes",
              ui::tokens::LABEL, {inset, height - inset - 16.0f}, TextAlign::Left, ui::tokens::VELLUM_DIM,
              TextFace::Label);

    if (state && state->selected_mark >= 0 && state->selected_mark < static_cast<int>(frame.marks.size())) {
        char sel_buf[128];
        std::snprintf(sel_buf, sizeof sel_buf, "Target: %s  —  N to plan transfer",
                      frame.marks[static_cast<size_t>(state->selected_mark)].text.c_str());
        push_text(batch, sel_buf, 15.0f, {width - inset, height - inset - 16.0f}, TextAlign::Right,
                  ui::tokens::DRIVE, TextFace::Label);
    }
}

void build_help(UIBatch &batch, float width, float height, const Input *input,
                ManualState *state) {
    // Suppresses the flight HUD; draws its own surface (plan 06 §4.6, fixes T-4)
    push_rect(batch, {0.0f, 0.0f}, {width, height}, with_alpha(ui::tokens::PLATE, 0.94f));

    const float inset = ui::tokens::safe_inset(width, height);
    float y = inset + 16.0f;
    push_text(batch, "Flight manual", 32.0f, {inset, y}, TextAlign::Left, ui::tokens::VELLUM);
    push_text(batch, "Space doesn't have brakes.", 15.0f, {inset, y + 42.0f}, TextAlign::Left, ui::tokens::NAV,
              TextFace::Label);

    // Typing filter display (plan 06 §4.6)
    if (state && !state->filter.empty()) {
        char f_buf[64];
        std::snprintf(f_buf, sizeof f_buf, "filter: \"%s\"  (Backspace to clear)", state->filter.c_str());
        push_text(batch, f_buf, 14.0f, {width - inset, y + 42.0f}, TextAlign::Right, ui::tokens::DRIVE,
                  TextFace::Label);
    }

    // Collect deduplicated binding rows
    struct ManualRow {
        std::string keys;
        const char *effect;
        const char *note;
        std::vector<SDL_Scancode> codes;
    };
    std::vector<ManualRow> rows;

    for (int i = 0; i < BINDING_COUNT;) {
        const Binding &first = BINDINGS[i];
        char keys[96];
        std::snprintf(keys, sizeof keys, "%s", first.keys);
        const char *note = first.note;
        std::vector<SDL_Scancode> codes;
        if (first.primary != SDL_SCANCODE_UNKNOWN) codes.push_back(first.primary);
        if (first.secondary != SDL_SCANCODE_UNKNOWN) codes.push_back(first.secondary);

        int next = i + 1;
        while (next < BINDING_COUNT && SDL_strcmp(BINDINGS[next].effect, first.effect) == 0) {
            std::snprintf(keys + SDL_strlen(keys), sizeof keys - SDL_strlen(keys), " / %s",
                          BINDINGS[next].keys);
            if ((!note || !*note) && BINDINGS[next].note && *BINDINGS[next].note) {
                note = BINDINGS[next].note;
            }
            if (BINDINGS[next].primary != SDL_SCANCODE_UNKNOWN) codes.push_back(BINDINGS[next].primary);
            if (BINDINGS[next].secondary != SDL_SCANCODE_UNKNOWN) codes.push_back(BINDINGS[next].secondary);
            ++next;
        }

        bool match = true;
        if (state && !state->filter.empty()) {
            std::string s_keys = keys;
            std::string s_eff = first.effect;
            std::string s_filt = state->filter;
            auto to_lower = [](std::string s) {
                for (char &c : s) c = static_cast<char>(std::tolower(c));
                return s;
            };
            s_keys = to_lower(s_keys);
            s_eff = to_lower(s_eff);
            s_filt = to_lower(s_filt);
            match = (s_keys.find(s_filt) != std::string::npos) || (s_eff.find(s_filt) != std::string::npos);
        }

        if (match) {
            rows.push_back({keys, first.effect, note, codes});
        }
        i = next;
    }

    // Paginated into columns that fit the viewport (plan 06 §4.6)
    const float start_y = y + 76.0f;
    const float row_h = 26.0f;
    const float bottom_y = height - inset - 40.0f;
    const int rows_per_col = std::max(5, static_cast<int>((bottom_y - start_y) / row_h));
    const int num_cols = width >= 1200.0f ? 2 : 1;
    const int rows_per_page = rows_per_col * num_cols;
    const int total_pages = std::max(1, static_cast<int>((rows.size() + rows_per_page - 1) / rows_per_page));

    int current_page = state ? std::clamp(state->page, 0, total_pages - 1) : 0;
    if (state) state->page = current_page;

    const size_t page_start = static_cast<size_t>(current_page * rows_per_page);
    const size_t page_end = std::min(rows.size(), page_start + static_cast<size_t>(rows_per_page));

    const float col_w = (width - inset * 2.0f - (num_cols - 1) * 40.0f) / static_cast<float>(num_cols);

    for (size_t idx = page_start; idx < page_end; ++idx) {
        const size_t local_idx = idx - page_start;
        const int c = static_cast<int>(local_idx / rows_per_col);
        const int r = static_cast<int>(local_idx % rows_per_col);

        const float col_x = inset + static_cast<float>(c) * (col_w + 40.0f);
        const float cur_y = start_y + static_cast<float>(r) * row_h;

        const auto &row_item = rows[idx];

        // Interactive live highlighting: the key you press highlights its row (plan 06 §4.6)
        bool key_held = false;
        if (input) {
            for (SDL_Scancode code : row_item.codes) {
                if (input->held(code)) {
                    key_held = true;
                    break;
                }
            }
        }

        if (key_held) {
            push_rect(batch, {col_x - 4.0f, cur_y - 2.0f}, {col_w, row_h - 2.0f},
                      with_alpha(tokens::NAV, 0.35f));
        }

        const glm::vec4 key_col = key_held ? tokens::DRIVE : tokens::VELLUM;
        const glm::vec4 eff_col = key_held ? tokens::DRIVE : tokens::VELLUM;

        push_text(batch, row_item.keys.c_str(), 13.0f, {col_x, cur_y}, TextAlign::Left, key_col);
        push_text(batch, row_item.effect, 14.0f, {col_x + 180.0f, cur_y - 1.0f}, TextAlign::Left,
                  eff_col, TextFace::Label);
        if (row_item.note && *row_item.note && col_w > 450.0f) {
            push_text(batch, row_item.note, 12.0f, {col_x + 420.0f, cur_y}, TextAlign::Left,
                      tokens::VELLUM_DIM, TextFace::Label);
        }
    }

    // Page indicator and footer
    char page_buf[48];
    std::snprintf(page_buf, sizeof page_buf, "page %d of %d", current_page + 1, total_pages);
    push_text(batch, page_buf, 13.0f, {width * 0.5f, height - inset - 16.0f}, TextAlign::Center,
              tokens::VELLUM_DIM, TextFace::Label);
    push_text(batch, "H or Esc to close   type to filter", 13.0f, {inset, height - inset - 16.0f},
              TextAlign::Left, tokens::VELLUM_DIM, TextFace::Label);
}

void draw_toasts(UIBatch &batch, const std::vector<Toast> &toasts, double now, float x, float y) {
    for (const Toast &toast : toasts) {
        if (toast.until < now) continue;
        const float alpha = clamp01(static_cast<float>((toast.until - now) / 0.4));
        const std::string text = (toast.kind == ToastKind::Warning ? "! " : "· ") + toast.text;
        push_text(batch, text.c_str(), 15.0f, {x, y}, TextAlign::Left, with_alpha(ui::tokens::ETCH, alpha),
                  TextFace::Label);
        y += 22.0f;
    }
}

void build_ephemeris(UIBatch &batch, const orrery::Frame &frame, const Rect &at, float alpha) {
    if (alpha <= 0.001f || frame.bodies.empty()) return;
    const glm::vec2 center = at.centre();
    const float fr = orrery::field_radius(frame);
    if (fr <= 0.0f) return;
    const float px_per_m = std::min(at.w, at.h) * 0.42f / fr;
    const glm::vec4 ink = with_alpha(tokens::VELLUM_RULE, alpha);

    for (const auto &body : frame.bodies) {
        if (body.elements.a > 0.0) {
            std::vector<glm::dvec2> points;
            orrery::sample_ring(body.elements, 96, points);
            for (size_t i = 0; i + 1 < points.size(); i += 2) {
                const glm::vec2 p0 = center + glm::vec2(points[i].x, -points[i].y) * px_per_m;
                const glm::vec2 p1 = center + glm::vec2(points[i + 1].x, -points[i + 1].y) * px_per_m;
                push_line(batch, p0, p1, 1.0f, ink);
            }
            for (int t = 0; t < 4; ++t) {
                const size_t idx = static_cast<size_t>(t * 24);
                if (idx < points.size()) {
                    const glm::vec2 p = center + glm::vec2(points[idx].x, -points[idx].y) * px_per_m;
                    const glm::vec2 dir = (glm::length(p - center) > 1e-4f)
                                              ? glm::normalize(p - center)
                                              : glm::vec2(1.0f, 0.0f);
                    push_line(batch, p - dir * 2.0f, p + dir * 2.0f, 1.0f, ink);
                }
            }
        }
        const glm::vec2 b_pos = center + glm::vec2(body.position.x, -body.position.y) * px_per_m;
        push_disc(batch, b_pos, 2.0f, with_alpha(tokens::VELLUM, alpha));
        if (body.name == "Wayfarer") {
            push_text(batch, body.name.c_str(), 10.0f, {b_pos.x + 6.0f, b_pos.y - 5.0f},
                      TextAlign::Left, with_alpha(tokens::VELLUM_DIM, alpha), TextFace::Label);
        }
    }

    push_disc(batch, center, 3.0f, with_alpha(tokens::DRIVE, alpha));
    push_arc(batch, center, 7.0f, 0.0f, 6.2831853f, 1.0f, with_alpha(tokens::DRIVE, 0.2f * alpha));
}

ContractResult build_contract(Context &ui, const Rect &screen, ContractState &state,
                             const ChartFrame &frame) {
    ContractResult result;
    UIBatch &batch = *ui.batch();

    const float right_w = 360.0f;
    const Rect chart_rect{0.0f, 0.0f, screen.w - right_w, screen.h};
    const Rect info_rect{screen.w - right_w, 0.0f, right_w, screen.h};

    // 1. Chart area: live pan & zoom (plan 06 §4.2)
    push_rect(batch, {chart_rect.x, chart_rect.y}, {chart_rect.w, chart_rect.h},
              with_alpha(tokens::PLATE, 0.90f));

    const float margin = 60.0f;
    const float span_x = std::max(100.0f, frame.bounds_max.x - frame.bounds_min.x);
    const float span_y = std::max(100.0f, frame.bounds_max.y - frame.bounds_min.y);
    const float base_scale = std::min((chart_rect.w - margin * 2.0f) / span_x,
                                      (chart_rect.h - margin * 2.0f) / span_y);
    const float scale = base_scale * state.zoom;
    const glm::vec2 origin(chart_rect.w * 0.5f + state.pan.x, chart_rect.h * 0.5f + state.pan.y);

    const auto to_screen = [&](const glm::vec2 &at) {
        return glm::vec2(origin.x + at.x * scale, origin.y - at.y * scale);
    };

    // Bounds
    const glm::vec2 top_left = to_screen({frame.bounds_min.x, frame.bounds_max.y});
    const glm::vec2 bottom_right = to_screen({frame.bounds_max.x, frame.bounds_min.y});
    push_rect(batch, top_left, {bottom_right.x - top_left.x, 1.0f}, with_alpha(tokens::VELLUM_DIM, 0.35f));
    push_rect(batch, {top_left.x, bottom_right.y}, {bottom_right.x - top_left.x, 1.0f}, with_alpha(tokens::VELLUM_DIM, 0.35f));
    push_rect(batch, top_left, {1.0f, bottom_right.y - top_left.y}, with_alpha(tokens::VELLUM_DIM, 0.35f));
    push_rect(batch, {bottom_right.x, top_left.y}, {1.0f, bottom_right.y - top_left.y}, with_alpha(tokens::VELLUM_DIM, 0.35f));

    // Hazards overlay: belt density
    if (state.hazards) {
        for (float y_band = -1800.0f; y_band <= 1800.0f; y_band += 300.0f) {
            const float density_alpha = 0.08f + 0.10f * std::cos(y_band * 0.003f);
            const glm::vec2 p0 = to_screen({frame.bounds_min.x, y_band});
            const glm::vec2 p1 = to_screen({frame.bounds_max.x, y_band + 200.0f});
            push_rect(batch, {p0.x, std::min(p0.y, p1.y)}, {p1.x - p0.x, std::abs(p1.y - p0.y)},
                      with_alpha(tokens::THREAT, density_alpha));
        }
    }

    // Rocks and ore
    for (const ChartFrame::Dot &rock : frame.rocks) {
        const glm::vec2 pt = to_screen(rock.at);
        if (pt.x < -10.0f || pt.x > chart_rect.w + 10.0f || pt.y < -10.0f || pt.y > chart_rect.h + 10.0f) continue;
        push_disc(batch, pt, std::max(0.8f, 0.9f + rock.radius * 0.05f * state.zoom),
                  with_alpha(state.hazards ? tokens::THREAT : tokens::VELLUM_DIM, state.hazards ? 0.6f : 0.42f));
    }
    for (const ChartFrame::Dot &chunk : frame.ore) {
        const glm::vec2 pt = to_screen(chunk.at);
        if (pt.x < -10.0f || pt.x > chart_rect.w + 10.0f || pt.y < -10.0f || pt.y > chart_rect.h + 10.0f) continue;
        push_disc(batch, pt, 1.5f, with_alpha(tokens::NAV, 0.85f));
    }

    // Markers
    for (const ChartFrame::Mark &mark : frame.marks) {
        const glm::vec2 pt = to_screen(mark.at);
        if (pt.x < -20.0f || pt.x > chart_rect.w + 20.0f || pt.y < -20.0f || pt.y > chart_rect.h + 20.0f) continue;
        switch (mark.kind) {
            case ChartFrame::Mark::Kind::Cargo:
                push_rect(batch, {pt.x - 4.0f, pt.y - 4.0f}, {8.0f, 8.0f}, tokens::NAV);
                push_arc(batch, pt, 10.0f, 0.0f, 6.2831853f, 1.2f, with_alpha(tokens::NAV, 0.6f));
                push_text(batch, mark.text.c_str(), 13.0f, {pt.x + 14.0f, pt.y - 8.0f},
                          TextAlign::Left, tokens::NAV, TextFace::Label);
                break;
            case ChartFrame::Mark::Kind::Station:
                push_arc(batch, pt, 12.0f, 0.0f, 6.2831853f, 1.5f, tokens::VELLUM);
                push_text(batch, mark.text.c_str(), 13.0f, {pt.x + 16.0f, pt.y - 8.0f},
                          TextAlign::Left, tokens::VELLUM, TextFace::Label);
                break;
            case ChartFrame::Mark::Kind::Relay:
                push_disc(batch, pt, 4.0f, tokens::NAV);
                push_text(batch, mark.text.c_str(), 12.0f, {pt.x + 12.0f, pt.y - 6.0f},
                          TextAlign::Left, tokens::VELLUM_DIM, TextFace::Label);
                break;
            case ChartFrame::Mark::Kind::Derelict:
                push_disc(batch, pt, 4.0f, tokens::THREAT);
                push_text(batch, mark.text.c_str(), 12.0f, {pt.x + 12.0f, pt.y - 6.0f},
                          TextAlign::Left, with_alpha(tokens::THREAT, 0.9f), TextFace::Label);
                break;
        }
    }

    const float inset = tokens::safe_inset(screen.w, screen.h);
    push_text(batch, "Zone chart — Nereid Recovery Zone", tokens::PX_25, {inset, inset + 20.0f},
              TextAlign::Left, tokens::VELLUM);
    push_text(batch, "drag to pan   wheel to zoom   inspect hazards and briefing", tokens::LABEL,
              {inset, inset + 52.0f}, TextAlign::Left, tokens::VELLUM_DIM, TextFace::Label);

    // 2. Right panel: Contract Briefing (plan 06 §4.2)
    ui.push(info_rect);
    push_rect(batch, {info_rect.x, info_rect.y}, {info_rect.w, info_rect.h},
              with_alpha(tokens::FIELD, 0.92f));
    ui.push(ui.inset(info_rect, 24.0f));

    ui.label(ui.cut_top(40.0f), "Contract", 32.0f, TextAlign::Left, tokens::ETCH);
    ui.label(ui.cut_top(22.0f), "SR-084: Resolve and recover", 14.0f, TextAlign::Left,
             tokens::NAV, TextFace::Label);
    ui.cut_top(14.0f);
    ui.rule(ui.cut_top(1.0f), with_alpha(tokens::ETCH, 0.2f));
    ui.cut_top(14.0f);

    const auto spec_row = [&](const char *name, const char *val, const glm::vec4 &col = tokens::ETCH) {
        const Rect row = ui.cut_top(28.0f);
        ui.label({row.x, row.y, 100.0f, row.h}, name, 13.0f, TextAlign::Left, tokens::ETCH_DIM, TextFace::Label);
        ui.label({row.x + 110.0f, row.y, row.w - 110.0f, row.h}, val, 15.0f, TextAlign::Left, col, TextFace::Label);
    };

    spec_row("payout", "42 000 cr", tokens::NAV);
    spec_row("deadline", "30 d 00 h");
    spec_row("licence", "Kestrel III");
    spec_row("zone", "Nereid Belt");
    spec_row("target", "Cargo Pod C-12");

    ui.cut_top(14.0f);
    ui.rule(ui.cut_top(1.0f), with_alpha(tokens::ETCH, 0.2f));
    ui.cut_top(14.0f);

    ui.section(ui.cut_top(26.0f), "survey overlays");
    ui.cut_top(6.0f);
    ui.toggle("cnt.hazards", ui.cut_top(36.0f), "belt density (hazards)", state.hazards);

    ui.cut_top(20.0f);
    ui.label(ui.cut_top(24.0f), "briefing notes", 13.0f, TextAlign::Left, tokens::ETCH_DIM, TextFace::Label);
    ui.label(ui.cut_top(48.0f),
             "Displaced container detected in high-density belt corridor. Secure payload and return to Wayfarer Station.",
             12.0f, TextAlign::Left, tokens::ETCH, TextFace::Label);

    // Buttons at bottom: Back returns to Title, Accept advances to Shipyard
    ui.cut_bottom(20.0f);
    const Rect btn_row = ui.cut_bottom(44.0f);
    const Rect back_rect = ui.column(btn_row, 0, 2, 16.0f);
    const Rect accept_rect = ui.column(btn_row, 1, 2, 16.0f);

    if (ui.button("cnt.back", back_rect, "Back")) result.back = true;
    if (ui.button("cnt.accept", accept_rect, "Accept")) result.accept = true;

    ui.pop();
    ui.pop();

    return result;
}

struct CatalogueItem {
    const char *id;
    const char *label;
    const char *mass_label;
    bool axial;
};

static const std::vector<CatalogueItem> kCatalogue[] = {
    // 0: command / nose
    {
        {"nose_hammerhead", "nose_hammerhead", "7.5t", true},
        {"nose_pushbow", "nose_pushbow", "12.0t", true},
        {"nose_stealth_needle", "nose_stealth", "2.5t", true},
    },
    // 1: hull sections
    {
        {"section_combat_a", "section_combat_a", "9.0t", true},
        {"section_delta_fore", "section_delta_fore", "4.5t", true},
        {"section_freight", "section_freight", "12.6t", true},
        {"section_stealth_combat", "section_stealth", "6.0t", true},
    },
    // 2: propellant
    {
        {"section_tank_saddle", "section_tank_saddle", "3.0t", true},
        {"section_reactor", "section_reactor", "9.0t", true},
        {"tank_drum_1", "tank_drum_1", "1.5t", true},
    },
    // 3: utility / thermal
    {
        {"section_machinery", "section_machinery", "6.5t", true},
        {"section_radiator_wing", "section_radiator_wing", "4.0t", true},
        {"section_delta_aft", "section_delta_aft", "4.0t", true},
    },
    // 4: propulsion
    {
        {"drive_twin_torch", "drive_twin_torch", "11.5t", true},
        {"drive_quad_block", "drive_quad_block", "16.5t", true},
        {"drive_stealth_twin", "drive_stealth_twin", "8.5t", true},
    },
    // 5: pods
    {
        {"section_tank_saddle", "tank_saddle (pod)", "3.0t", false},
        {"section_radiator_wing", "radiator (pod)", "4.0t", false},
        {"tank_drum_1", "tank_drum (pod)", "1.5t", false},
        {"section_combat_a", "combat_a (pod)", "9.0t", false},
        {"section_machinery", "machinery (pod)", "6.5t", false},
    },
};

static const char *const kCategories[] = {
    "command / nose",
    "hull sections",
    "propellant",
    "utility / thermal",
    "propulsion",
    "pods",
};
constexpr int kCatCount = 6;

ShipyardResult build_shipyard(Context &ui, const Rect &screen, ShipyardState &state) {
    ShipyardResult result;
    UIBatch &batch = *ui.batch();

    const float left_w = 260.0f;
    const float right_w = 300.0f;

    // 1. Left column: CATALOGUE (PLAN-08 §8)
    const Rect left_panel{0.0f, 0.0f, left_w, screen.h};
    ui.panel(left_panel, with_alpha(tokens::FIELD, 0.92f));
    ui.push(left_panel);
    ui.push(ui.inset(left_panel, 14.0f));

    ui.label(ui.cut_top(32.0f), "CATALOGUE", 22.0f, TextAlign::Left, tokens::ETCH);
    ui.cut_top(4.0f);

    for (int i = 0; i < kCatCount; ++i) {
        char label[48];
        std::snprintf(label, sizeof label, " %s  >", kCategories[i]);
        const bool active = (state.category == i);
        if (active) {
            ui.panel({left_panel.x + 8.0f, ui.area().y + 1.0f, left_w - 16.0f, 24.0f},
                     with_alpha(tokens::NAV, 0.25f));
        }
        if (ui.row(kCategories[i], ui.cut_top(26.0f), label, i, active ? tokens::NAV : tokens::ETCH)) {
            state.category = i;
            state.held = -1;
        }
    }

    ui.cut_top(8.0f);
    ui.rule(ui.cut_top(1.0f), with_alpha(tokens::ETCH, 0.2f));
    ui.cut_top(8.0f);

    const auto &cat_items = kCatalogue[std::clamp(state.category, 0, kCatCount - 1)];
    std::string hovered_cat_part;
    for (size_t j = 0; j < cat_items.size(); ++j) {
        const auto &item = cat_items[j];
        const bool is_held = (state.held == static_cast<int>(j));
        if (is_held) {
            ui.panel({left_panel.x + 8.0f, ui.area().y + 1.0f, left_w - 16.0f, 26.0f},
                     with_alpha(tokens::DRIVE, 0.3f));
        }
        const Rect row_rect = ui.cut_top(26.0f);
        if (row_rect.contains(ui.pointer().at)) {
            hovered_cat_part = item.id;
        }
        char row_txt[64];
        std::snprintf(row_txt, sizeof row_txt, "%-16s %6s", item.label, item.mass_label);
        if (ui.row(item.id, row_rect, row_txt, static_cast<int>(j),
                   is_held ? tokens::DRIVE : tokens::ETCH_DIM)) {
            state.held = is_held ? -1 : static_cast<int>(j);
        }
    }
    if (state.held == -1) {
        state.preview_part = hovered_cat_part;
    } else {
        state.preview_part.clear();
    }

    // Bottom toggles
    ui.cut_bottom(12.0f);
    const bool held_is_axial = (state.held >= 0 && cat_items[static_cast<size_t>(state.held)].axial);
    if (held_is_axial) {
        state.mirror = false;
        ui.label(ui.cut_bottom(28.0f), "mirror radial (axial part)", 13.0f, TextAlign::Left,
                 tokens::ETCH_DIM, TextFace::Label);
    } else {
        ui.toggle("sh.mirror", ui.cut_bottom(28.0f), "mirror radial", state.mirror);
    }
    ui.toggle("sh.flanges", ui.cut_bottom(28.0f), "show flanges", state.show_flanges);

    ui.pop();
    ui.pop();

    // 2. Precompute candidate ghost placement and ring hover (PLAN-09 U6)
    const Camera cam = shipyard_camera(ModelSet{}, state, static_cast<uint32_t>(screen.w), static_cast<uint32_t>(screen.h));
    const glm::mat4 vp = view_projection(cam);
    const float px_per_m = static_cast<float>(screen.h) / (2.0f * static_cast<float>(cam.half_height));
    const float ring_r = std::max(6.0f, 1.25f * px_per_m);

    int best_slot = -1;
    Facing best_facing = Facing::Fore;
    bool best_ok = false;
    float best_dist = 26.0f;

    if (state.held >= 0) {
        const auto &item = cat_items[static_cast<size_t>(state.held)];
        const std::vector<Facing> offered_facings = item.axial
            ? std::vector<Facing>{Facing::Fore, Facing::Aft}
            : std::vector<Facing>{Facing::Starboard, Facing::Port, Facing::Dorsal, Facing::Ventral};

        for (int s = 0; s < state.design.slots; ++s) {
            for (Facing f : offered_facings) {
                Placement p_cand;
                p_cand.part = item.id;
                p_cand.slot = s;
                p_cand.facing = f;
                p_cand.span = 1;
                p_cand.axial = item.axial;
                bool ok = can_mount(state.design.spine, state.design.placements, p_cand);
                if (state.mirror && !item.axial) {
                    Placement p_mir = p_cand;
                    p_mir.facing = opposite_facing(f);
                    if (!can_mount(state.design.spine, state.design.placements, p_mir)) ok = false;
                }

                Mount m = mount_transform(state.design.spine, p_cand);
                glm::vec4 clip = vp * glm::vec4(m.pos, 1.0);
                if (clip.w > 0.0f) {
                    float px = ((clip.x / clip.w) * 0.5f + 0.5f) * screen.w;
                    float py = (1.0f - ((clip.y / clip.w) * 0.5f + 0.5f)) * screen.h;
                    if (px > left_w && px < screen.w - right_w) {
                        float d = std::hypot(px - ui.pointer().at.x, py - ui.pointer().at.y);
                        if (d < best_dist) {
                            best_dist = d;
                            best_slot = s;
                            best_facing = f;
                            best_ok = ok;
                        }
                    }
                }
            }
        }

        if (best_slot >= 0 && best_ok) {
            Placement cand;
            cand.part = item.id;
            cand.slot = best_slot;
            cand.facing = best_facing;
            cand.span = 1;
            cand.axial = item.axial;
            state.ghost_placement = cand;
            if (state.mirror && !item.axial) {
                Placement mir = cand;
                mir.facing = opposite_facing(best_facing);
                state.ghost_mirror = mir;
            } else {
                state.ghost_mirror.reset();
            }
        } else {
            state.ghost_placement.reset();
            state.ghost_mirror.reset();
        }
    } else {
        state.ghost_placement.reset();
        state.ghost_mirror.reset();
    }

    // 3. Right column: DERIVED stats and deltas (PLAN-09 §3.3)
    const Rect right_panel{screen.w - right_w, 0.0f, right_w, screen.h};
    ui.panel(right_panel, with_alpha(tokens::FIELD, 0.92f));
    ui.push(right_panel);
    ui.push(ui.inset(right_panel, 14.0f));

    ui.label(ui.cut_top(32.0f), "DERIVED", 22.0f, TextAlign::Left, tokens::ETCH);
    ui.label(ui.cut_top(18.0f), state.design.name.empty() ? "Custom" : state.design.name.c_str(),
             14.0f, TextAlign::Left, tokens::NAV, TextFace::Label);
    ui.rule(ui.cut_top(1.0f), with_alpha(tokens::ETCH, 0.2f));
    ui.cut_top(8.0f);

    if (state.spec_dirty) {
        state.cached_spec = derive_spec(state.design, state.parts);
        state.spec_dirty = false;
    }

    ShipSpec preview_spec = state.cached_spec;
    bool has_ghost_spec = false;
    if (state.ghost_placement.has_value()) {
        ShipDesign preview_design = state.design;
        mount_placement(preview_design, *state.ghost_placement, state.mirror && !state.ghost_placement->axial);
        preview_spec = derive_spec(preview_design, state.parts);
        has_ghost_spec = true;
    }

    const Real M_dry = state.cached_spec.mass;
    const Real M_fuel = state.cached_spec.fuel;
    const Real thrust = state.cached_spec.thrust;
    const Real twr = calculate_twr(thrust, M_dry + M_fuel);
    const Real dv = calculate_delta_v(thrust, M_dry, M_dry + M_fuel) / 1000.0;
    const Real torque = state.cached_spec.torque;
    const Real hull = state.cached_spec.hull;
    const Real heat_load = calculate_heat_load(thrust);
    const Real thermal_margin = calculate_thermal_margin(state.cached_spec.cooling, heat_load);

    const auto format_delta = [&](Real cur, Real prev, const char *fmt) -> std::string {
        if (!has_ghost_spec) return "";
        const Real d = cur - prev;
        if (std::abs(d) < 1e-4) return "";
        char b[32];
        std::snprintf(b, sizeof b, fmt, d);
        return b;
    };

    const auto stat_row = [&](const char *label, const char *val, const std::string &delta_str, const char *unit,
                              const glm::vec4 &col = tokens::ETCH, const glm::vec4 &delta_col = tokens::DRIVE) {
        const Rect row = ui.cut_top(22.0f);
        ui.label({row.x, row.y, 85.0f, row.h}, label, 12.0f, TextAlign::Left, tokens::ETCH_DIM, TextFace::Label);
        ui.label({row.x + 85.0f, row.y, 55.0f, row.h}, val, 13.0f, TextAlign::Right, col, TextFace::Label);
        if (!delta_str.empty()) {
            ui.label({row.x + 145.0f, row.y, 75.0f, row.h}, delta_str.c_str(), 11.0f, TextAlign::Left, delta_col, TextFace::Label);
        }
        ui.label({row.x + 225.0f, row.y, 45.0f, row.h}, unit, 12.0f, TextAlign::Left, tokens::ETCH_DIM, TextFace::Label);
    };

    char buf[64];
    std::snprintf(buf, sizeof buf, "%.1f", M_dry / 1000.0);
    stat_row("DRY MASS", buf, format_delta(preview_spec.mass / 1000.0, M_dry / 1000.0, "(%+.1f)"), "t");

    std::snprintf(buf, sizeof buf, "%.1f", M_fuel / 1000.0);
    stat_row("PROPELLANT", buf, format_delta(preview_spec.fuel / 1000.0, M_fuel / 1000.0, "(%+.1f)"), "t");

    std::snprintf(buf, sizeof buf, "%.1f", (M_dry + M_fuel) / 1000.0);
    stat_row("ALL-UP", buf, format_delta((preview_spec.mass + preview_spec.fuel) / 1000.0, (M_dry + M_fuel) / 1000.0, "(%+.1f)"), "t");

    std::snprintf(buf, sizeof buf, "%.0f", thrust / 1000.0);
    stat_row("THRUST", buf, format_delta(preview_spec.thrust / 1000.0, thrust / 1000.0, "(%+.0f)"), "kN");

    std::snprintf(buf, sizeof buf, "%.2f", twr);
    stat_row("TWR @ 1g", buf, format_delta(calculate_twr(preview_spec.thrust, preview_spec.mass + preview_spec.fuel), twr, "(%+.2f)"), "—", twr < 1.0 ? tokens::THREAT : tokens::ETCH);

    if (dv > 0.0) std::snprintf(buf, sizeof buf, "%.1f", dv);
    else std::snprintf(buf, sizeof buf, "—");
    stat_row("ΔV", buf, format_delta(calculate_delta_v(preview_spec.thrust, preview_spec.mass, preview_spec.mass + preview_spec.fuel) / 1000.0, dv, "(%+.1f)"), "km/s");

    std::snprintf(buf, sizeof buf, "%.3f", torque);
    stat_row("TORQUE", buf, format_delta(preview_spec.torque, torque, "(%+.3f)"), "rad/s²");

    std::snprintf(buf, sizeof buf, "%.0f", hull);
    stat_row("HULL", buf, format_delta(preview_spec.hull, hull, "(%+.0f)"), "—");

    std::snprintf(buf, sizeof buf, "%.1f", thermal_margin);
    stat_row("THERMAL", buf, format_delta(calculate_thermal_margin(preview_spec.cooling, calculate_heat_load(preview_spec.thrust)), thermal_margin, "(%+.1f)"), "kW", thermal_margin < 0.0 ? tokens::THREAT : tokens::ETCH);

    // Status checks
    const bool check_drive = state.design_has_drive();
    const bool check_twr = twr > 0.0;
    const Vec2 com = centre_of_mass(state.design, state.parts);
    const bool check_com = std::abs(com.x) <= 0.6;
    const bool check_thermal = thermal_margin >= 0.0;
    const bool check_fuel = M_fuel > 0.0;

    ui.cut_top(8.0f);
    ui.rule(ui.cut_top(1.0f), with_alpha(tokens::ETCH, 0.2f));
    ui.cut_top(6.0f);

    auto check_line = [&](bool ok, const char *line_label) {
        const Rect row = ui.cut_top(20.0f);
        ui.label({row.x, row.y, 16.0f, row.h}, ok ? "✓" : "✗", 13.0f, TextAlign::Left,
                 ok ? tokens::NAV : tokens::THREAT, TextFace::Label);
        ui.label({row.x + 18.0f, row.y, row.w - 18.0f, row.h}, line_label, 12.0f, TextAlign::Left,
                 ok ? tokens::ETCH : tokens::THREAT, TextFace::Label);
    };

    check_line(check_drive, "drive installed");
    check_line(check_twr, "TWR over 0.0");
    check_line(check_com, "thrust line on centre");
    check_line(check_thermal, "thermal margin >= 0 kW");
    check_line(check_fuel, "propellant loaded");

    ui.pop();
    ui.pop();

    // 4. Middle area: ghost rings, turntable picking (PLAN-09 §3.4)
    if (state.held >= 0) {
        const auto &item = cat_items[static_cast<size_t>(state.held)];
        const std::vector<Facing> offered_facings = item.axial
            ? std::vector<Facing>{Facing::Fore, Facing::Aft}
            : std::vector<Facing>{Facing::Starboard, Facing::Port, Facing::Dorsal, Facing::Ventral};

        for (int s = 0; s < state.design.slots; ++s) {
            for (Facing f : offered_facings) {
                Placement p_cand;
                p_cand.part = item.id;
                p_cand.slot = s;
                p_cand.facing = f;
                p_cand.span = 1;
                p_cand.axial = item.axial;
                bool ok = can_mount(state.design.spine, state.design.placements, p_cand);
                if (state.mirror && !item.axial) {
                    Placement p_mir = p_cand;
                    p_mir.facing = opposite_facing(f);
                    if (!can_mount(state.design.spine, state.design.placements, p_mir)) ok = false;
                }

                Mount m = mount_transform(state.design.spine, p_cand);
                glm::vec4 clip = vp * glm::vec4(m.pos, 1.0);
                if (clip.w > 0.0f) {
                    float px = ((clip.x / clip.w) * 0.5f + 0.5f) * screen.w;
                    float py = (1.0f - ((clip.y / clip.w) * 0.5f + 0.5f)) * screen.h;
                    if (px > left_w && px < screen.w - right_w) {
                        const bool is_hovered = (s == best_slot && f == best_facing);
                        const float thick = is_hovered ? 2.0f : 1.0f;
                        const float a = is_hovered ? 0.95f : 0.40f;
                        const glm::vec4 ring_col = ok ? with_alpha(tokens::NAV, a) : with_alpha(tokens::THREAT, a);

                        for (int seg = 0; seg < 24; ++seg) {
                            float a0 = (seg / 24.0f) * 6.2831853f;
                            float a1 = ((seg + 1) / 24.0f) * 6.2831853f;
                            push_line(batch, {px + std::cos(a0) * ring_r, py + std::sin(a0) * ring_r},
                                             {px + std::cos(a1) * ring_r, py + std::sin(a1) * ring_r}, thick, ring_col);
                        }
                        if (is_hovered) {
                            char slot_buf[32];
                            std::snprintf(slot_buf, sizeof slot_buf, "slot %d", s);
                            push_text(batch, slot_buf, 11.0f, {px + ring_r + 6.0f, py - 6.0f}, TextAlign::Left,
                                      ok ? tokens::DRIVE : tokens::THREAT, TextFace::Label);
                        }
                    }
                }
            }
        }

        if (best_slot >= 0 && ui.pointer().pressed) {
            if (best_ok) {
                Placement new_p;
                new_p.part = item.id;
                new_p.slot = best_slot;
                new_p.facing = best_facing;
                new_p.span = 1;
                new_p.axial = item.axial;
                mount_placement(state.design, new_p, state.mirror && !item.axial);
                state.spec_dirty = true;
                state.held = -1;
                state.ghost_placement.reset();
                state.ghost_mirror.reset();
            }
        }
    } else {
        int hovered_k = -1;
        float best_occupied_dist = 26.0f;
        for (size_t k = 0; k < state.design.placements.size(); ++k) {
            const Placement &p = state.design.placements[k];
            if (p.destroyed) continue;
            Mount m = mount_transform(state.design.spine, p);
            glm::vec4 clip = vp * glm::vec4(m.pos, 1.0);
            if (clip.w > 0.0f) {
                float px = ((clip.x / clip.w) * 0.5f + 0.5f) * screen.w;
                float py = (1.0f - ((clip.y / clip.w) * 0.5f + 0.5f)) * screen.h;
                if (px > left_w && px < screen.w - right_w) {
                    float d = std::hypot(px - ui.pointer().at.x, py - ui.pointer().at.y);
                    if (d < best_occupied_dist) {
                        best_occupied_dist = d;
                        hovered_k = static_cast<int>(k);
                    }
                }
            }
        }

        state.selected = hovered_k;
        if (hovered_k >= 0) {
            const Placement &p = state.design.placements[static_cast<size_t>(hovered_k)];
            Mount m = mount_transform(state.design.spine, p);
            glm::vec4 clip = vp * glm::vec4(m.pos, 1.0);
            if (clip.w > 0.0f) {
                float px = ((clip.x / clip.w) * 0.5f + 0.5f) * screen.w;
                float py = (1.0f - ((clip.y / clip.w) * 0.5f + 0.5f)) * screen.h;
                for (int seg = 0; seg < 24; ++seg) {
                    float a0 = (seg / 24.0f) * 6.2831853f;
                    float a1 = ((seg + 1) / 24.0f) * 6.2831853f;
                    push_line(batch, {px + std::cos(a0) * (ring_r + 4.0f), py + std::sin(a0) * (ring_r + 4.0f)},
                                     {px + std::cos(a1) * (ring_r + 4.0f), py + std::sin(a1) * (ring_r + 4.0f)}, 1.5f,
                                     with_alpha(tokens::THREAT, 0.75f));
                }
                char rem_buf[96];
                std::snprintf(rem_buf, sizeof rem_buf, "%s (right-click to remove)", p.part.c_str());
                push_text(batch, rem_buf, 12.0f, {px + ring_r + 8.0f, py - 6.0f}, TextAlign::Left,
                          tokens::DRIVE, TextFace::Label);
            }
        }

        if (hovered_k >= 0 && ui.pointer().right_pressed) {
            const int grp = state.design.placements[static_cast<size_t>(hovered_k)].group;
            if (grp > 0) {
                std::vector<Placement> kept;
                for (const auto &pl : state.design.placements) {
                    if (pl.group != grp) kept.push_back(pl);
                }
                state.design.placements = std::move(kept);
            } else {
                state.design.placements.erase(state.design.placements.begin() + hovered_k);
            }
            state.spec_dirty = true;
            state.selected = -1;
        }
    }

    // 4. Bottom controls
    const float mid_w = screen.w - left_w - right_w;
    const Rect bottom_bar{left_w, screen.h - 80.0f, mid_w, 80.0f};
    ui.push(bottom_bar);
    const Rect btn_row = ui.cut_top(44.0f);
    const Rect back_rect{btn_row.x + btn_row.w * 0.5f - 160.0f, btn_row.y, 140.0f, 40.0f};
    const Rect launch_rect{btn_row.x + btn_row.w * 0.5f + 20.0f, btn_row.y, 140.0f, 40.0f};

    if (ui.button("sh.back", back_rect, "Back")) result.back = true;

    const bool all_pass = check_drive && check_twr && check_com && check_thermal && check_fuel;
    if (all_pass) {
        if (ui.button("sh.launch", launch_rect, "Launch")) result.launch = true;
    } else {
        ui.panel(launch_rect, with_alpha(tokens::FIELD, 0.5f));
        ui.label(launch_rect, "Launch", 16.0f, TextAlign::Center, tokens::ETCH_DIM);
        std::string fail_line;
        if (!check_drive) {
            fail_line = "launch refused: no forward drive: the chain needs a drive at the stern slot";
        } else if (!check_twr) {
            fail_line = "launch refused: no thrust";
        } else if (!check_com) {
            char b[128];
            std::snprintf(b, sizeof b, "launch refused: thrust line off centre by %.2f m — mirror the port pod",
                          std::abs(com.x));
            fail_line = b;
        } else if (!check_thermal) {
            char b[128];
            std::snprintf(b, sizeof b, "launch refused: heat load exceeds cooling by %.0f kW — add a radiator section",
                          -thermal_margin);
            fail_line = b;
        } else if (!check_fuel) {
            fail_line = "launch refused: no propellant";
        }
        ui.label(ui.cut_top(24.0f), fail_line.c_str(), 13.0f, TextAlign::Center, tokens::THREAT, TextFace::Label);
    }
    ui.pop();

    push_text(batch, "drag orbit · wheel zoom · click ring to place · right-click to remove · 1-4 presets",
              12.0f, {left_w + 20.0f, 24.0f}, TextAlign::Left, with_alpha(tokens::ETCH, 0.6f),
              TextFace::Label);

    return result;
}

Camera shipyard_camera(const ModelSet &models, const ShipyardState &state, uint32_t width, uint32_t height) {
    Camera out;
    float extent = 4.0f;
    if (state.held == -1 && !state.preview_part.empty() && models.store.has(state.preview_part)) {
        const ModelMeta &meta = models.store.meta(state.preview_part);
        const float dx = meta.aabb_max.x - meta.aabb_min.x;
        const float dy = meta.aabb_max.y - meta.aabb_min.y;
        extent = std::max(2.5f, std::hypot(dx, dy) * 0.75f);
    } else {
        const Real L = static_cast<Real>(state.design.slots) * state.design.pitch;
        const Real half_len = L * 0.5;
        const Real half_wid = std::max(static_cast<Real>(state.design.half_width), 2.5);
        const float diag = static_cast<float>(std::hypot(2.0 * half_len, 2.0 * half_wid));
        extent = std::max(4.0f, diag * 0.5f);
    }
    out.half_height = extent * 1.4f * state.distance;
    out.aspect = height > 0 ? static_cast<float>(width) / static_cast<float>(height) : 1.0f;
    out.up = glm::vec3(0.0f, 0.0f, 1.0f);
    const glm::vec3 forward(std::cos(state.pitch) * std::sin(state.yaw),
                            -std::cos(state.pitch) * std::cos(state.yaw),
                            std::sin(state.pitch));
    out.target = glm::vec3(0.0f);
    out.eye = forward * static_cast<float>(out.half_height / std::tan(static_cast<double>(CAMERA_FOV_Y) * 0.5));
    return out;
}

void build_shipyard_scene(SceneBuilder &scene, const ModelSet &models, const ShipyardState &state) {
    if (state.held == -1 && !state.preview_part.empty() && models.store.has(state.preview_part)) {
        const glm::quat rot = glm::angleAxis(state.preview_yaw, glm::vec3(0.0f, 0.0f, 1.0f));
        scene.add_model(models.store.model(state.preview_part), glm::vec3(0.0f), rot,
                        static_cast<float>(state.design.scale), false, 0.0f);
        return;
    }

    if (!state.design.placements.empty()) {
        std::vector<Real> jets(state.design.placements.size(), 0.0);
        add_design(scene, models, state.design, glm::vec3(0.0f), glm::quat(1, 0, 0, 0),
                   static_cast<float>(state.design.scale), 3, 0.0f, jets);
    }

    const float design_scale = static_cast<float>(state.design.scale);
    if (state.ghost_placement.has_value() && models.store.has(state.ghost_placement->part)) {
        const Mount m = mount_transform(state.design.spine, *state.ghost_placement);
        const glm::vec3 at = glm::vec3(m.pos) * design_scale;
        const glm::quat rot = glm::quat(m.rot);
        scene.add_model(models.store.model(state.ghost_placement->part), at, rot, design_scale,
                        false, 0.0f, glm::vec3(0.40f));
    }
    if (state.ghost_mirror.has_value() && models.store.has(state.ghost_mirror->part)) {
        const Mount m = mount_transform(state.design.spine, *state.ghost_mirror);
        const glm::vec3 at = glm::vec3(m.pos) * design_scale;
        const glm::quat rot = glm::quat(m.rot);
        scene.add_model(models.store.model(state.ghost_mirror->part), at, rot, design_scale,
                        false, 0.0f, glm::vec3(0.40f));
    }
}

}  // namespace opra::ui
