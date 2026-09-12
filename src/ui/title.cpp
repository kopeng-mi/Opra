#include "ui/title.h"

#include <cstdio>

#include "ui/tokens.h"

namespace opra::ui {
namespace {

const char *const kEntries[] = {"continue", "new contract", "settings", "manual", "quit"};
constexpr int kEntryCount = 5;

/** The session line: a time, and where the ship is. A fact, not an invitation. */
void session_line(char *out, size_t size, const TitleFrame &frame) {
    char clock[32];
    const int total = static_cast<int>(frame.sessionSeconds);
    std::snprintf(clock, sizeof clock, "%02d:%02d:%02d", total / 3600, (total / 60) % 60,
                  total % 60);
    if (frame.docked && frame.dockName && *frame.dockName) {
        std::snprintf(out, size, "T+ %s, docked at %s", clock, frame.dockName);
    } else {
        std::snprintf(out, size, "T+ %s, under way", clock);
    }
}

}  // namespace

TitleAction build_title(Context &ui, const Rect &screen, const TitleFrame &frame, int &selected) {
    const float inset = tokens::safe_inset(screen.w, screen.h);
    const float left = inset;
    // The plate sits a little above centre: the orrery's own centre is the star, and the plate must
    // not cover it.
    float y = screen.h * 0.5f - 150.0f;
    const float reveal = clamp01(frame.reveal);

    // R-4: the startup is a chart surface, not flight glass - warm ink on a plate. Six rows of text
    // can do with no panel at all, but the two materials have to be *visible* somewhere or "two
    // instruments, one darkness" is a claim nobody can check.
    ui.panel({left - tokens::SPACE[4], y - tokens::SPACE[4], 560.0f, 404.0f},
             with_alpha(tokens::PLATE, 0.66f * reveal));

    // The wordmark is the one place the display face is the treatment, so it is spaced by hand.
    ui.label({left, y, 600.0f, tokens::PX_50}, "O P R A", tokens::DISPLAY_TITLE, TextAlign::Left,
             with_alpha(tokens::VELLUM, reveal), TextFace::Display);
    y += tokens::PX_50 + tokens::SPACE[2];
    ui.rule({left, y, 420.0f, 1.0f}, with_alpha(tokens::VELLUM, tokens::RULE * reveal));
    y += tokens::SPACE[3];

    ui.label({left, y, 600.0f, tokens::PX_13}, "Nereid recovery service", tokens::LABEL,
             TextAlign::Left, with_alpha(tokens::VELLUM, tokens::DORMANT * reveal), TextFace::Label);
    y += tokens::SPACE[2] + tokens::PX_13;
    ui.label({left, y, 600.0f, tokens::PX_13}, "Kestrel-class licence, third renewal",
             tokens::LABEL, TextAlign::Left, with_alpha(tokens::VELLUM, tokens::DORMANT * reveal),
             TextFace::Label);
    y += tokens::SPACE[4];

    // The list. The selected row is the only accent, and the session line hangs off `continue`
    // because that is the row it describes.
    char session[96];
    session_line(session, sizeof session, frame);
    TitleAction chosen = TitleAction::None;
    for (int i = 0; i < kEntryCount; ++i) {
        const Rect row{left, y, 420.0f, tokens::PX_25 + tokens::SPACE[1]};
        // The widget owns the label; the plate adds only the line that describes the row, so nothing
        // is drawn twice. Rows carry no rule: the list is the structure (R-6).
        if (ui.row(kEntries[i], row, kEntries[i], i, tokens::VELLUM)) {
            chosen = static_cast<TitleAction>(i + 1);
            selected = i;
        }
        if (i == 0) {
            ui.label({row.x + 220.0f, row.y, 520.0f, row.h}, session, tokens::LABEL, TextAlign::Left,
                     with_alpha(tokens::VELLUM, tokens::DORMANT * reveal), TextFace::Label);
        }
        y += row.h + tokens::SPACE[1];
    }
    return chosen;
}

}  // namespace opra::ui
