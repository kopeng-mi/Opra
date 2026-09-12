// Flight simulation, ported from the DRIFT source (AstraWars/src/physics.ts).
// Pure, DOM-free, deterministic: the whole port is a fixed-step Newtonian plane.
#pragma once

#include <unordered_map>
#include <vector>

#include <deque>

#include <glm/glm.hpp>

#include "core/rng.h"
#include "core/units.h"
#include "sim/collision.h"
#include "sim/shapes.h"

namespace opra {

enum class ShipClass { Kestrel = 0, Mule = 1, Needle = 2 };

/** Everything `step_ship` reads about a hull. */
struct ShipSpec {
    const char *name;
    const char *role;
    Real mass;
    Real thrust;
    Real fuel;
    Real torque;
    Real hull;
    Real length;
    Real cargo;
    /** Heat shed per second. */
    Real cooling;
    Real scanScale;
    Real collectScale;
};

extern const ShipSpec SHIPS[3];

extern const HullBoxes HULL_BOXES[3];

/** The pilot's request for one step. Five fields: it is also the wire input packet. */
struct FlightInput {
    Real thrust = 0;
    Real turn = 0;
    Real strafe = 0;
    bool brake = false;
    bool boost = false;
};

struct ShipState {
    Vec2 position;
    Vec2 velocity;
    Real angle = 0;
    Real angularVelocity = 0;
    Real fuel = 0;
    Real hull = 0;
    Real heat = 0;
    Real cooling = 0;
    Real acceleration = 0;
    Real thrustLevel = 0;
    bool rcsActive = false;
    bool assist = true;
    ShipClass shipClass = ShipClass::Kestrel;
    const ShipSpec *spec = nullptr;
    /** The drawn hull's shapes: custom builds carry their own, stock classes use the table. */
    Collider collider{};
    /** Axis-aligned extents of `collider` about the hull origin, for the collar and the muzzle. */
    HullBoxes bounds{};
    /**
     * Per-jet RCS authority, 0..1, from this step's torque solution. Indexed by (starboard bit) |
     * (fore bit): [starboard-fore, port-fore, starboard-aft, port-aft] - the four corners the
     * exporter mounts `rcs-jet` cones on. Each jet fades on its own contribution, so the pilot can
     * see which jets the sim fired.
     *
     * ponytail: four ideal corners at unit moment arm. The sim never reads the model's pod offsets,
     * so a hull whose jets are not on the corners still lights them by quadrant. Upgrade path: the
     * exporter's jet positions in the sidecar, the same way the collider shapes arrive.
     */
    Real rcsJet[4] = {0, 0, 0, 0};
    /** Seconds since the last hull-damaging contact. */
    Real contactTimer = 1;
};

/** `collider` is the drawn hull's shape set (from the model sidecar); empty means "use the table". */
ShipState create_ship(ShipClass shipClass = ShipClass::Kestrel, Collider collider = {});

Real length(const Vec2 &v);
Real distance(const Vec2 &a, const Vec2 &b);
Real heading(Real angle);
Real clampr(Real value, Real min, Real max);

/** Seconds a hull must be clear of a contact before the next one counts as a fresh impact. */
inline constexpr Real CONTACT_REARM = 0.35;

/**
 * The hull as the air sees it: drag area over mass, m^2/kg, and the stagnation radius in metres. A
 * Kestrel is 80 m of hull, so its frontal area is dominated by its length rather than its beam.
 */
inline constexpr Real DRAG_CD_AREA_OVER_MASS = 0.0016;
inline constexpr Real NOSE_RADIUS = 6.0;
/** Stagnation flux, W/m^2, that takes the heat gauge from cold to full in one second (plan 3.3). */
inline constexpr Real HEAT_FLUX_FULL = 1.2e6;

/** What a pad's base gives a ship standing on it (plan G16): slow enough to be a decision. */
inline constexpr double BASE_FUEL_PER_SECOND = 45.0;
inline constexpr double BASE_REPAIR_PER_SECOND = 4.0;

/** Newtonian planar motion, integrated with semi-implicit Euler at a fixed 120 Hz. `gravity` is the
 *  acceleration the frame imparts this step: zero in the recovery zone's co-orbiting frame, which
 *  is what keeps the μ = 0 outputs bit-identical to the original port. */
void step_ship(ShipState &state, const FlightInput &input, Real dt, const Vec2 &gravity = {});

/**
 * The cutter's beam for one frame. `out_bearing` is the aim point's offset from the nose, clamped
 * to the mount's arc; the return is the unit direction the beam fires along. The hull's forward is
 * (-sin, cos), so a bearing of zero fires straight out of the nose.
 */
Vec2 cutter_beam(Real angle, const Vec2 &muzzle, const Vec2 &aim, Real arc, Real &out_bearing);

/** One frame of cutter draw. Both terms scale with real elapsed time, never the refresh rate. */
void step_cutter(ShipState &state, Real fuelPerSecond, Real heatPerSecond, Real dt);

struct Obstacle {
    int id = 0;
    /** Broken: out of the grid, out of the scene, but still owned by its container. */
    bool retired = false;
    Real x = 0;
    Real y = 0;
    Real radius = 0;
    int seed = 0;
    Real z = 0;
    Real hp = 0;
    Real maxHp = 0;
    Real vx = 0;
    Real vy = 0;
    bool moving = false;
};

enum class CargoKind { Archive, Blackbox };

struct Cargo {
    const char *id;
    const char *name;
    CargoKind kind;
    Vec2 position;
    bool collected;
};

/** The playable volume: a 5.2 x 4.2 km slab of the Nereid recovery zone. */
struct Sector {
    Real minX;
    Real maxX;
    Real minY;
    Real maxY;
};

extern const Sector SECTOR;
extern const Vec2 STATION;
extern const Vec2 RELAY;
extern const Vec2 DERELICT;
inline constexpr Real DOCK_SPEED = 8;
inline constexpr Real DOCK_RADIUS = 270;

Real docking_radius(const ShipState &state);

/** Anything solid that is not an asteroid: the station ring and arms, the relay mast, the wreck. */
struct SolidBody {
    enum class Kind { Circle, Box };
    Kind kind;
    const char *id;
    Real x;
    Real y;
    Real radius;      // circles
    Real halfLength;  // boxes
    Real halfWidth;
    Real angle;
    Real restitution;
};

extern const SolidBody SOLID_BODIES[5];
extern const int SOLID_BODY_COUNT;

bool is_station_body(const SolidBody &body);

/** Rock integrity scales with cross-section. */
Real rock_hp(Real radius);

std::vector<Cargo> create_cargo();

std::deque<Obstacle> create_obstacles();

/** Uniform grid over the static rock field: every ship, round and beam query goes through it. */
class SpatialGrid {
public:
    explicit SpatialGrid(std::deque<Obstacle> &obstacles);
    void add(Obstacle *obstacle);
    void remove(Obstacle *obstacle);
    /** Files a moving rock again after it has been repositioned. */
    void refile(Obstacle *obstacle, Real previousX, Real previousY);
    /** Every rock whose cell touches the 3x3 neighbourhood of (x, y). */
    void near(Real x, Real y, std::vector<Obstacle *> &out) const;

    /**
     * Every rock in the cells a swept segment touches, plus one cell of margin for the largest
     * rock radius. Bounded by the segment, not by the whole field.
     */
    void near_segment(Real x0, Real y0, Real x1, Real y1, std::vector<Obstacle *> &out) const;

    /** Read-only view of the buckets, for diagnostics and the selftest's ownership check. */
    const std::unordered_map<int, std::vector<Obstacle *>> &cells() const { return cells_; }

private:
    void insert(Obstacle *obstacle);
    void remove_at(Obstacle *obstacle, Real x, Real y);
    std::unordered_map<int, std::vector<Obstacle *>> cells_;
};

/** Resolves one rock contact and returns the hull damage it cost. */
Real resolve_collision(ShipState &state, const Obstacle &rock);
/** Station arms turn with the station; the rest of the solid bodies are static. */
void set_station_spin(Real angle);
Box solid_body_box(const SolidBody &body);

/** Resolves every solid-body contact; returns the damage taken this step. */
Real resolve_bodies(ShipState &state, bool docked);

/** Whether the ground was in contact this step, and what it cost. */
struct SurfaceContact {
    bool touched = false;
    Real damage = 0;
};

/**
 * The ground under the ship: the segment between the two heightfield samples that bracket it, as a
 * box the existing collider already knows how to hit (plan 3.2). `relative` is the ship's offset
 * from the body's centre, in metres; the push-out lands in the zone frame, which shares the system
 * frame's orientation.
 */
SurfaceContact resolve_surface(ShipState &state, const glm::dvec2 &relative, const struct Body &body,
                               const struct SurfaceProfile &profile);

bool can_dock(const ShipState &state);
Real recovery_radius(const ShipState &state, const Cargo &cargo);
bool can_recover(const ShipState &state, const Cargo &cargo);

struct Ore {
    int id = 0;
    Real x = 0;
    Real y = 0;
    Real vx = 0;
    Real vy = 0;
    Real amount = 0;
    Real life = 0;
};

struct FractureResult {
    std::vector<Obstacle> fragments;
    std::vector<Ore> ore;
};

/** Breaks a rock and returns the fragments and ore it left behind. */
FractureResult fracture_rock(Obstacle &rock, int &nextId, Rng &rng);

inline constexpr Real ORE_PICKUP_RADIUS = 62;
inline constexpr Real ORE_PRICE = 4;

/** Drifting ore, collected on proximity; returns the amount taken this step. */
Real step_ore(std::vector<Ore> &ore, const ShipState &state, Real dt,
              Real pickupRadius = ORE_PICKUP_RADIUS, Real space = 1e30);

/** Moves one fragment and files it again in the grid when it crosses a cell. */
void step_fragment(Obstacle &fragment, SpatialGrid &grid, Real dt);

}  // namespace opra
