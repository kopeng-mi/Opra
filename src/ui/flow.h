// The screen graph, as data. Every change of screen goes through one of these two tables: there is
// no `app.map = !app.map` anywhere, because a table can be tested headless and a boolean toggled in
// an input handler cannot. It also settles the class of bug where one key meant two things - an
// action is consumed by exactly one screen, and the table says which (plan 5.1).
//
// Two tables and not one, because there are two kinds of edge and conflating them is how a key
// starts meaning something no one bound: FLOW is key-driven (M, N, H, Esc, F1, F2), MENU_FLOW is the
// rows a plate or a menu offers and the pointer or the arrow keys select.
#pragma once

#include <vector>

#include "game/bindings.h"

namespace opra::ui {

/**
 * The screens. Flow is Title -> Contract -> Shipyard -> Flight (plan 06 L1).
 * Overlay screens push onto the stack; dismissing them pops back to whoever opened them (L2).
 */
enum class Screen {
    Startup,
    Contract,
    Shipyard,
    Flight,
    Chart,
    Manual,
    Pause,
    Settings,
    Viewer
};

enum class Mode { Replace, Push, Pop };

/**
 * One key-driven edge. `on` is the action that takes you from `from` to `to`; `mode` controls
 * whether the target screen replaces the current, pushes onto the stack, or pops back.
 */
struct Transition {
    Screen from;
    Action on;
    Screen to;
    Mode mode;
};

extern const Transition FLOW[];
extern const int FLOW_COUNT;

/**
 * The rows the plate and the menus offer. They are edges like any other; the difference is only how
 * they are selected.
 */
enum class MenuAction {
    TitleBegin,
    TitleSettings,
    TitleManual,
    TitleQuit,
    ContractAccept,
    ContractBack,
    ShipyardLaunch,
    ShipyardBack,
    PauseResume,
    PauseSettings,
    PauseAbandon,
    SettingsBack,
};

struct MenuTransition {
    Screen from;
    MenuAction on;
    Screen to;
    Mode mode;
};

extern const MenuTransition MENU_FLOW[];
extern const int MENU_FLOW_COUNT;

/** The name the log and the tests use. */
const char *screen_name(Screen screen);
/** The enum value for a name, or Startup when the name is not a screen. */
Screen screen_from_name(const char *name);

const Transition *find_transition(Screen from, Action on);
const MenuTransition *find_menu_transition(Screen from, MenuAction on);

/** Stack mutations per plan 06 §2.1. Pop guards against popping the root element. */
void apply(std::vector<Screen> &stack, const Transition &edge);
void apply_menu(std::vector<Screen> &stack, const MenuTransition &edge);

bool apply(std::vector<Screen> &stack, Action on);
bool apply_menu(std::vector<Screen> &stack, MenuAction on);

/** The screen an action leads to from `from`, or `from` itself when it leads nowhere. */
Screen advance(Screen from, Action on);
/** True when `on` is bound in `from` (whether or not it changes the screen). */
bool handles(Screen from, Action on);
/** The screen a menu row leads to, or `from` itself when the row is not offered there. */
Screen advance_menu(Screen from, MenuAction on);

}  // namespace opra::ui
