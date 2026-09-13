// The live sector: the ship, the rock field, fragments, ore and cargo, stepped at a fixed rate.
// Owns the storage the spatial grid points into, so no grid entry can outlive its rock.
#pragma once

#include <deque>
#include <string>
#include <deque>
#include <vector>

#include "sim/combat.h"
#include "sim/component.h"
#include "sim/dock.h"
#include "sim/descent.h"
#include "sim/terrain.h"
#include "sim/physics.h"
#include "sim/system.h"

#include "orbit/transfer.h"

namespace opra {

/** Something the pilot can select with Tab: cargo, the station, the mast, the wreck. */
struct Contact {
    std::string name;
    Vec2 position;
    bool is_cargo = false;
    int cargo_index = -1;
    /** Stable across rebuilds of the list: the cargo index, or one of the marks below. */
    int id = -1;
};

/** The fixed marks. Negative so they never collide with a cargo index. */
inline constexpr int CONTACT_STATION = -2;
inline constexpr int CONTACT_RELAY = -3;
inline constexpr int CONTACT_DERELICT = -4;

/** Where the id sits in the list, or -1. Ids are stable; positions in the list are not. */
int contact_index(const std::vector<Contact> &list, int id);

/** An arc of a body the survey has mapped, in body-fixed angle (plan 3.7). */
struct SurveyArc {
    double from = 0.0;
    double to = 0.0;
};

/** A satellite waiting on its orbit check, or past it (plan 3.7). */
struct Satellite {
    std::string contract;
    int body = -1;
    orbit::Elements elements{};
    double deployed_at = 0.0;
    /** The orbit the contract asked for, as a tolerance box on (a, e). */
    double box_a = 0.0;
    double box_e = 0.0;
    /** False until one full period has passed: a marginal orbit fails honestly, not at release. */
    bool checked = false;
    bool valid = false;
};

struct World {
    /**
     * The star system this sector belongs to. Everything in the zone - rocks, structures, cargo -
     * is an offset from `anchor`, which is the body the sector co-orbits, so the local coordinates
     * stay small and exact while the system itself is Gm-scale. Empty for a bare test world: no
     * system means no gravity, which is what keeps the port's golden values valid.
     */
    SystemDef system;
    std::vector<BodyState> bodies;
    /** Index of the body the zone co-orbits (the station), or -1. */
    int anchor_body = -1;
    /** The anchor's barycentric state at `elapsed`: the zone frame's own place in the system. */
    BodyState anchor;
    /** The deepest body holding the ship, from the last step. -1 means the star. */
    int primary = -1;

    ShipState ship = create_ship(ShipClass::Kestrel);
    ShipDesign design;
    ShipSpec derived{};
    void rebuild_from_design(const PartTable &parts);
    void rebuild_from_design();
    /** A deque, like the fragments: the grid holds pointers into it, so erasing one body
     *  must not move the others. */
    std::deque<Obstacle> rocks = create_obstacles();
    /**
     * Fragments live in a deque: the grid stores raw pointers into this storage, a vector's
     * reallocation would dangle every one of them, and a deque never moves its elements.
     */
    std::deque<Obstacle> fragments;
    std::vector<Ore> ore;
    std::vector<Cargo> cargos = create_cargo();
    SpatialGrid grid{rocks};
    Rng rng{4712};
    double elapsed = 0.0;
    double oreHeld = 0.0;
    double stationSpin = 0.0;
    bool docked = false;
    int next_id = 1000;
    /** The tracked contact, by Contact::id - never by position in contacts(). */
    int target = -1;

    // -------------------------------------------------------- worlds (plan 03 G11-G16)
    /** Terrain per body, in SystemDef::bodies order; a body without terrain has an empty profile. */
    std::vector<SurfaceProfile> surfaces;
    /** The arcs the survey has mapped, per body (plan 3.7). */
    std::vector<std::vector<SurveyArc>> scanned;
    /** Deployed satellites and their orbit checks (plan 3.7). */
    std::vector<Satellite> satellites;
    /** The survey's discoveries (plan 05 s6.1); `found` is permanent for the run. A derelict, an
     *  anomalous mass or a cached depot the scan sweep crossed: positions are deterministic in the
     *  body's own seed, so a system file describes them and nothing is hardcoded. */
    struct Discovery {
        std::string name;
        std::string kind;  // "derelict", "anomaly", "depot"
        int body = -1;
        double theta = 0.0;
        bool found = false;
    };
    std::vector<Discovery> discoveries;
    /** The body the ship is standing on, or -1 while it is flying (plan 3.4). */
    int landed_body = -1;
    /** Seconds of simulator time since touchdown; -1 while flying. */
    double landed_for = -1.0;
    /**
     * The air at the ship this step: kg/m^3, W/m^2 at the stagnation point, and metres above the
     * ground. Zero and -inf in vacuum. The HUD, the map and the warp rail all read these.
     */
    double air_density = 0.0;
    double air_flux = 0.0;
    double air_altitude = 0.0;
    /** The body whose air and ground the ship is inside, or -1: the primary's, when it has any. */
    int air_body = -1;
    /** The gate as it stood at the last ground contact: what the HUD and the director read. */
    TouchdownGate touchdown;
    /** The base the ship is standing on, or -1: refuelling and repairing while it stays (plan G16). */
    int base_body = -1;
    std::string base_name;

    std::vector<Contact> contacts() const;

    // -------------------------------------------------------------- close quarters (plan 05 s5)
    /** Rounds and torpedoes in flight. Any of either forces the warp rail to 1x (s2.7). */
    std::vector<Round> rounds;
    std::vector<Torpedo> torpedoes;
    /** The PDC mounts, ship-frame metres, read from the hull's sidecar hardpoints. */
    std::vector<Vec2> pdc_mounts;
    /** True when the PDCs may engage the tracked contact: the pilot's weapons release. */
    bool weapons_free = false;

    /** One combat step: PDC engagement, round motion, torpedo guidance, hit application. */
    void step_combat(Real dt);

    /** Fires one torpedo at the tracked contact, or does nothing without one. */
    bool fire_torpedo();

    /** Rounds or torpedoes in flight: the warp rail reads this every frame (s2.7). */
    bool combat_active() const { return !rounds.empty() || !torpedoes.empty(); }

    // ---------------------------------------------------------------- docking (E9, plan 3.6)
    /** The station's ports in the zone frame, as the model exported them, in metres. */
    std::vector<Port> station_ports;
    /** The ship's own port: the one it berths with, in the hull's frame. */
    Port ship_port;
    /** Index of the port being approached, or -1. */
    int target_port = -1;
    /** Where the ship's port is and which way it faces, this frame. */
    PortState ship_port_state;
    /** Where the target port is and which way it faces, this frame. */
    PortState target_state;
    /** The gate's five terms, as the HUD reads them. */
    ApproachGate gate;
    /** The capture in progress, when `capture_started >= 0`. */
    Capture capture;
    double capture_started = -1.0;
    /** Seconds of simulator time the ship has been hard docked; -1 while it is flying. */
    double docked_for = -1.0;

    /** Sets the sector's ports from the exported models (model units, scaled to metres). */
    void set_ports(const std::vector<Port> &station, const Port &hull);

    /** Releases the ship: a 0.5 m/s separation impulse along the target port's normal. */
    void undock();

    // ------------------------------------------------------------- the maneuver plan (plan 3.5)
    /** Burns in time order, in the orbital frame at each burn: the planner's whole state. */
    std::vector<orbit::Node> nodes;

    /** Applies every node whose time has come to the ship's state, and drops it from the plan. */
    void run_nodes();

    /** The conic the plan produces from here: the predicted trajectory the map draws. */
    orbit::Elements planned_conic() const;

    /** Advances to the next node's time, exactly: the rail is what makes this a warp, not a skip. */
    bool warp_to_next_node();

    /** One frame of approach, capture and hard dock. Called from step and warp_step's caller. */
    void update_docking(Real dt);

    /** Takes ownership of a loaded system and starts the zone co-orbiting `anchor_index`. */
    void attach_system(const SystemDef &loaded, int anchor_index);

    /** The ship's state in the barycentric frame: the anchor's own state plus the zone offset. */
    glm::dvec2 system_position() const;
    glm::dvec2 system_velocity() const;

    /**
     * A body's state in the ZONE frame - barycentric minus the anchor - which is the frame the
     * camera, the ship and every drawn mark live in. Plan 05's overlay and deep pass drew planets
     * at barycentric positions into a zone-frame camera and put the whole sky a quadrant away;
     * these two are the one conversion, named once.
     */
    glm::dvec2 body_zone_position(int index) const {
        return bodies[static_cast<size_t>(index)].position - anchor.position;
    }
    glm::dvec2 body_zone_velocity(int index) const {
        return bodies[static_cast<size_t>(index)].velocity - anchor.velocity;
    }

    /**
     * The acceleration the ship's *frame* feels, i.e. the gravity at the ship minus the gravity at
     * the anchor. The zone co-orbits its anchor, so the star's own pull cancels and what is left is
     * the tidal field: metres per second squared, small, and exactly what a co-orbiting frame
     * should show. Zero when no system is attached.
     *
     * Not const: the sphere-of-influence walk carries hysteresis, which is state, and a ship
     * skimming a boundary must not switch primaries every frame.
     */
    Vec2 gravity();

    void step(const FlightInput &input, Real dt);

    /** Drag and stagnation heating from the primary's air, if it has any (plan 3.3). */
    void apply_air(Real dt);

    /** The primary's ground: contact, then the touchdown gate (plan 3.2, 3.4). */
    void apply_ground(Real dt);

    /**
     * Railed advance: the ship rides its osculating conic for `seconds` without integrating at all.
     * This is what time warp above 10x does, where a fixed-step integrator would have to run
     * thousands of times and would still drift. Exact at any step, which is the whole reason the
     * ship is put on a conic in the first place.
     */
    void warp_step(Real seconds);

    /** True when the hull touched something within the last CONTACT_REARM seconds. */
    bool contact_recent() const { return ship.contactTimer < CONTACT_REARM; }

    /** Set when the last step crossed a sphere-of-influence boundary: warp drops to 1x on it. */
    bool soi_switched = false;

    /** Breaks a rock, files its fragments in the grid and drops its ore. */
    /** Breaks a body open: it leaves the grid and spawns fragments and ore. */
    void break_rock(Obstacle &rock);
};

}  // namespace opra
