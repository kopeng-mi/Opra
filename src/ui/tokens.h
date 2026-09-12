// The design system in one place: two materials, one darkness (plan 4.2). The flight view is glass
// you look through - cool, etched, mostly bare. The map and every data screen are a chart you read -
// warm ivory ink on a dark plate. Opacity is the second axis and carries as much meaning as hue:
// a dormant readout is not a different colour, it is the same ink at lower weight.
#pragma once

#include <algorithm>

#include <glm/glm.hpp>

namespace opra::ui::tokens {

// ------------------------------------------------------------------ grounds
/** The field: flight background, the deepest ground. */
inline const glm::vec4 FIELD{0.027f, 0.051f, 0.082f, 1.0f};  // #070d15
/** The plate: chart ground. One step up, fractionally warmer. */
inline const glm::vec4 PLATE{0.055f, 0.078f, 0.106f, 1.0f};  // #0e141b

// ---------------------------------------------------------------------- inks
/** Cool white: primary marks on glass. */
inline const glm::vec4 ETCH{0.863f, 0.902f, 0.910f, 1.0f};  // #dce6e8
/** Warm ivory: chart ink, ephemeris figures, orrery rings. */
inline const glm::vec4 VELLUM{0.902f, 0.863f, 0.784f, 1.0f};  // #e6dcc8
/** Teal: navigation, contacts, things that are where they should be. The hue is the hulls'
 *  observed teal band (plan-04 A2: #275459) at ink lightness, so a nav mark and the ship's own
 *  band read as one family. */
inline const glm::vec4 NAV{0.510f, 0.725f, 0.729f, 1.0f};  // #82b9ba
/** Amber: propulsion, energy, the active transfer. The observed ochre band's hue (#ae7040). */
inline const glm::vec4 DRIVE{0.933f, 0.722f, 0.478f, 1.0f};  // #eeb87a
/** Coral: out of tolerance, and nothing else. */
inline const glm::vec4 THREAT{0.875f, 0.510f, 0.467f, 1.0f};  // #df8277

// ------------------------------------------------------------------ opacity
inline constexpr float LIVE = 1.00f;     // the thing you are reading now
inline constexpr float DORMANT = 0.55f;  // present, not asserting itself
inline constexpr float RULE = 0.28f;     // structural hairlines
inline constexpr float GRID = 0.12f;     // the field's own ruling

/** The two working weights, named: a dormant readout is the same ink, not a different colour. */
inline const glm::vec4 ETCH_DIM{ETCH.x, ETCH.y, ETCH.z, DORMANT};
inline const glm::vec4 VELLUM_DIM{VELLUM.x, VELLUM.y, VELLUM.z, DORMANT};
inline const glm::vec4 VELLUM_RULE{VELLUM.x, VELLUM.y, VELLUM.z, RULE};

// ------------------------------------------------------------------ spacing
/** 4 px base: 4 / 8 / 12 / 20 / 32 / 52. Nothing is spaced by a number that is not on this scale. */
inline constexpr float SPACE[6] = {4.0f, 8.0f, 12.0f, 20.0f, 32.0f, 52.0f};

/** Safe inset: 28 px, or 3% of the short edge, whichever is larger (fixes D-18). */
inline float safe_inset(float width, float height) {
    return std::max(28.0f, std::min(width, height) * 0.03f);
}

// --------------------------------------------------------------------- type
// A ~1.25 ratio: 10 / 13 / 16 / 20 / 25 / 32 / 40 / 50.
inline constexpr float PX_10 = 10.0f;
inline constexpr float PX_13 = 13.0f;
inline constexpr float PX_16 = 16.0f;
inline constexpr float PX_20 = 20.0f;
inline constexpr float PX_25 = 25.0f;
inline constexpr float PX_32 = 32.0f;
inline constexpr float PX_40 = 40.0f;
inline constexpr float PX_50 = 50.0f;

/** Display: title and screen names, Hydrogen Whiskey, caps only. */
inline constexpr float DISPLAY_TITLE = PX_50;
inline constexpr float DISPLAY_SCREEN = PX_32;
/** Readout: every number, Barlow Condensed, tabular figures. */
inline constexpr float READOUT_LARGE = PX_25;
inline constexpr float READOUT = PX_20;
inline constexpr float READOUT_SMALL = PX_16;
inline constexpr float READOUT_TINY = PX_13;
/** Label: prose, units and row headers, Barlow, sentence case. */
inline constexpr float LABEL = PX_13;
inline constexpr float LABEL_SMALL = PX_10;

}  // namespace opra::ui::tokens
