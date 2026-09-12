#include "ui/map.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include "orbit/kepler.h"
#include "ui/table.h"
#include "ui/tokens.h"

namespace opra::ui {
namespace {

constexpr double kPi = 3.14159265358979323846;

// Spelled in escapes so the source stays ASCII. Both are in the shipped faces (verified against
// their cmaps); the Greek nu is not, which is why the anomaly column is headed "nu".
constexpr const char *kDegree = "\xc2\xb0";     // U+00B0
constexpr const char *kDeltaV = "\xce\x94v";    // U+0394, v

/** A long distance on the almanac's own scale: gigametres, two figures. */
std::string gigametres(double metres) {
    char buffer[32];
    std::snprintf(buffer, sizeof buffer, "%.2f", metres / 1e9);
    return buffer;
}

/** Where the body is on its own conic, from the elements the almanac was handed. */
double true_anomaly(const orbit::Elements &elements, double t) {
    if (!(elements.a > 0.0)) return 0.0;
    const double mean = elements.M0 + orbit::mean_motion(elements) * (t - elements.t0);
    if (!orbit::is_elliptic(elements)) {
        const double h = orbit::solve_hyperbolic(mean, elements.e);
        return 2.0 * std::atan2(std::sqrt(elements.e + 1.0) * std::sinh(h * 0.5),
                                std::sqrt(elements.e - 1.0) * std::cosh(h * 0.5));
    }
    const double eccentric = orbit::solve_kepler(mean, elements.e);
    return 2.0 * std::atan2(std::sqrt(1.0 + elements.e) * std::sin(eccentric * 0.5),
                            std::sqrt(1.0 - elements.e) * std::cos(eccentric * 0.5));
}

/** Degrees, wrapped into [0, 360): the degree sign is in both shipped faces, the Greek nu is not. */
std::string degrees(double radians) {
    char buffer[32];
    double value = std::fmod(radians * 180.0 / kPi, 360.0);
    if (value < 0.0) value += 360.0;
    std::snprintf(buffer, sizeof buffer, "%.0f%s", value, kDegree);
    return buffer;
}

std::string speed(double metres_per_second) {
    char buffer[32];
    std::snprintf(buffer, sizeof buffer, "%.1f km/s", metres_per_second / 1e3);
    return buffer;
}

std::string speed_fine(double metres_per_second) {
    char buffer[32];
    std::snprintf(buffer, sizeof buffer, "%.2f km/s", metres_per_second / 1e3);
    return buffer;
}

std::string distance(double metres) {
    char buffer[32];
    std::snprintf(buffer, sizeof buffer, "%.2f Gm", metres / 1e9);
    return buffer;
}

/** Seconds as the almanac prints time: days and hours, then hours and minutes, then minutes. */
std::string duration(double seconds) {
    char buffer[32];
    const long long total = static_cast<long long>(std::llround(std::max(0.0, seconds)));
    const long long days = total / 86400;
    const long long hours = total % 86400 / 3600;
    const long long minutes = total % 3600 / 60;
    if (days > 0) {
        std::snprintf(buffer, sizeof buffer, "%lld d %02lld h", days, hours);
    } else if (hours > 0) {
        std::snprintf(buffer, sizeof buffer, "%lld h %02lld m", hours, minutes);
    } else {
        std::snprintf(buffer, sizeof buffer, "%lld m", minutes);
    }
    return buffer;
}

}  // namespace

MapLayout map_layout(float width, float height) {
    const float inset = tokens::safe_inset(width, height);
    const float gap = tokens::SPACE[4];  // 32 px of air between the two instruments
    const float usable = std::max(0.0f, width - inset * 2.0f);
    // ~34% for the almanac, but never so wide that the chart has nowhere to be.
    const float almanac = std::min(std::max(usable * 0.34f, 320.0f), std::max(0.0f, usable - 320.0f));

    MapLayout layout;
    layout.almanac = {width - inset - almanac, inset, almanac, std::max(0.0f, height - inset * 2.0f)};
    layout.orrery = {inset, inset, std::max(0.0f, usable - almanac - gap), layout.almanac.h};
    return layout;
}

void build_map(Context &ui, const MapLayout &panes, const orrery::Frame &frame) {
    const Rect &at = panes.almanac;
    // R-4: chart surfaces are warm ivory on a plate, the other of the two materials. The almanac is
    // the place the split earns its keep: the flight glass stays cool, the chart goes warm.
    ui.panel({at.x - tokens::SPACE[3], at.y - tokens::SPACE[3], at.w + tokens::SPACE[3] * 2.0f,
              at.h + tokens::SPACE[3] * 2.0f},
             with_alpha(tokens::PLATE, 0.82f));
    // The ephemeris. The star is drawn and not listed: it is the frame's origin, with no a and no
    // nu of its own to state.
    const std::vector<TableColumn> columns = {
        {"body", 1.00f, TextAlign::Left, TextFace::Label, tokens::LABEL},
        {"a/Gm", 0.66f, TextAlign::Right, TextFace::Readout, tokens::READOUT},
        // U+03BD, Greek nu: all three shipped faces carry it (verified against their cmaps), and the
        // text engine is UTF-8 clean, so the almanac's header is the symbol it means.
        {"ν", 0.44f, TextAlign::Right, TextFace::Readout, tokens::READOUT},
    };
    std::vector<TableRow> rows;
    int highlight = -1;
    for (const orrery::Body &body : frame.bodies) {
        if (!(body.elements.a > 0.0)) continue;
        rows.push_back({body.name, gigametres(body.elements.a),
                        degrees(true_anomaly(body.elements, frame.t))});
        if (frame.target.has_target && body.name == frame.target.name) {
            highlight = static_cast<int>(rows.size()) - 1;
        }
    }
    float y = at.y + build_table(ui, {at.x, at.y, at.w, 0.0f}, columns, rows, highlight,
                                 tokens::VELLUM);

    if (frame.target.has_target) {
        y += tokens::SPACE[4];
        // The block is a table too: its header is the ship whose transfer it is, and the rule under
        // that header is the one the ephemeris above already taught the reader to trust.
        const std::vector<TableColumn> block = {
            {frame.target.ship.c_str(), 1.00f, TextAlign::Left, TextFace::Label, tokens::LABEL},
            {"", 1.00f, TextAlign::Right, TextFace::Readout, tokens::READOUT},
        };
        const std::vector<TableRow> figures = {
            {"r", distance(frame.target.r)},
            {"v", speed(frame.target.speed)},
            {kDeltaV, speed_fine(frame.target.dv)},
            {"arrival", duration(frame.target.arrival)},
            {"window", duration(frame.target.window)},
        };
        build_table(ui, {at.x, y, at.w, 0.0f}, block, figures, -1, tokens::VELLUM);
    }

    // The footer, under the chart: what the transfer is, and how long until it can be flown.
    if (!frame.target.from.empty() && !frame.target.to.empty()) {
        const float line = tokens::PX_20 + tokens::SPACE[1];
        const float top = panes.orrery.y + panes.orrery.h - line * 2.0f;
        const std::string route = frame.target.from + " -> " + frame.target.to;
        ui.label({panes.orrery.x, top, panes.orrery.w, tokens::PX_16}, route.c_str(), tokens::LABEL,
                 TextAlign::Left, with_alpha(tokens::VELLUM, tokens::DORMANT), TextFace::Label);
        const std::string window = "window opens in " + duration(frame.target.window);
        ui.label({panes.orrery.x, top + line, panes.orrery.w, tokens::PX_20}, window.c_str(),
                 tokens::READOUT, TextAlign::Left, tokens::DRIVE, TextFace::Readout);
    }
}

}  // namespace opra::ui
