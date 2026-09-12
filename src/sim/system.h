// The star system as data: bodies on analytic Kepler rails about their parents, in double metres,
// in a frame anchored at the barycentre. Nothing here is hardcoded - the file names every body, so
// a second system is a second file (plan E4), and a station is a body with no gravity of its own.
#pragma once

#include <string>
#include <vector>

#include <glm/glm.hpp>

#include "core/units.h"
#include "orbit/kepler.h"

namespace opra {

enum class BodyClass { Star, Planet, Moon, Station };

/** A body's atmosphere, for drag and stagnation heating (plan 3.3). Absent means vacuum. */
struct Atmosphere {
    bool present = false;
    double rho0 = 0.0;          // density at the datum, kg/m^3
    double scale_height = 0.0;  // H, metres: rho(h) = rho0 * exp(-h/H)
    double top = 0.0;           // altitude above which nothing is modelled, metres
};

/** A forced-flat span of terrain: a landing pad (plan 3.2). */
struct Pad {
    double theta = 0.0;  // angle about the body, radians
    double width = 0.0;  // arc length, metres
    std::string name;
};

/** A body's surface, as the heightfield generator sees it (plan 3.2). */
struct TerrainDef {
    bool present = false;
    unsigned int seed = 0;
    /** Octave amplitude as a fraction of the body radius: the plan's A = R * amplitude. */
    double amplitude = 0.004;
    std::vector<Pad> pads;
};

/**
 * A station that is not on a conic: it holds a fixed offset from its primary in the primary's
 * rotating frame (plan 3.5). A station at L1 has a smaller radius than the secondary, so a
 * Keplerian propagation of it would run at a different period and drift away within days. That is
 * the documented cheat - see plan 3.5 - and `ponytail:` in orbit/lagrange.cpp.
 */
struct LagrangeSeat {
    bool present = false;
    int primary = -1;     // the secondary the point belongs to: Tessera for Tessera L4
    int point = 4;        // 1..5
    double dtheta = 0.0;  // extra rotation about the primary, radians
};

/** A base on a pad: refuel, repair, contracts (plan G16). */
struct SurfaceSeat {
    bool present = false;
    int pad = -1;            // index into the body's terrain pads
    std::string name;        // "Tessera Downport"
    std::string model;       // a name in assets/models.json, or empty for a pad with nothing on it
};

struct Body {
    std::string id;
    std::string name;
    BodyClass kind = BodyClass::Planet;
    /** Index of the body it orbits, or -1 for the star. Parents always precede their children. */
    int parent = -1;
    /** Gravitational parameter, m^3/s^2. Zero for a station: it is a target, not a primary. */
    double mu = 0.0;
    double radius = 0.0;
    /** A name in assets/models.json. */
    std::string model;
    orbit::Elements elements{};
    /** Sphere of influence about the parent, metres. Zero when the body cannot hold one. */
    double soi = 0.0;
    /** Drag and heating, if it has air (G12). */
    Atmosphere atmosphere;
    /** A heightfield and its pads, if it has a surface (G13). */
    TerrainDef terrain;
    /** A co-rotating L-point placement, if the station sits at one (G11). */
    LagrangeSeat lagrange;
    /** A base on one of this body's pads (G16). */
    SurfaceSeat surface;
};

struct Belt {
    std::string id;
    int parent = -1;
    double inner = 0.0;
    double outer = 0.0;
    int count = 0;
    unsigned int seed = 0;
};

struct SystemDef {
    std::string name;
    double epoch = 0.0;
    /** bodies[0] is the star: the frame's anchor, with no parent. */
    std::vector<Body> bodies;
    Belt belt;
    /** Present and empty from day one (E4). Nothing reads it yet. */
    std::vector<std::string> jump_links;

    int index_of(const std::string &id) const;
};

/** Reads a system file. A malformed one is fatal and names the field and the file: a silently
 *  half-loaded system is worse than no system. */
SystemDef load_system(const std::string &path);

/** A body's place and motion in the barycentric frame, double metres. */
struct BodyState {
    glm::dvec2 position{0.0};
    glm::dvec2 velocity{0.0};
};

/** Fills `out` with every body's state at `t`, in SystemDef::bodies order. */
void propagate(const SystemDef &system, double t, std::vector<BodyState> &out);

/**
 * The deepest body whose sphere of influence holds `position`, or -1 for the star. `current` is
 * the body the caller was already orbiting: the test has hysteresis (enter at the SOI radius,
 * leave at 1.02x it) so a ship skimming the boundary does not thrash between frames.
 */
int primary_of(const SystemDef &system, const std::vector<BodyState> &states, int current,
               const glm::dvec2 &position);

}  // namespace opra
