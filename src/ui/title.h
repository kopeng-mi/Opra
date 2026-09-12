// The startup plate: the almanac's title page over the live orrery (E12, plan 4.4). One composed
// moment - the wordmark, the rule, the service line - and a short list of what can be done next.
// Copy is plain and active: `continue`, not `Continue Game`; the status line states a fact.
#pragma once

#include "ui/draw.h"
#include "ui/ui.h"

namespace opra::ui {

/** What the plate says about the session it is offering to continue. */
struct TitleFrame {
    double sessionSeconds = 0.0;
    bool docked = false;
    const char *dockName = "";
    const char *shipName = "";
    /** True while the rings are still drawing themselves in: the plate fades up with them. */
    float reveal = 1.0f;
};

/** What the pilot chose. `None` means nothing was chosen this frame. */
enum class TitleAction { None, Continue, NewContract, Settings, Manual, Quit };

/**
 * Draws the title plate and runs its list. Up/Down (or the mouse) move the selection, Enter (or a
 * click) chooses. The plate is left-aligned and sits in the safe area, so it reads the same at any
 * window size.
 */
TitleAction build_title(Context &ui, const Rect &screen, const TitleFrame &frame,
                        int &selected);

}  // namespace opra::ui
