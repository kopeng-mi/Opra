// The screen graph, as data. Every change of screen goes through one of these two tables: there is
// no `app.map = !app.map` anywhere, because a table can be tested headless and a boolean toggled in
// an input handler cannot. It also settles the class of bug where one key meant two things - an
// action is consumed by exactly one screen, and the table says which (plan 5.1).
//
// Two tables and not one, because there are two kinds of edge and conflating them is how a key
// starts meaning something no one bound: FLOW is key-driven (M, N, H, Esc, F2), MENU_FLOW is the
// rows a plate or a menu offers and the pointer or the arrow keys select.
#pragma once

#include "game/bindings.h"

namespace opra::ui {

/**
 * The screens. `Hangar` is not here yet: the plan lists it for the refit work, and a row for a
 * screen that cannot be reached is exactly what the reachability assert exists to catch.
 */
enum class Screen { Startup, Flight, Chart, Map, Manual, Pause, Settings, Viewer };

/**
 * One key-driven edge. `on` is the action that takes you from `from` to `to`; `pushes` means `to`
 * remembers where it came from, so Back returns there rather than to Flight.
 */
struct Transition {
    Screen from;
    Action on;
    Screen to;
    bool pushes;
};

extern const Transition FLOW[];
extern const int FLOW_COUNT;

/**
 * The rows the plate and the menus offer. They are edges like any other; the difference is only how
 * they are selected.
 */
enum class MenuAction {
    TitleContinue,
    TitleNewContract,
    TitleSettings,
    TitleManual,
    TitleQuit,
    PauseResume,
    PauseSettings,
    SettingsBack,
};

struct MenuTransition {
    Screen from;
    MenuAction on;
    Screen to;
};

extern const MenuTransition MENU_FLOW[];
extern const int MENU_FLOW_COUNT;

/** The name the log and the tests use. */
const char *screen_name(Screen screen);
/** The enum value for a name, or Startup when the name is not a screen. */
Screen screen_from_name(const char *name);

/** The screen an action leads to from `from`, or `from` itself when it leads nowhere. */
Screen advance(Screen from, Action on);
/** True when `on` is bound in `from` (whether or not it changes the screen). */
bool handles(Screen from, Action on);
/** The screen a menu row leads to, or `from` itself when the row is not offered there. */
Screen advance_menu(Screen from, MenuAction on);

}  // namespace opra::ui
