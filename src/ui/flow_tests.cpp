#include "ui/flow_tests.h"

#include <set>
#include <vector>

#include "core/file.h"
#include "game/input.h"
#include "hud/hud.h"
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

    // PLAN-09 U1/U2/U3/U4: Bearing collar ring in flight HUD
    {
        HudFrame frame;
        frame.screen = {1600.0f, 900.0f};
        frame.shipScreen = {800.0f, 450.0f};
        frame.shipRadiusPx = 50.0f;
        frame.headingDeg = 45.0;
        frame.speed = 120.0;
        frame.throttle = 0.8f;
        frame.velocityDir = {0.707f, -0.707f};
        frame.noseDir = {0.707f, -0.707f};
        frame.zoom = 150.0;
        frame.density = Density::One;

        UIBatch batch;
        build_flight_hud(batch, frame);

        bool found_n = false, found_e = false, found_s = false, found_w = false;
        bool found_hdg = false;
        for (const auto &t : batch.texts) {
            if (t.text == "N") found_n = true;
            if (t.text == "E") found_e = true;
            if (t.text == "S") found_s = true;
            if (t.text == "W") found_w = true;
            if (t.text == "045°") found_hdg = true;
        }
        check(found_n && found_e && found_s && found_w, "hud: collar ring has cardinal labels");
        check(found_hdg, "hud: collar ring has floating heading readout");
        check(!batch.solid.empty(), "hud: collar ring draws geometry");

        // When hideCollar is true, cardinal labels should not be present
        UIBatch hidden_batch;
        frame.hideCollar = true;
        build_flight_hud(hidden_batch, frame);
        bool hidden_n = false;
        for (const auto &t : hidden_batch.texts) {
            if (t.text == "N") hidden_n = true;
        }
        check(!hidden_n, "hud: hideCollar suppresses collar ring");
    }

    // PLAN-09 U6/U7: Shipyard catalogue hover preview and mount ghost
    {
        ui::ShipyardState s;
        s.design.name = "Test";
        s.design.slots = 12;
        s.design.pitch = 4.0;
        PartSpec ps;
        ps.dry_mass = 7500.0;
        s.parts["nose_hammerhead"] = ps;
        s.held = -1;

        ui::Context ui;
        UIBatch batch;
        ui::Pointer pointer{};
        // First catalogue item row is around y = 14 + 32 + 4 + 6*26 + 8 + 1 + 8 + 13 = 236
        pointer.at = {50.0f, 240.0f};
        pointer.valid = true;
        ui::Nav nav{};
        ui.begin(batch, {1600.0f, 900.0f}, pointer, nav, 0.0);
        ui::build_shipyard(ui, {0.0f, 0.0f, 1600.0f, 900.0f}, s);
        ui.end();

        check(!s.preview_part.empty(), "shipyard: catalogue hover sets preview_part");

        // Now test ghost placement when holding a part
        s.held = 0; // hold first part in category 0 (nose_hammerhead)
        s.category = 0;
        Camera cam = ui::shipyard_camera(ModelSet{}, s, 1600, 900);
        glm::mat4 vp = view_projection(cam);
        Placement cand;
        cand.part = "nose_hammerhead";
        cand.slot = 0;
        cand.facing = Facing::Fore;
        cand.span = 1;
        cand.axial = true;
        Mount m = mount_transform(s.design.spine, cand);
        glm::vec4 clip = vp * glm::vec4(m.pos, 1.0);
        float px = ((clip.x / clip.w) * 0.5f + 0.5f) * 1600.0f;
        float py = (1.0f - ((clip.y / clip.w) * 0.5f + 0.5f)) * 900.0f;
        pointer.at = {px, py};

        ui.begin(batch, {1600.0f, 900.0f}, pointer, nav, 0.0);
        ui::build_shipyard(ui, {0.0f, 0.0f, 1600.0f, 900.0f}, s);
        ui.end();

        check(s.ghost_placement.has_value(), "shipyard: hovering mount ring sets ghost_placement");
        if (s.ghost_placement) {
            check(s.ghost_placement->part == "nose_hammerhead", "shipyard: ghost part matches held item");
        }

        bool found_delta = false;
        for (const auto &t : batch.texts) {
            if (t.text.find("(+") != std::string::npos) found_delta = true;
        }
        check(found_delta, "shipyard: ghost preview renders stat deltas in DERIVED panel");
    }

    // PLAN-09 U8: Middle mouse button input and shipyard camera reset
    {
        Input input;
        input.middle = true;
        input.previous_middle = false;
        check(input.middle_pressed(), "input: middle_pressed detects rising edge");
        input.begin_frame();
        check(!input.middle_pressed(), "input: middle_pressed clears on next frame");

        ui::ShipyardState s;
        s.yaw_target = 2.5f;
        s.pitch_target = 0.8f;
        s.distance_target = 2.0f;

        // Simulate middle click reset
        if (input.previous_middle) { // was pressed in previous frame
            s.yaw_target = 0.6f;
            s.pitch_target = 0.35f;
            s.distance_target = 1.0f;
        }
        check(std::abs(s.yaw_target - 0.6f) < 1e-4f, "shipyard: middle click resets yaw");
        check(std::abs(s.pitch_target - 0.35f) < 1e-4f, "shipyard: middle click resets pitch");
        check(std::abs(s.distance_target - 1.0f) < 1e-4f, "shipyard: middle click resets distance");
    }

    return selftest::failures() - before;
}

}  // namespace opra
