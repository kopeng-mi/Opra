// The almanac table: the structural device every data screen shares (plan 4.3). A hairline above
// and below the header, figures right-aligned on their column, no vertical rules, no zebra
// striping. It is a table because the data genuinely is tabular, and it reads like a printed one.
#pragma once

#include <string>
#include <vector>

#include "ui/draw.h"
#include "ui/tokens.h"
#include "ui/ui.h"

namespace opra::ui {

struct TableColumn {
    const char *header = "";
    /** Share of the table's width. The columns are laid out proportionally, gaps included. */
    float weight = 1.0f;
    TextAlign align = TextAlign::Right;
    TextFace face = TextFace::Readout;
    float px = 20.0f;
};

using TableRow = std::vector<std::string>;

/**
 * Draws the table inside `at`, top-aligned, and returns the height it used. `highlight` names the
 * row that is the current subject (the active transfer, the tracked body), or -1 for none: it is
 * the only row that gets an accent, so the reader's eye goes to one place. `ink` is the chart's own
 * colour: cool glass on the flight view, warm vellum on a chart surface (tokens.h, two materials).
 */
float build_table(Context &ui, const Rect &at, const std::vector<TableColumn> &columns,
                  const std::vector<TableRow> &rows, int highlight = -1,
                  const glm::vec4 &ink = tokens::ETCH);

}  // namespace opra::ui
