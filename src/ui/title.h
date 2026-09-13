// The startup plate: the almanac's title page over the live orrery (E12, plan 4.4). One composed
// moment - the wordmark, the rule, the service line - and a short list of what can be done next.
// Copy is plain and active: `begin`, not `New Game`; the fabricated session line is removed (T-3).
#pragma once

#include "ui/draw.h"
#include "ui/ui.h"

namespace opra::ui {

struct TitleFrame {
    /** True while the rings are still drawing themselves in: the plate fades up with them. */
    float reveal = 1.0f;
};

/** What the pilot chose. `None` means nothing was chosen this frame. */
enum class TitleAction { None, Begin, Settings, Manual, Quit };

/**
 * Draws the title plate and runs its list. Up/Down (or the mouse) move the selection, Enter (or a
 * click) chooses. The plate is left-aligned and sits in the safe area as a narrow column (plan 06 §4.1).
 */
TitleAction build_title(Context &ui, const Rect &screen, const TitleFrame &frame,
                        int &selected);

}  // namespace opra::ui
