#include "ui/title.h"

#include "ui/tokens.h"

namespace opra::ui {
namespace {

// Rows: begin, settings, manual, quit. Continue and new contract both removed (plan 06 §4.1, T-3).
const char *const kEntries[] = {"begin", "settings", "manual", "quit"};
constexpr int kEntryCount = 4;

}  // namespace

TitleAction build_title(Context &ui, const Rect &screen, const TitleFrame &frame, int &selected) {
    const float inset = tokens::safe_inset(screen.w, screen.h);
    const float left = inset;
    // The plate sits a little above centre: the orrery's own centre is the star, and the plate must
    // not cover it. It is a narrow left column (plan 06 §4.1).
    float y = screen.h * 0.5f - 150.0f;
    const float reveal = clamp01(frame.reveal);

    // Narrow left plate: 400 px wide, keeping the star visible in the centre.
    const Rect panel_rect{left - tokens::SPACE[4], y - tokens::SPACE[4], 400.0f, 340.0f};
    ui.panel(panel_rect, with_alpha(tokens::PLATE, 0.66f * reveal));
    if (ui.batch()) {
        push_rect(*ui.batch(), {panel_rect.x, panel_rect.y}, {1.0f, panel_rect.h},
                  with_alpha(tokens::VELLUM_RULE, 0.28f * reveal));
    }

    // The wordmark is the one place the display face is the treatment, so it is spaced by hand.
    ui.label({left, y, 360.0f, tokens::PX_50}, "O P R A", tokens::DISPLAY_TITLE, TextAlign::Left,
             with_alpha(tokens::VELLUM, reveal), TextFace::Display);
    y += tokens::PX_50 + tokens::SPACE[2];
    ui.rule({left, y, 340.0f, 1.0f}, with_alpha(tokens::VELLUM, tokens::RULE * reveal));
    y += tokens::SPACE[3];

    ui.label({left, y, 360.0f, tokens::PX_13}, "NEREID RECOVERY SERVICE", tokens::LABEL,
             TextAlign::Left, with_alpha(tokens::VELLUM_DIM, reveal), TextFace::Label);
    y += tokens::SPACE[2] + tokens::PX_13;
    ui.label({left, y, 360.0f, tokens::PX_13}, "Wayfarer Station · Kestrel-class licence, third renewal",
             tokens::LABEL, TextAlign::Left, with_alpha(tokens::VELLUM, 0.40f * reveal),
             TextFace::Label);
    y += tokens::SPACE[4];

    // The list: begin, settings, manual, quit. No fabricated session line (T-3).
    TitleAction chosen = TitleAction::None;
    for (int i = 0; i < kEntryCount; ++i) {
        const Rect row{left, y, 340.0f, tokens::PX_25 + tokens::SPACE[1]};
        if (ui.row(kEntries[i], row, kEntries[i], i, tokens::VELLUM)) {
            chosen = static_cast<TitleAction>(i + 1);
            selected = i;
        }
        y += row.h + tokens::SPACE[1];
    }
    return chosen;
}

}  // namespace opra::ui
