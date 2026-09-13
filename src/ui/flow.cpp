#include "ui/flow.h"

#include <cstring>

namespace opra::ui {

/**
 * The whole flow. Reading down the table is reading the game: Esc always goes back, the two
 * instruments (chart, manual) each open from Flight and close onto it, and the viewer closes on
 * its own key as well. There is no map screen to open: the flight view is the map (plan 05 J2).
 *
 * Two rules the tests enforce: no action appears twice for one screen, and every screen has a path
 * back to Flight. Both are easy to break by hand and impossible to break quietly here.
 */
const Transition FLOW[] = {
    // Flight. Every instrument opens here and every one of them closes onto Flight.
    {Screen::Flight, Action::Chart, Screen::Chart, false},
    {Screen::Flight, Action::Manual, Screen::Manual, false},
    {Screen::Flight, Action::Pause, Screen::Pause, false},
    {Screen::Flight, Action::ModelViewer, Screen::Viewer, false},

    // The instruments close on Esc and on their own key.
    {Screen::Chart, Action::Pause, Screen::Flight, false},
    {Screen::Chart, Action::Chart, Screen::Flight, false},
    {Screen::Manual, Action::Pause, Screen::Flight, false},
    {Screen::Manual, Action::Manual, Screen::Flight, false},
    {Screen::Viewer, Action::Pause, Screen::Flight, false},
    {Screen::Viewer, Action::ModelViewer, Screen::Flight, false},

    // The pause screen resumes on Esc.
    {Screen::Pause, Action::Pause, Screen::Flight, false},
    {Screen::Settings, Action::Pause, Screen::Pause, false},
};

const int FLOW_COUNT = static_cast<int>(sizeof(FLOW) / sizeof(FLOW[0]));

/**
 * The rows. The plate's five entries are the title's own list (ui/title.h prints them), the pause
 * screen's two are its own, and the settings screen's is its Back row. No key is invented here: the
 * plate's rows are chosen, not bound.
 */
const MenuTransition MENU_FLOW[] = {
    {Screen::Startup, MenuAction::TitleContinue, Screen::Flight},
    {Screen::Startup, MenuAction::TitleNewContract, Screen::Flight},
    {Screen::Startup, MenuAction::TitleSettings, Screen::Settings},
    {Screen::Startup, MenuAction::TitleManual, Screen::Manual},
    {Screen::Startup, MenuAction::TitleQuit, Screen::Startup},  // the loop reads the row and quits
    {Screen::Pause, MenuAction::PauseResume, Screen::Flight},
    {Screen::Pause, MenuAction::PauseSettings, Screen::Settings},
    {Screen::Settings, MenuAction::SettingsBack, Screen::Pause},
};

const int MENU_FLOW_COUNT = static_cast<int>(sizeof(MENU_FLOW) / sizeof(MENU_FLOW[0]));

const char *screen_name(Screen screen) {
    switch (screen) {
        case Screen::Startup: return "startup";
        case Screen::Flight: return "flight";
        case Screen::Chart: return "chart";
        case Screen::Manual: return "manual";
        case Screen::Pause: return "pause";
        case Screen::Settings: return "settings";
        case Screen::Viewer: return "viewer";
    }
    return "flight";
}

Screen screen_from_name(const char *name) {
    for (Screen screen :
         {Screen::Startup, Screen::Flight, Screen::Chart, Screen::Manual, Screen::Pause,
          Screen::Settings, Screen::Viewer}) {
        if (std::strcmp(screen_name(screen), name) == 0) return screen;
    }
    return Screen::Startup;
}

Screen advance(Screen from, Action on) {
    for (int i = 0; i < FLOW_COUNT; ++i) {
        const Transition &edge = FLOW[i];
        if (edge.from == from && edge.on == on) return edge.to;
    }
    return from;
}

bool handles(Screen from, Action on) {
    for (int i = 0; i < FLOW_COUNT; ++i) {
        if (FLOW[i].from == from && FLOW[i].on == on) return true;
    }
    return false;
}

Screen advance_menu(Screen from, MenuAction on) {
    for (int i = 0; i < MENU_FLOW_COUNT; ++i) {
        const MenuTransition &edge = MENU_FLOW[i];
        if (edge.from == from && edge.on == on) return edge.to;
    }
    return from;
}

}  // namespace opra::ui
