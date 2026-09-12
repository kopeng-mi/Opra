// The system map's UI half: the almanac. The orrery itself is 3D and belongs to render/; this draws
// the ephemeris table, the selected target's transfer block and the footer that sit beside it, from
// the same neutral frame (render/orrery.h), so the two halves cannot disagree about the system.
//
// A POD frame in, marks out: the UI layer never sees a World (plan 2).
#pragma once

#include "render/orrery.h"
#include "ui/draw.h"
#include "ui/ui.h"

namespace opra::ui {

/** Where the two instruments sit, in screen pixels. One function, so the camera's framing and the
 *  table's rectangle cannot disagree about where the split is. */
struct MapLayout {
    Rect orrery;   // the chart's pane: the orrery and the footer line
    Rect almanac;  // the table's pane: the ephemeris and the target block
};

/** The split: the almanac takes ~34% of the safe area's width, never under 320 px. */
MapLayout map_layout(float width, float height);

/**
 * The almanac: one ephemeris row per body (a and the true anomaly at the frame's epoch), the
 * selected target's transfer block, and the footer line. Figures are Readout, prose is Label.
 */
void build_map(Context &ui, const MapLayout &panes, const orrery::Frame &frame);

}  // namespace opra::ui
