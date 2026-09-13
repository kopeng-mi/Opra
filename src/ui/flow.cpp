#include "ui/flow.h"

#include <cstring>

namespace opra::ui {

/**
 * The whole flow (plan 06 §2.2). Reading down the table is reading the run:
 * Startup -> Contract -> Shipyard -> Flight (L1).
 * Instruments push onto the stack; dismissing them pops back to whoever opened them (L2, T-1).
 */
const Transition FLOW[] = {
    // Startup. Manual and settings push onto Startup and return to it.
    {Screen::Startup, Action::Manual, Screen::Manual, Mode::Push},
    {Screen::Startup, Action::Settings, Screen::Settings, Mode::Push},

    // Contract and Shipyard. Esc returns to the previous step.
    {Screen::Contract, Action::Pause, Screen::Startup, Mode::Pop},
    {Screen::Shipyard, Action::Pause, Screen::Contract, Mode::Pop},

    // Flight. Every instrument opens here and returns on dismiss.
    {Screen::Flight, Action::Chart, Screen::Chart, Mode::Push},
    {Screen::Flight, Action::Manual, Screen::Manual, Mode::Push},
    {Screen::Flight, Action::Pause, Screen::Pause, Mode::Push},
    {Screen::Flight, Action::ModelViewer, Screen::Viewer, Mode::Push},

    // The instruments close on Esc and on their own key.
    {Screen::Chart, Action::Pause, Screen::Flight, Mode::Pop},
    {Screen::Chart, Action::Chart, Screen::Flight, Mode::Pop},
    {Screen::Manual, Action::Pause, Screen::Flight, Mode::Pop},
    {Screen::Manual, Action::Manual, Screen::Flight, Mode::Pop},
    {Screen::Viewer, Action::Pause, Screen::Flight, Mode::Pop},
    {Screen::Viewer, Action::ModelViewer, Screen::Flight, Mode::Pop},

    // The pause and settings screens pop back on Esc.
    {Screen::Pause, Action::Pause, Screen::Flight, Mode::Pop},
    {Screen::Settings, Action::Pause, Screen::Pause, Mode::Pop},
    {Screen::Settings, Action::Settings, Screen::Pause, Mode::Pop},
};

const int FLOW_COUNT = static_cast<int>(sizeof(FLOW) / sizeof(FLOW[0]));

/**
 * The rows. Startup offers begin / settings / manual / quit.
 * Contract offers accept / back.
 * Shipyard offers launch / back.
 * Pause offers resume / settings / abandon.
 * Settings offers back.
 */
const MenuTransition MENU_FLOW[] = {
    {Screen::Startup, MenuAction::TitleBegin, Screen::Contract, Mode::Replace},
    {Screen::Startup, MenuAction::TitleSettings, Screen::Settings, Mode::Push},
    {Screen::Startup, MenuAction::TitleManual, Screen::Manual, Mode::Push},
    {Screen::Startup, MenuAction::TitleQuit, Screen::Startup, Mode::Replace},  // loop quits
    {Screen::Contract, MenuAction::ContractAccept, Screen::Shipyard, Mode::Replace},
    {Screen::Contract, MenuAction::ContractBack, Screen::Startup, Mode::Replace},
    {Screen::Shipyard, MenuAction::ShipyardLaunch, Screen::Flight, Mode::Replace},
    {Screen::Shipyard, MenuAction::ShipyardBack, Screen::Contract, Mode::Replace},
    {Screen::Pause, MenuAction::PauseResume, Screen::Flight, Mode::Pop},
    {Screen::Pause, MenuAction::PauseSettings, Screen::Settings, Mode::Push},
    {Screen::Pause, MenuAction::PauseAbandon, Screen::Startup, Mode::Replace},
    {Screen::Settings, MenuAction::SettingsBack, Screen::Pause, Mode::Pop},
};

const int MENU_FLOW_COUNT = static_cast<int>(sizeof(MENU_FLOW) / sizeof(MENU_FLOW[0]));

const char *screen_name(Screen screen) {
    switch (screen) {
        case Screen::Startup: return "startup";
        case Screen::Contract: return "contract";
        case Screen::Shipyard: return "shipyard";
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
         {Screen::Startup, Screen::Contract, Screen::Shipyard, Screen::Flight,
          Screen::Chart, Screen::Manual, Screen::Pause, Screen::Settings, Screen::Viewer}) {
        if (std::strcmp(screen_name(screen), name) == 0) return screen;
    }
    return Screen::Startup;
}

const Transition *find_transition(Screen from, Action on) {
    for (int i = 0; i < FLOW_COUNT; ++i) {
        if (FLOW[i].from == from && FLOW[i].on == on) return &FLOW[i];
    }
    return nullptr;
}

const MenuTransition *find_menu_transition(Screen from, MenuAction on) {
    for (int i = 0; i < MENU_FLOW_COUNT; ++i) {
        if (MENU_FLOW[i].from == from && MENU_FLOW[i].on == on) return &MENU_FLOW[i];
    }
    return nullptr;
}

void apply(std::vector<Screen> &stack, const Transition &edge) {
    if (edge.mode == Mode::Replace) {
        if (!stack.empty()) stack.back() = edge.to;
        else stack.push_back(edge.to);
    } else if (edge.mode == Mode::Push) {
        stack.push_back(edge.to);
    } else if (edge.mode == Mode::Pop) {
        if (stack.size() > 1) {
            stack.pop_back();
        } else if (!stack.empty() && edge.to != Screen::Startup) {
            stack.back() = edge.to;
        }
    }
}

void apply_menu(std::vector<Screen> &stack, const MenuTransition &edge) {
    if (edge.on == MenuAction::PauseAbandon) {
        stack.clear();
        stack.push_back(Screen::Startup);
        return;
    }
    if (edge.mode == Mode::Replace) {
        if (!stack.empty()) stack.back() = edge.to;
        else stack.push_back(edge.to);
    } else if (edge.mode == Mode::Push) {
        stack.push_back(edge.to);
    } else if (edge.mode == Mode::Pop) {
        if (stack.size() > 1) {
            stack.pop_back();
        } else if (!stack.empty() && edge.to != Screen::Startup) {
            stack.back() = edge.to;
        }
    }
}

bool apply(std::vector<Screen> &stack, Action on) {
    if (stack.empty()) stack.push_back(Screen::Startup);
    const Transition *edge = find_transition(stack.back(), on);
    if (!edge) return false;
    apply(stack, *edge);
    return true;
}

bool apply_menu(std::vector<Screen> &stack, MenuAction on) {
    if (on == MenuAction::PauseAbandon) {
        stack.clear();
        stack.push_back(Screen::Startup);
        return true;
    }
    if (stack.empty()) stack.push_back(Screen::Startup);
    const MenuTransition *edge = find_menu_transition(stack.back(), on);
    if (!edge) return false;
    apply_menu(stack, *edge);
    return true;
}

Screen advance(Screen from, Action on) {
    const Transition *edge = find_transition(from, on);
    if (!edge) return from;
    return edge->to;
}

bool handles(Screen from, Action on) {
    return find_transition(from, on) != nullptr;
}

Screen advance_menu(Screen from, MenuAction on) {
    const MenuTransition *edge = find_menu_transition(from, on);
    if (!edge) return from;
    return edge->to;
}

}  // namespace opra::ui
