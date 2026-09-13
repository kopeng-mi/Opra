#include "ui/menus.h"

#include <cstdio>

#include "hud/hud.h"

namespace opra::ui {

PauseResult build_pause(Context &ui, const Rect &screen) {
    PauseResult result;
    ui.push(screen);
    // Scrim to α 0.92 (plan 06 §4.7, fixes T-8).
    push_rect(*ui.batch(), {0.0f, 0.0f}, {screen.w, screen.h}, with_alpha(ui::tokens::FIELD, 0.92f));

    const float width = 420.0f;
    const Rect panel{screen.w * 0.5f - width * 0.5f, screen.h * 0.32f, width, 300.0f};
    ui.push(panel);

    ui.label(ui.cut_top(56.0f), "Paused", 40.0f, TextAlign::Left, ui::tokens::ETCH);
    ui.label(ui.cut_top(30.0f), "the drive is holding station", 14.0f, TextAlign::Left,
             ui::tokens::ETCH_DIM, TextFace::Label);
    ui.cut_top(28.0f);
    if (ui.button("pause.resume", ui.cut_top(40.0f), "Resume")) result.resume = true;
    if (ui.button("pause.settings", ui.cut_top(40.0f), "Settings")) result.open_settings = true;
    // Abandon run returns to title (plan 06 §2.2, T-3)
    if (ui.button("pause.quit", ui.cut_top(40.0f), "Abandon run")) result.quit = true;
    ui.pop();

    ui.pop();
    return result;
}

SettingsResult build_settings(Context &ui, const Rect &screen, Settings &settings) {
    SettingsResult result;
    ui.push(screen);
    // Scrim to α 0.92 (plan 06 §4.7, fixes T-8).
    push_rect(*ui.batch(), {0.0f, 0.0f}, {screen.w, screen.h}, with_alpha(ui::tokens::FIELD, 0.92f));

    // Bounded settings width; label left, control adjacent, gap capped at 180 px (T-9).
    const float width = 520.0f;
    const Rect panel{screen.w * 0.5f - width * 0.5f, 100.0f, width, screen.h - 200.0f};
    ui.push(panel);

    ui.label(ui.cut_top(56.0f), "Settings", 40.0f, TextAlign::Left, ui::tokens::ETCH);
    ui.cut_top(16.0f);
    ui.section(ui.cut_top(26.0f), "view");
    ui.cut_top(8.0f);

    // Camera pitch preview lives behind scrim while dragging.
    float pitch = settings.camera_pitch;
    if (ui.slider("set.pitch", ui.cut_top(38.0f), "camera pitch", pitch, 17.0f, 88.0f)) {
        settings.camera_pitch = pitch;
        result.changed = true;
    }

    ui.cut_top(12.0f);
    ui.section(ui.cut_top(26.0f), "flight");
    ui.cut_top(8.0f);
    if (ui.toggle("set.assist", ui.cut_top(38.0f), "attitude assist at launch", settings.assist)) {
        result.changed = true;
    }
    if (ui.toggle("set.motion", ui.cut_top(38.0f), "reduced motion", settings.reduced_motion)) {
        result.changed = true;
    }

    ui.cut_top(12.0f);
    ui.section(ui.cut_top(26.0f), "renderer");
    ui.cut_top(8.0f);

    // Two-button segmented pair for MSAA rather than detached radio marks (T-10).
    int msaa = settings.msaa >= 4 ? 4 : 1;
    const Rect msaa_row = ui.cut_top(38.0f);
    ui.label({msaa_row.x, msaa_row.y, 160.0f, msaa_row.h}, "multisampling", 15.0f,
             TextAlign::Left, ui::tokens::ETCH_DIM, TextFace::Label);
    const Rect seg1{msaa_row.x + 180.0f, msaa_row.y + 4.0f, 70.0f, 28.0f};
    const Rect seg4{msaa_row.x + 258.0f, msaa_row.y + 4.0f, 70.0f, 28.0f};
    if (msaa == 1) {
        ui.panel(seg1, with_alpha(ui::tokens::NAV, 0.35f));
    }
    if (ui.button("set.msaa1", seg1, "1x")) {
        settings.msaa = 1;
        result.changed = true;
    }
    if (msaa == 4) {
        ui.panel(seg4, with_alpha(ui::tokens::NAV, 0.35f));
    }
    if (ui.button("set.msaa4", seg4, "4x")) {
        settings.msaa = 4;
        result.changed = true;
    }

    if (ui.toggle("set.stats", ui.cut_top(38.0f), "show frame budget", settings.show_stats)) {
        result.changed = true;
    }

    ui.cut_top(16.0f);
    if (ui.button("set.back", ui.cut_top(40.0f), "Back")) result.back = true;
    ui.pop();
    ui.pop();
    return result;
}

void build_stats(Context &ui, const Rect &screen, float fps, unsigned instances, unsigned runs,
                 unsigned long long triangles, int submits) {
    const Rect panel{screen.w - 290.0f, 74.0f, 262.0f, 116.0f};
    push_rect(*ui.batch(), {panel.x, panel.y}, {panel.w, panel.h}, with_alpha(ui::tokens::FIELD, 0.55f));
    ui.push(panel);
    ui.label(ui.cut_top(26.0f), "frame budget", 13.0f, TextAlign::Left, ui::tokens::ETCH_DIM,
             TextFace::Label);
    char line[64];
    std::snprintf(line, sizeof line, "%.1f fps  %.2f ms", static_cast<double>(fps),
                  fps > 0.0f ? 1000.0 / static_cast<double>(fps) : 0.0);
    ui.label(ui.cut_top(20.0f), line, 14.0f, TextAlign::Left, ui::tokens::ETCH, TextFace::Label);
    std::snprintf(line, sizeof line, "%u inst  %u runs  %llu tris", instances, runs, triangles);
    ui.label(ui.cut_top(20.0f), line, 13.0f, TextAlign::Left, ui::tokens::ETCH_DIM, TextFace::Label);
    std::snprintf(line, sizeof line, "%d GPU submits", submits);
    ui.label(ui.cut_top(20.0f), line, 13.0f, TextAlign::Left, ui::tokens::ETCH_DIM, TextFace::Label);
    ui.pop();
}

}  // namespace opra::ui
