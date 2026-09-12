#include "ui/table.h"

#include "ui/draw.h"
#include "ui/tokens.h"

namespace opra::ui {

float build_table(Context &ui, const Rect &at, const std::vector<TableColumn> &columns,
                  const std::vector<TableRow> &rows, int highlight, const glm::vec4 &ink) {
    if (columns.empty() || at.w <= 0.0f) return 0.0f;
    float total_weight = 0.0f;
    for (const TableColumn &column : columns) total_weight += column.weight;
    if (total_weight <= 0.0f) return 0.0f;

    const float gap = tokens::SPACE[3];  // 20 px of air between columns, and no vertical rules
    const float usable = at.w - gap * static_cast<float>(columns.size() - 1);
    if (usable <= 0.0f) return 0.0f;

    float y = at.y;
    const float header_h = tokens::PX_13 + tokens::SPACE[1];
    float x = at.x;
    for (size_t i = 0; i < columns.size(); ++i) {
        const float width = usable * columns[i].weight / total_weight;
        ui.label({x, y, width, header_h}, columns[i].header, tokens::LABEL, columns[i].align,
                 with_alpha(tokens::VELLUM, tokens::DORMANT), TextFace::Label);
        x += width + gap;
    }
    y += header_h;
    // The one structural rule: the table is held together by the line under its header.
    ui.rule({at.x, y, at.w, 1.0f}, with_alpha(tokens::VELLUM, tokens::RULE));
    y += tokens::SPACE[1];

    const float row_h = tokens::PX_20 + tokens::SPACE[1];
    for (size_t row = 0; row < rows.size(); ++row) {
        const bool lit = static_cast<int>(row) == highlight;
        const glm::vec4 color = lit ? tokens::DRIVE : with_alpha(ink, tokens::DORMANT);
        x = at.x;
        for (size_t i = 0; i < columns.size() && i < rows[row].size(); ++i) {
            const float width = usable * columns[i].weight / total_weight;
            ui.label({x, y, width, tokens::PX_20}, rows[row][i].c_str(), columns[i].px,
                     columns[i].align, color, columns[i].face);
            x += width + gap;
        }
        y += row_h;
    }
    return y - at.y;
}

}  // namespace opra::ui
