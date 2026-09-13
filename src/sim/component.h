// Modular ships (PLAN-02 §7): a hull is a set of components and everything the sim reads about it
// is derived from that set, never authored per hull. P13 is the data model only — the editor, the
// per-component meshes and the collider/port concatenation arrive later.
#pragma once

#include <string>
#include <vector>

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

/** A named component set. No hierarchy yet: every mount is already in the ship's frame. */
struct ShipDesign {
    std::string name;
    std::vector<Component> components;
};

/** The cone a drive's axis has to sit in to push the hull: 15° either side of +Y. */
inline constexpr Real DRIVE_ARC = 15.0 * 3.14159265358979323846 / 180.0;

/** True for a drive: an id under the "drive." prefix. */
bool is_drive(const Component &component);

/** True for an RCS block: an id under the "rcs." prefix. */
bool is_rcs(const Component &component);

/** Dry-mass centre of the design, from the mount points. Zero for a design with no dry mass. */
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

/**
 * The six derived figures packed into a `ShipSpec`. `role`, `length`, `cargo` and the two scales do
 * not come from a component set — P4 fills `length` from the concatenated collider's bounds — so
 * they arrive empty, zero and 1. `name` aliases `design.name`: the design must outlive the spec.
 */
ShipSpec derive_spec(const ShipDesign &design);

/** A stock hull as a component set. `derived_*` over this must reproduce the SHIPS row. */
const ShipDesign &stock_design(ShipClass shipClass);

// ------------------------------------------------------- plan 05 s6.2's mount-time acceptance

/** True when the design carries at least one drive: a design without one is flagged, never
 *  silently allowed to fly (s6.2). */
bool design_has_drive(const ShipDesign &design);

/**
 * Mounts a component into a design, rejecting overlaps: two parts closer than the 4 m flange
 * pitch cannot share a hull. Returns false without mounting when the mount would overlap.
 */
bool mount_component(ShipDesign &design, const Component &component);

/** Thrust-to-weight at `body`, for the refit's warning: a number, not a rule. */
Real design_twr(const ShipDesign &design, const struct Body &body);

}  // namespace opra
