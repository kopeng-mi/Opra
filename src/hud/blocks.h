// The HUD's blocks: each one is laid into a corner column and each one reports the height it used.
// R-1 was a stacking bug - two clusters placed at hand-computed y values drew over each other - and
// the fix is structural: a block cannot overlap its neighbour because the cursor that places it is
// the only thing that knows where the previous block ended. `check_block_layout` proves that every
// frame under --debug (plan 5.2).
#pragma once

#include <vector>

#include "hud/hud.h"
#include "ui/draw.h"
#include "ui/ui.h"

namespace opra {

/** A block's name and the rectangle it drew into. The name is what the layout pass logs. */
struct BlockRect {
    const char *name;
    ui::Rect at;
};

/**
 * The block contract (plan 06 §3.6). A block that has nothing to say returns false from live()
 * and occupies nothing.
 */
struct Block {
    const char *title = "";
    float (*measure)(const HudFrame &) = nullptr;
    void (*build)(UIBatch &, const HudFrame &, const ui::Rect &at) = nullptr;
    bool (*live)(const HudFrame &) = nullptr;
};

/** Fills with FIELD at α 0.72 plus a 12 px ring at α 0.36 (plan 06 §3.4). */
void draw_scrim(UIBatch &batch, const ui::Rect &at);

/**
 * One corner's column. Top corners stack downward from the safe area's top edge, bottom corners
 * upward from its bottom edge; the cursor is the whole of the state.
 */
class BlockLayouter {
public:
    BlockLayouter(const ui::Rect &column, bool from_top);

    /** Reserves `height` and returns the rect the block should draw into. */
    ui::Rect place(const char *name, float height);

    const std::vector<BlockRect> &blocks() const { return blocks_; }

private:
    ui::Rect column_;
    bool from_top_;
    float cursor_ = 0.0f;
    std::vector<BlockRect> blocks_;
};

/**
 * The layout assertion pass (plan 5.2, F7). Rules:
 *  - every block sits inside the safe area;
 *  - no two blocks overlap;
 *  - a text whose anchor falls inside a block has to fit inside it;
 *  - no block's rect may intersect the arc's reserved span (plan 06 §3.6).
 *
 * Returns the violation count; `log` prints each one with the block name.
 */
int check_block_layout(const std::vector<BlockRect> &blocks, const UIBatch &batch,
                       const ui::Rect &safe, bool log);

}  // namespace opra
