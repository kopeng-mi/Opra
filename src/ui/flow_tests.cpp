#include "ui/flow_tests.h"

#include <set>
#include <vector>

#include "selftest.h"
#include "ui/flow.h"

namespace opra {
namespace {

using ui::MenuAction;
using ui::Screen;
using selftest::check;

const Screen kScreens[] = {Screen::Startup, Screen::Flight, Screen::Chart,
                           Screen::Manual,  Screen::Pause,  Screen::Settings, Screen::Viewer};

/** Every screen one edge, from either table, leads to from `from`. */
std::vector<Screen> neighbours(Screen from) {
    std::vector<Screen> out;
    for (int i = 0; i < ui::FLOW_COUNT; ++i) {
        if (ui::FLOW[i].from == from) out.push_back(ui::FLOW[i].to);
    }
    for (int i = 0; i < ui::MENU_FLOW_COUNT; ++i) {
        if (ui::MENU_FLOW[i].from == from) out.push_back(ui::MENU_FLOW[i].to);
    }
    return out;
}

/** True when `to` can be reached from `from` in any number of steps. */
bool reachable(Screen from, Screen to) {
    std::set<Screen> seen{from};
    std::vector<Screen> open{from};
    while (!open.empty()) {
        const Screen at = open.back();
        open.pop_back();
        if (at == to) return true;
        for (Screen next : neighbours(at)) {
            if (seen.insert(next).second) open.push_back(next);
        }
    }
    return false;
}

}  // namespace

int flow_tests() {
    const int before = selftest::failures();

    // Every screen is reachable from the plate. A screen nobody can open is a screen nobody tests.
    for (Screen screen : kScreens) {
        if (screen == Screen::Startup) continue;
        check(reachable(Screen::Startup, screen), "flow: every screen is reachable from the plate");
    }

    // Every screen has a way back to Flight. This is the rule that catches a modal screen that can
    // only be left by quitting.
    for (Screen screen : kScreens) {
        if (screen == Screen::Flight || screen == Screen::Startup) continue;
        check(reachable(screen, Screen::Flight), "flow: every screen has a path back to flight");
    }

    // No action is bound twice in one screen, and no menu row appears twice in one screen.
    for (Screen screen : kScreens) {
        for (int i = 0; i < ui::FLOW_COUNT; ++i) {
            if (ui::FLOW[i].from != screen) continue;
            for (int j = i + 1; j < ui::FLOW_COUNT; ++j) {
                if (ui::FLOW[j].from != screen) continue;
                check(ui::FLOW[i].on != ui::FLOW[j].on,
                      "flow: no action is bound twice in one screen");
            }
        }
        for (int i = 0; i < ui::MENU_FLOW_COUNT; ++i) {
            if (ui::MENU_FLOW[i].from != screen) continue;
            for (int j = i + 1; j < ui::MENU_FLOW_COUNT; ++j) {
                if (ui::MENU_FLOW[j].from != screen) continue;
                check(ui::MENU_FLOW[i].on != ui::MENU_FLOW[j].on,
                      "flow: no menu row appears twice in one screen");
            }
        }
    }

    // No unreachable edge: every source is a screen the player can be standing on, and every edge
    // leaves the screen it starts on.
    for (int i = 0; i < ui::FLOW_COUNT; ++i) {
        check(reachable(Screen::Startup, ui::FLOW[i].from),
              "flow: every transition starts on a reachable screen");
        check(ui::FLOW[i].to != ui::FLOW[i].from, "flow: no transition goes nowhere");
    }
    for (int i = 0; i < ui::MENU_FLOW_COUNT; ++i) {
        check(reachable(Screen::Startup, ui::MENU_FLOW[i].from),
              "flow: every menu edge starts on a reachable screen");
    }

    // An action a screen does not handle leaves it where it was, and `handles` agrees with
    // `advance` about which those are.
    for (Screen screen : kScreens) {
        for (Action action : {Action::Chart, Action::Map, Action::Manual, Action::Pause,
                              Action::ModelViewer, Action::Interact}) {
            const bool handled = ui::handles(screen, action);
            check(handled || ui::advance(screen, action) == screen,
                  "flow: an unhandled action leaves the screen alone");
        }
    }

    // The plate's rows: continue flies, settings opens the settings screen, and quit stays on the
    // plate - the loop reads that row and stops.
    check(ui::advance_menu(Screen::Startup, MenuAction::TitleContinue) == Screen::Flight,
          "flow: the plate's continue flies");
    check(ui::advance_menu(Screen::Startup, MenuAction::TitleNewContract) == Screen::Flight,
          "flow: the plate's new contract flies");
    check(ui::advance_menu(Screen::Startup, MenuAction::TitleSettings) == Screen::Settings,
          "flow: the plate's settings row opens the settings screen");
    check(ui::advance_menu(Screen::Settings, MenuAction::SettingsBack) == Screen::Pause,
          "flow: settings goes back to the pause screen it came from");

    // Which is what makes the settings screen's only route home the pause screen, two Escs long.
    check(reachable(Screen::Settings, Screen::Flight), "flow: settings can be left");

    return selftest::failures() - before;
}

}  // namespace opra
