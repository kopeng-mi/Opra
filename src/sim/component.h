// Modular ships (PLAN-02 §7): a hull is a set of components and everything the sim reads about it
// is derived from that set, never authored per hull. P13 is the data model only — the editor, the
// per-component meshes and the collider/port concatenation arrive later.
#pragma once

#include <array>
#include <string>
#include <unordered_map>
#include <vector>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include "core/units.h"
#include "sim/physics.h"

namespace opra {

/**
 * One module of a design, placed in the ship's frame: metres, nose +Y, starboard +X, so an `angle`
 * of zero points the part's own nose the same way as the ship's. The mount is a flange position
 * (plan-04 A6): the module's own model carries the s2.5 flange at its attachment face, and the
 * place to stack a hull module is the previous module's flange one 4 m pitch along, so a design is
 * a stack of parts that share one bolt pattern. The exporter derives the module's collider and
 * ports from the same model, so a set that respects the flange pitch never interpenetrates.
 */
struct Component {
    /** Dotted kind tag. The prefix is the kind: "drive." is a drive, "rcs." a corner jet. */
    std::string id;
    /**
     * Model name. The model carries its own mesh, collider shapes and ports; the derivation never
     * reads this field. Empty is numbers-only: the exporter emits one merged mesh per stock hull,
     * so only the hull's core module names a model until §5.2's clustering splits them apart.
     */
    std::string model;
    /** Attachment point in the parent's frame. */
    Vec2 mount;
    Real angle = 0;
    // Everything the sim reads is derived from the set, never authored per ship:
    Real dry_mass = 0;
    Real propellant = 0;
    Real thrust = 0;
    /**
     * A module's own yaw authority (a reaction wheel). The §7 sums read an RCS block's thrust x
     * moment arm, not this field, so every stock module carries zero.
     */
    Real torque = 0;
    /** Hull integrity the module contributes: the derivation sums it into `ShipSpec::hull`. */
    Real heat_capacity = 0;
    Real cooling = 0;
};

// ---------------------------------------------------------------- Spine kit (plan 07 §2, §3)

enum class SpineFamily { Truss, Keel, Monocoque };

/** Layer-neutral part specification, mirroring the sidecar's "part" block (§2.1, §7.3). */
struct PartSpec {
    Real dry_mass = 0, propellant = 0, thrust = 0, cooling = 0, heat_capacity = 0;
    Real rcs_authority = 0;
    int rcs_jets = 0, span = 1;
    bool axial = true;
    std::string kind;
};
using PartTable = std::unordered_map<std::string, PartSpec>;

enum class Facing : uint8_t {
    Fore = 0,
    Aft = 1,
    Starboard = 2,
    Port = 3,
    Dorsal = 4,
    Ventral = 5
};

inline glm::dvec3 face_dir(Facing f) {
    switch (f) {
        case Facing::Fore: return {0.0, 1.0, 0.0};
        case Facing::Aft: return {0.0, -1.0, 0.0};
        case Facing::Starboard: return {1.0, 0.0, 0.0};
        case Facing::Port: return {-1.0, 0.0, 0.0};
        case Facing::Dorsal: return {0.0, 0.0, 1.0};
        case Facing::Ventral: return {0.0, 0.0, -1.0};
    }
    return {0.0, 1.0, 0.0};
}

inline bool is_axial(Facing f) {
    return f == Facing::Fore || f == Facing::Aft;
}

inline Facing opposite_facing(Facing f) {
    switch (f) {
        case Facing::Starboard: return Facing::Port;
        case Facing::Port: return Facing::Starboard;
        case Facing::Dorsal: return Facing::Ventral;
        case Facing::Ventral: return Facing::Dorsal;
        default: return f;
    }
}

inline glm::dquat q_face(Facing f) {
    constexpr double kPi = 3.14159265358979323846;
    switch (f) {
        case Facing::Fore: return glm::dquat(1.0, 0.0, 0.0, 0.0);
        case Facing::Aft: return glm::angleAxis(kPi, glm::dvec3(0.0, 0.0, 1.0));
        case Facing::Starboard: return glm::angleAxis(-kPi * 0.5, glm::dvec3(0.0, 0.0, 1.0));
        case Facing::Port: return glm::angleAxis(kPi * 0.5, glm::dvec3(0.0, 0.0, 1.0));
        case Facing::Dorsal: return glm::angleAxis(kPi * 0.5, glm::dvec3(1.0, 0.0, 0.0));
        case Facing::Ventral: return glm::angleAxis(-kPi * 0.5, glm::dvec3(1.0, 0.0, 0.0));
    }
    return glm::dquat(1.0, 0.0, 0.0, 0.0);
}

struct ChainDef {
    std::string name = "chain";
    std::string family = "truss";
    union {
        int slots = 12;
        int stations;
    };
    Real pitch = 4.0;
    Real half_width = 1.6;
    Real recess = 0.0;
    Real mass = 0.0;
    Real heat_capacity = 0.0;
    Real cooling = 0.0;

    ChainDef() : slots(12) {}
    ChainDef(const ChainDef &o) = default;
    ChainDef &operator=(const ChainDef &o) = default;
    ChainDef(ChainDef &&o) noexcept = default;
    ChainDef &operator=(ChainDef &&o) noexcept = default;
};
using SpineDef = ChainDef;

struct Mount {
    glm::dvec3 pos{0.0};
    glm::dquat rot{1.0, 0.0, 0.0, 0.0};
};

struct Placement {
    std::string part;
    union {
        int slot = 0;
        int station;
    };
    Facing facing = Facing::Aft;
    int roll = 0;
    int span = 1;
    bool axial = false;
    int stack_index = 0;
    int group = 0;
    bool destroyed = false;

    Placement() : slot(0) {}
    Placement(const Placement &o) = default;
    Placement &operator=(const Placement &o) = default;
    Placement(Placement &&o) noexcept = default;
    Placement &operator=(Placement &&o) noexcept = default;
};

inline Mount mount_transform(const ChainDef &chain, const Placement &p) {
    Mount m;
    const Real L = static_cast<Real>(chain.slots) * chain.pitch;
    const Real plane_fore = L * 0.5 - static_cast<Real>(p.slot) * chain.pitch;
    const Real plane_aft  = L * 0.5 - static_cast<Real>(p.slot + 1) * chain.pitch;
    const Real centre_y   = L * 0.5 - (static_cast<Real>(p.slot) + 0.5) * chain.pitch;

    if (p.facing == Facing::Fore) {
        m.pos = glm::dvec3(0.0, plane_aft, 0.0);
    } else if (p.facing == Facing::Aft) {
        m.pos = glm::dvec3(0.0, plane_fore, 0.0);
    } else {
        const Real offset_radial = chain.half_width - chain.recess;
        m.pos = glm::dvec3(0.0, centre_y, 0.0) + face_dir(p.facing) * offset_radial;
    }

    const glm::dquat q_f = q_face(p.facing);
    constexpr double kPi = 3.14159265358979323846;
    const Real roll_rad = static_cast<Real>(p.roll) * (kPi * 0.5);
    const glm::dquat q_roll = glm::angleAxis(roll_rad, glm::dvec3(0.0, 1.0, 0.0));
    m.rot = q_f * q_roll;
    return m;
}

using OccupancyGrid = std::array<bool, 96>;

inline size_t grid_index(int slot, Facing facing) {
    const int lane = is_axial(facing) ? static_cast<int>(Facing::Fore) : static_cast<int>(facing);
    return static_cast<size_t>(slot * 6 + lane);
}

OccupancyGrid design_occupancy(const ChainDef &chain, const std::vector<Placement> &placements);
bool can_mount(const ChainDef &chain, const std::vector<Placement> &placements, const Placement &p);

/** A named component set. Holds both legacy components and chain-based placements. */
struct ShipDesign {
    std::string name;
    std::vector<Component> components;
    ChainDef spine;
    std::vector<Placement> placements;

    std::string role;
    std::string shipClass;
    int slots = 12;
    Real pitch = 4.0;
    Real half_width = 1.6;
    Real recess = 0.0;
    Real scale = 1.0;
    Real cargo = 0;
    Real scan_scale = 1.0;
    Real collect_scale = 1.0;
};

bool mount_placement(ShipDesign &design, const Placement &p, bool mirror = false);

/** The cone a drive's axis has to sit in to push the hull: 15° either side of +Y. */
inline constexpr Real DRIVE_ARC = 15.0 * 3.14159265358979323846 / 180.0;

/** Fuel flow at full throttle (kg/s, physics.cpp:128). */
constexpr Real FUEL_MDOT = 14.0;

inline Real calculate_delta_v(Real thrust, Real dry_mass, Real wet_mass) {
    if (thrust <= 0 || wet_mass <= dry_mass * (1.0 + 1e-7)) return 0;
    const Real isp_g0 = thrust / FUEL_MDOT;
    return isp_g0 * std::log(wet_mass / dry_mass);
}

inline Real calculate_twr(Real thrust, Real wet_mass, Real g = 9.80665) {
    if (wet_mass <= 0 || g <= 0) return 0;
    return thrust / (wet_mass * g);
}

/** Display thermal model scale (PLAN-08 §8.4): 350 kW per unit of ShipSpec::cooling. */
constexpr Real HEAT_SCALE = 350.0;

inline Real calculate_heat_load(Real thrust) {
    return thrust * 1.2e-5;
}

inline Real calculate_thermal_margin(Real cooling, Real heat_load) {
    return (cooling * HEAT_SCALE) - heat_load;
}

/** True for a drive: an id under the "drive." prefix. */
bool is_drive(const Component &component);

/** True for an RCS block: an id under the "rcs." prefix. */
bool is_rcs(const Component &component);

/** Dry-mass centre of the design, from the mount points. Zero for a design with no dry mass. */
Vec2 centre_of_mass(const ShipDesign &design, const PartTable &parts);
Vec2 centre_of_mass(const ShipDesign &design);

Real derived_mass(const ShipDesign &design);

/** Only drives inside `DRIVE_ARC` of +Y count; a canted or retrograde one adds nothing. */
Real derived_thrust(const ShipDesign &design);

/** RCS thrust x the block's moment arm about `centre_of_mass`, summed. */
Real derived_torque(const ShipDesign &design);

Real derived_fuel(const ShipDesign &design);

/** Hull integrity: the summed component heat capacity. */
Real derived_hull(const ShipDesign &design);

Real derived_cooling(const ShipDesign &design);

ShipSpec derive_spec(const ShipDesign &design, const PartTable &parts);
ShipSpec derive_spec(const ShipDesign &design);

/** A stock hull as a component set. `derived_*` over this must reproduce the SHIPS row. */
const ShipDesign &stock_design(ShipClass shipClass);

// ------------------------------------------------------- plan 05 s6.2's mount-time acceptance

/** True when the design carries at least one drive: a design without one is flagged, never
 *  silently allowed to fly (s6.2). */
bool design_has_drive(const ShipDesign &design, const PartTable &parts);
bool design_has_drive(const ShipDesign &design);

/**
 * Mounts a component into a design, rejecting overlaps: two parts closer than the 4 m flange
 * pitch cannot share a hull. Returns false without mounting when the mount would overlap.
 */
bool mount_component(ShipDesign &design, const Component &component);

/** Thrust-to-weight at `body`, for the refit's warning: a number, not a rule. */
Real design_twr(const ShipDesign &design, const struct Body &body);

}  // namespace opra
