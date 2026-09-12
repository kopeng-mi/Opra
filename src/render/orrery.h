// The system map's 3D half: the orrery. Concentric rings at true proportion sampled from each
// body's own conic, true-anomaly ticks, a glyph per body at a floor size - a planet at true scale
// is sub-pixel beside its own orbit, and pretending otherwise is a lie (plan 4.4) - the belt as a
// thin annulus of rocks, and the ship as one distinct mark.
//
// The frame is a neutral description in double metres about one origin (orbit/ is a leaf math
// library), so the same builder serves the map screen and the startup plate (E12). Positions are
// divided by a single factor and cast to float exactly once, so the chart's proportions are exact
// whatever the system's size.
#pragma once

#include <string>
#include <vector>

#include <glm/glm.hpp>

#include "orbit/kepler.h"
#include "render/camera.h"
#include "render/model.h"
#include "render/scene.h"

namespace opra::orrery {

// --------------------------------------------------------------------------- ink
// The chart's ink, mirroring ui/tokens.h the way hud/hud.h does: the palette travels with the layer
// that draws it, and the design system stays where the plate and the type live.
namespace ink {

inline const glm::vec3 VELLUM{0.902f, 0.863f, 0.784f};  // #e6dcc8 chart ink
inline const glm::vec3 DRIVE{0.937f, 0.722f, 0.475f};   // #efb879 the live transfer
inline const glm::vec3 ETCH{0.863f, 0.902f, 0.910f};    // #dce6e8 the ship mark
/** The system file's star colour (#ff9c5c). The body with no orbit of its own is the primary, and
 *  it is the one mark whose ink is chosen here: sim::SystemDef does not carry a star colour yet. */
inline const glm::vec3 STAR{1.000f, 0.612f, 0.361f};

/** Dormant weight, ui/tokens.h::DORMANT: the belt is present, not asserting itself. */
inline constexpr float DORMANT = 0.55f;

}  // namespace ink

// -------------------------------------------------------------------------- rules
/** A ring's vertex budget. At 192 the chord sagitta is r (1 - cos(pi/192)) = 1.34e-4 r, an order
 *  under the 0.1% the rings are held to, and the count is fixed so the map's instance budget is a
 *  constant. */
inline constexpr int RING_SEGMENTS = 192;

/** True-anomaly ticks along each ring: one per 30 degrees. Sampling is uniform in true anomaly, so
 *  tick k is sample k * RING_SEGMENTS / RING_TICKS. */
inline constexpr int RING_TICKS = 12;

/** Segments in a sphere of influence disc: a boundary mark, coarse on purpose. */
inline constexpr int SOI_SEGMENTS = 64;

/** A glyph never draws smaller than this share of the drawn field. 2.5% clears the star's 3.4e8 m
 *  disc even against the system's inner field (Tessera's 1.72e10 m orbit), so every body reads as
 *  one deliberate mark and the true sizes stay in the almanac. */
inline constexpr double GLYPH_FLOOR = 0.025;

/** The drawn field spans this many scene units, so the chart fits the camera's 1..12000 unit depth
 *  range whether the system is 47 Gm across or 100 Mm: one factor scales every mark. */
inline constexpr double FIELD_UNITS = 40.0;

/** Air around the field, so a glyph at the edge is not cut by the frame. */
inline constexpr float FIELD_MARGIN = 1.08f;

/** Rocks drawn for the belt, however many it holds: the map's instance budget is bounded. */
inline constexpr int MAX_BELT_ROCKS = 1200;

/** Chart pitch: half a degree short of straight down. At exactly 90 degrees the plane normal is
 *  parallel to the view direction, which degenerates view_frustum's right vector; at 89.5 a 1.72e10
 *  m ring projects 416.5 px across and 415.8 px down on a 1600x900 chart, which is the 0.7 px the
 *  tilt costs, and every frustum test stays defined. */
inline constexpr float MAP_PITCH = 1.5620696f;

// ------------------------------------------------------------------------- input
/** One body of the chart, in double metres about the frame's origin: where it is now, the conic it
 *  rides, and how big it actually is. `primary` is the body it orbits, so a moon's ring is drawn
 *  around its planet and not around the star. */
struct Body {
    std::string name;
    glm::dvec2 position{0.0};
    glm::dvec2 primary{0.0};
    orbit::Elements elements{};
    /** True radius, metres. The almanac prints it; the glyph floors it. */
    double radius = 0.0;
    glm::vec3 color{ink::VELLUM};

    /**
     * What the planet shader needs beyond the size (plan 4.4). These are the system file's own
     * numbers, filled from the game layer's copy of the body: the terrain seed and amplitude over
     * the radius, and the atmosphere as a scale height over the radius plus a top. An airless body
     * leaves both at zero, and an amplitude of zero is a smooth sphere.
     */
    float terrain_seed = 0.0f;
    float terrain_amplitude = 0.0f;
    float scale_height = 0.0f;
    float atmosphere_top = 0.0f;
    /** The body's map names, straight from the system file (plan-04 s3.4). */
    std::string albedo_map;
    std::string cloud_map;
    std::string night_map;
    std::string photosphere_map;
};

/**
 * A predicted path drawn on the chart: a polyline in double metres about the frame origin, not a
 * conic, because a patched-conic leg is a list of points and the render layer must not re-derive
 * it (plan 3.6/3.7). The map fills these from the game layer; an empty frame draws none.
 */
struct PredictedPath {
    std::vector<glm::dvec2> points;
    /** The body the leg is drawn about, as an index into `bodies`, or -1 for the frame origin. */
    int body = -1;
    /** True for a leg after the encounter: the pre-encounter approach is the solid one. */
    bool post = false;
    bool dashed = false;
    /** The body's sphere of influence, metres: drawn as a dashed disc about `body`. 0 draws none. */
    double soi = 0.0;
};

/** The almanac's second block: the selected target and the transfer to it, as the game layer worked
 *  them out (orbit/transfer.h). Read only when `has_target`, and by the map screen only. */
struct Target {
    bool has_target = false;
    std::string name;      // the body the transfer is aimed at
    std::string ship;      // "Kestrel": the block's header
    double r = 0.0;        // metres from the primary at the burn
    double speed = 0.0;    // m/s
    double dv = 0.0;       // m/s, both burns
    double arrival = 0.0;  // seconds from the epoch
    double window = 0.0;   // seconds until the departure window opens
    std::string from;      // "Tessera"
    std::string to;        // "Halberd"
};

/** The whole chart. The game layer fills it from the world; the renderer and the UI both read it,
 *  and neither of them includes sim/ (plan 2). */
struct Frame {
    std::vector<Body> bodies;
    /** The epoch drawn, seconds: the table's true anomalies are read at it. */
    double t = 0.0;

    /** The belt, about the body it orbits: radii in metres, rocks in the drawn annulus. */
    glm::dvec2 belt_primary{0.0};
    double belt_inner = 0.0;
    double belt_outer = 0.0;
    int belt_count = 0;
    unsigned belt_seed = 0;

    /** The live transfer, about the frame origin: the actual conic, not a decorative arc. */
    bool has_transfer = false;
    orbit::Elements transfer{};

    /**
     * Predicted paths, drawn as the transfer arc is: a polyline per leg, dashed when the leg is an
     * intention rather than the orbit the ship is on. The map fills these from the encounter
     * prediction; the render layer never computes them (plan 3.6).
     */
    std::vector<PredictedPath> predicted;

    glm::dvec2 ship_position{0.0};
    /** Radians: forward = (-sin, cos), the flight model's own convention (physics.h). */
    double ship_heading = 0.0;

    /** True scale draws every glyph at its real radius: the honest, and mostly empty, chart. */
    bool true_scale = false;

    Target target;
};

/** The meshes the orrery draws with, as the ids they were registered under. */
struct Meshes {
    int box = -1;    // unit cube: ring segments, ticks and transfer dashes
    int rock = -1;   // coarse unit sphere: the belt
    int ship = -1;   // unit cone, nose +Y: the ship mark
    /** Unit sphere, dense enough for the planet shader: every body draws with this, and the LOD is
     *  the shader's, not the geometry's (plan 4.4). */
    int planet = -1;
};

/**
 * Registers the orrery's meshes in the shared library and returns their ids. Idempotent by key, and
 * it must run before the GPU upload: the renderer's draw table is built from the library, so a mesh
 * created after it is never drawn. Call it beside the star/sky/nebula registrations in
 * ModelSet::build, or from App::init before upload_mesh_library - the same moment either way.
 */
Meshes add_meshes(MeshLibrary &library);

// ------------------------------------------------------------------- the frame's geometry
/** The drawn field: the farthest mark's radius about the origin, in metres. */
double field_radius(const Frame &frame);

/** Metres per scene unit for a field: the field spans FIELD_UNITS. */
double metres_per_unit(double field_radius);

/**
 * A glyph's drawn radius in metres: the body's true radius, or GLYPH_FLOOR of the field when that
 * is larger, or the true radius outright in true-scale mode.
 */
double glyph_radius(double body_radius, double field_radius, bool true_scale);

/** The chord sagitta of a regular `segments`-gon of `radius`: r (1 - cos(pi/segments)), which is
 *  how far the drawn ring sits inside the true conic at each chord's midpoint. */
double ring_sagitta(double radius, int segments);

/**
 * `segments` points along the body's own conic, uniform in true anomaly: one full revolution for an
 * ellipse, with no duplicate vertex at the join, and an open branch for a hyperbola.
 */
void sample_ring(const orbit::Elements &elements, int segments, std::vector<glm::dvec2> &out);

/** `count` rocks uniform in the annulus [inner, outer] about `centre`, deterministic in `seed`. */
void sample_belt(double inner, double outer, const glm::dvec2 &centre, int count, unsigned seed,
                 std::vector<glm::dvec2> &out);

/**
 * Places every mark for one frame: rings, ticks, glyphs, the belt, the ship and, when there is one,
 * the live transfer. All of it is ink, not matter, so it is drawn in the additive backdrop pass -
 * unlit, order-independent, and unable to occlude anything. Positions are metres about the frame
 * origin, divided by the frame's factor before the one float cast.
 */
void build(SceneBuilder &scene, const Meshes &meshes, const Frame &frame);

/**
 * The chart's camera: a plan view of the field, framed so the system lands in a pane whose centre is
 * `centre_x` of the screen width (the almanac takes the right), with a pitch half a degree off the
 * pole so the view basis stays defined.
 */
Camera map_camera(const Frame &frame, float width, float height, float centre_x = 0.5f);

}  // namespace opra::orrery
