#include "ui/flow_tests.h"

#include <set>
#include <vector>

#include "core/file.h"
#include "selftest.h"
#include "sim/designs.h"
#include "ui/flow.h"
#include "ui/screens.h"
#include "ui/ui.h"

#include <fstream>
#include <nlohmann/json.hpp>

namespace opra {
namespace {

using ui::MenuAction;
using ui::Screen;
using selftest::check;

const Screen kScreens[] = {
    Screen::Startup, Screen::Contract, Screen::Shipyard,
    Screen::Flight,  Screen::Chart,    Screen::Manual,
    Screen::Pause,   Screen::Settings, Screen::Viewer
};

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

    // Every screen has a way back to Flight (or Startup).
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
    // leaves the screen it starts on (except loops like Quit).
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
                              Action::ModelViewer, Action::Interact, Action::Settings}) {
            const bool handled = ui::handles(screen, action);
            check(handled || ui::advance(screen, action) == screen,
                  "flow: an unhandled action leaves the screen alone");
        }
    }

    // New test 1: for every screen S and every Push edge S -> I, applying the edge then I's Pop
    // edge returns a stack whose back is S (plan 06 §2.3, fixes T-1).
    for (Screen s : kScreens) {
        for (int i = 0; i < ui::FLOW_COUNT; ++i) {
            if (ui::FLOW[i].from == s && ui::FLOW[i].mode == ui::Mode::Push) {
                const Screen target = ui::FLOW[i].to;
                std::vector<Screen> stack = {s};
                ui::apply(stack, ui::FLOW[i]);
                check(stack.size() == 2 && stack.back() == target, "flow: push increases depth");

                bool popped = false;
                for (int j = 0; j < ui::FLOW_COUNT; ++j) {
                    if (ui::FLOW[j].from == target && ui::FLOW[j].mode == ui::Mode::Pop) {
                        std::vector<Screen> test_stack = stack;
                        ui::apply(test_stack, ui::FLOW[j]);
                        check(test_stack.size() == 1 && test_stack.back() == s,
                              "flow: push then pop returns to source screen (FLOW)");
                        popped = true;
                        break;
                    }
                }
                if (!popped) {
                    for (int j = 0; j < ui::MENU_FLOW_COUNT; ++j) {
                        if (ui::MENU_FLOW[j].from == target && ui::MENU_FLOW[j].mode == ui::Mode::Pop) {
                            std::vector<Screen> test_stack = stack;
                            ui::apply_menu(test_stack, ui::MENU_FLOW[j]);
                            check(test_stack.size() == 1 && test_stack.back() == s,
                                  "flow: push then pop returns to source screen (MENU)");
                            popped = true;
                            break;
                        }
                    }
                }
                check(popped, "flow: push target has at least one pop edge");
            }
        }
        for (int i = 0; i < ui::MENU_FLOW_COUNT; ++i) {
            if (ui::MENU_FLOW[i].from == s && ui::MENU_FLOW[i].mode == ui::Mode::Push) {
                const Screen target = ui::MENU_FLOW[i].to;
                std::vector<Screen> stack = {s};
                ui::apply_menu(stack, ui::MENU_FLOW[i]);
                check(stack.size() == 2 && stack.back() == target, "flow: menu push increases depth");

                bool popped = false;
                for (int j = 0; j < ui::MENU_FLOW_COUNT; ++j) {
                    if (ui::MENU_FLOW[j].from == target && ui::MENU_FLOW[j].mode == ui::Mode::Pop) {
                        std::vector<Screen> test_stack = stack;
                        ui::apply_menu(test_stack, ui::MENU_FLOW[j]);
                        check(test_stack.size() == 1 && test_stack.back() == s,
                              "flow: menu push then pop returns to source screen (MENU)");
                        popped = true;
                        break;
                    }
                }
                if (!popped) {
                    for (int j = 0; j < ui::FLOW_COUNT; ++j) {
                        if (ui::FLOW[j].from == target && ui::FLOW[j].mode == ui::Mode::Pop) {
                            std::vector<Screen> test_stack = stack;
                            ui::apply(test_stack, ui::FLOW[j]);
                            check(test_stack.size() == 1 && test_stack.back() == s,
                                  "flow: menu push then pop returns to source screen (FLOW)");
                            popped = true;
                            break;
                        }
                    }
                }
                check(popped, "flow: menu push target has at least one pop edge");
            }
        }
    }

    // New test 2: no Pop edge exists on Startup (plan 06 §2.3).
    for (int i = 0; i < ui::FLOW_COUNT; ++i) {
        if (ui::FLOW[i].from == Screen::Startup) {
            check(ui::FLOW[i].mode != ui::Mode::Pop, "flow: no Pop edge exists on Startup (FLOW)");
        }
    }
    for (int i = 0; i < ui::MENU_FLOW_COUNT; ++i) {
        if (ui::MENU_FLOW[i].from == Screen::Startup) {
            check(ui::MENU_FLOW[i].mode != ui::Mode::Pop, "flow: no Pop edge exists on Startup (MENU_FLOW)");
        }
    }

    // New test 3: every screen reachable by Push has at least one Pop edge (plan 06 §2.3).
    std::set<Screen> pushed_screens;
    for (int i = 0; i < ui::FLOW_COUNT; ++i) {
        if (ui::FLOW[i].mode == ui::Mode::Push) pushed_screens.insert(ui::FLOW[i].to);
    }
    for (int i = 0; i < ui::MENU_FLOW_COUNT; ++i) {
        if (ui::MENU_FLOW[i].mode == ui::Mode::Push) pushed_screens.insert(ui::MENU_FLOW[i].to);
    }
    for (Screen s : pushed_screens) {
        bool has_pop = false;
        for (int i = 0; i < ui::FLOW_COUNT; ++i) {
            if (ui::FLOW[i].from == s && ui::FLOW[i].mode == ui::Mode::Pop) has_pop = true;
        }
        for (int i = 0; i < ui::MENU_FLOW_COUNT; ++i) {
            if (ui::MENU_FLOW[i].from == s && ui::MENU_FLOW[i].mode == ui::Mode::Pop) has_pop = true;
        }
        check(has_pop, "flow: every screen reachable by Push has at least one Pop edge");
    }

    // New test 4: Abandon from any stack depth yields exactly {Startup} (plan 06 §2.3).
    const std::vector<std::vector<Screen>> test_stacks = {
        {Screen::Flight},
        {Screen::Startup},
        {Screen::Startup, Screen::Contract, Screen::Shipyard, Screen::Flight},
        {Screen::Flight, Screen::Pause},
        {Screen::Flight, Screen::Pause, Screen::Settings},
    };
    for (const auto &initial : test_stacks) {
        std::vector<Screen> st = initial;
        ui::apply_menu(st, MenuAction::PauseAbandon);
        check(st.size() == 1 && st.front() == Screen::Startup,
              "flow: Abandon from any stack depth yields exactly {Startup}");
    }

    // The plate's rows: begin advances to contract, settings opens settings, quit stays on plate.
    check(ui::advance_menu(Screen::Startup, MenuAction::TitleBegin) == Screen::Contract,
          "flow: the plate's begin advances to contract");
    check(ui::advance_menu(Screen::Startup, MenuAction::TitleSettings) == Screen::Settings,
          "flow: the plate's settings row opens the settings screen");
    check(ui::advance_menu(Screen::Contract, MenuAction::ContractAccept) == Screen::Shipyard,
          "flow: contract accept advances to shipyard");
    check(ui::advance_menu(Screen::Shipyard, MenuAction::ShipyardLaunch) == Screen::Flight,
          "flow: shipyard launch advances to flight");
    check(ui::advance_menu(Screen::Settings, MenuAction::SettingsBack) == Screen::Pause,
          "flow: settings goes back to pause/parent");

    // Gate 11: A design with no drive refuses launch with a reason line
    {
        ui::ShipyardState s;
        s.design.name = "Driveless";
        s.design.slots = 12;
        Placement p;
        p.part = "nose_hammerhead";
        p.slot = 0;
        p.facing = Facing::Fore;
        s.design.placements.push_back(p);
        check(!s.design_has_drive(), "gate 11: driveless design has no drive");

        ui::Context ui;
        UIBatch batch;
        ui::Pointer pointer{};
        ui::Nav nav{};
        ui.begin(batch, {1600.0f, 900.0f}, pointer, nav, 0.0);
        const ui::ShipyardResult res = ui::build_shipyard(ui, {0.0f, 0.0f, 1600.0f, 900.0f}, s);
        ui.end();

        check(!res.launch, "gate 11: design with no drive refuses launch");
        bool reason_found = false;
        for (const auto &t : batch.texts) {
            if (t.text.find("no forward drive") != std::string::npos) {
                reason_found = true;
                break;
            }
        }
        check(reason_found, "gate 11: refusal reason line is drawn");
    }

    // Gate 14: Composed collider shape count <= 64 per ship; a design over it is rejected
    {
        DesignStore store;
        store.load(asset_path("assets/designs.json"));

        std::ifstream manifest_file(asset_path("assets/models.json"));
        nlohmann::json manifest;
        manifest_file >> manifest;
        std::unordered_map<std::string, int> shape_counts;
        for (const auto &entry : manifest["models"]) {
            const std::string name = entry.value("name", "");
            const std::string sidecar_rel = entry.value("sidecar", "");
            std::ifstream sc_file(asset_path(sidecar_rel));
            if (!sc_file.is_open()) continue;
            nlohmann::json sc;
            sc_file >> sc;
            if (sc.contains("collider") && sc["collider"].contains("shapes")) {
                shape_counts[name] = static_cast<int>(sc["collider"]["shapes"].size());
            }
        }

        for (const std::string &dname : store.names()) {
            const ShipDesign &d = store.design(dname);
            int total_shapes = 0;
            for (const auto &p : d.placements) {
                total_shapes += shape_counts[p.part];
            }
            check(total_shapes <= 64, ("gate 14: " + dname + " composed shapes <= 64").c_str());
        }
    }

    return selftest::failures() - before;
}

}  // namespace opra
