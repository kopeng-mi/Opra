#include "ui/menus.h"

#include <cstdio>

#include "hud/hud.h"

namespace opra::ui {

PauseResult build_pause(Context &ui, const Rect &screen) {
    PauseResult result;
    ui.push(screen);
    push_rect(*ui.batch(), {0.0f, 0.0f}, {screen.w, screen.h}, with_alpha(ui::tokens::FIELD, 0.78f));

    const float width = 420.0f;
    const Rect panel{screen.w * 0.5f - width * 0.5f, screen.h * 0.32f, width, 300.0f};
    ui.push(panel);

    ui.label(ui.cut_top(56.0f), "Paused", 40.0f, TextAlign::Left, ui::tokens::ETCH);
    ui.label(ui.cut_top(30.0f), "the drive is holding station", 14.0f, TextAlign::Left,
             ui::tokens::ETCH_DIM, TextFace::Label);
    ui.cut_top(28.0f);
    if (ui.button("pause.resume", ui.cut_top(40.0f), "Resume")) result.resume = true;
    if (ui.button("pause.settings", ui.cut_top(40.0f), "Settings")) result.open_settings = true;
    if (ui.button("pause.quit", ui.cut_top(40.0f), "Quit to desktop")) result.quit = true;
    ui.pop();

    ui.pop();
    return result;
}

SettingsResult build_settings(Context &ui, const Rect &screen, Settings &settings) {
    SettingsResult result;
    ui.push(screen);
    push_rect(*ui.batch(), {0.0f, 0.0f}, {screen.w, screen.h}, with_alpha(ui::tokens::FIELD, 0.82f));

    const float width = 720.0f;
    const Rect panel{screen.w * 0.5f - width * 0.5f, 120.0f, width, screen.h - 240.0f};
    ui.push(panel);

    ui.label(ui.cut_top(56.0f), "Settings", 40.0f, TextAlign::Left, ui::tokens::ETCH);
    ui.cut_top(20.0f);
    ui.section(ui.cut_top(26.0f), "view");
    ui.cut_top(8.0f);

    // F1: the orbit pitch is a framing choice read once at launch, not a flight control. It lives
    // here so the one thing the camera does not do during flight can still be set. The default
    // zoom is not a setting any more: the home framing is a design constant now that one wheel
    // runs from hull to system scale (plan 05 J2).
    float pitch = settings.camera_pitch;
    if (ui.slider("set.pitch", ui.cut_top(38.0f), "camera pitch  (degrees)", pitch, 17.0f, 88.0f)) {
        settings.camera_pitch = pitch;
        result.changed = true;
    }
    if (ui.cut_top(10.0f).h > 0.0f) ui.cut_top(0.0f);

    ui.section(ui.cut_top(26.0f), "flight");
    ui.cut_top(8.0f);
    if (ui.toggle("set.assist", ui.cut_top(38.0f), "attitude assist at launch", settings.assist)) {
        result.changed = true;
    }
    if (ui.toggle("set.motion", ui.cut_top(38.0f), "reduced motion  (instant camera)",
                  settings.reduced_motion)) {
        result.changed = true;
    }

    ui.cut_top(14.0f);
    ui.section(ui.cut_top(26.0f), "renderer");
    ui.cut_top(8.0f);
    int msaa = settings.msaa >= 4 ? 4 : 1;
    const Rect msaa_row = ui.cut_top(38.0f);
    ui.label({msaa_row.x, msaa_row.y, msaa_row.w * 0.5f, msaa_row.h}, "multisampling", 17.0f,
             TextAlign::Left, ui::tokens::ETCH_DIM);
    const Rect options{msaa_row.x + msaa_row.w * 0.5f, msaa_row.y, msaa_row.w * 0.5f, msaa_row.h};
    if (ui.radio("set.msaa1", ui.column(options, 0, 2, 40.0f), "1x", msaa, 1)) {
        settings.msaa = 1;
        result.changed = true;
    }
    if (ui.radio("set.msaa4", ui.column(options, 1, 2, 40.0f), "4x", msaa, 4)) {
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
    std::snprintf(line, sizeof line, "%.0f fps   %d submits", static_cast<double>(fps), submits);
    ui.label(ui.cut_top(22.0f), line, 15.0f, TextAlign::Left, ui::tokens::ETCH);
    std::snprintf(line, sizeof line, "%u instances   %u runs", instances, runs);
    ui.label(ui.cut_top(22.0f), line, 15.0f, TextAlign::Left, ui::tokens::ETCH);
    std::snprintf(line, sizeof line, "%llu triangles", triangles);
    ui.label(ui.cut_top(22.0f), line, 15.0f, TextAlign::Left, ui::tokens::ETCH);
    ui.pop();
}

}  // namespace opra::ui
