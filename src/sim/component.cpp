// The three stock hulls as component sets, and the derivation §7 defines. The sets are literal
// data: nothing here is computed at run time, and every mass and thrust below is the figure that
// makes the derivation land on the SHIPS row.
#include "sim/component.h"

#include <cmath>
#include <string_view>

namespace opra {
namespace {

bool has_prefix(const std::string &id, std::string_view prefix) {
    return id.size() >= prefix.size() &&
           std::string_view(id).compare(0, prefix.size(), prefix) == 0;
}

/**
 * The Kestrel: the core, a hab block, a fore and an aft tank, two main drives on the transom at
 * (±16, -42), and the four corner jets the exporter mounts `rcs-jet` cones on.
 *
 * ponytail: the merged hull mesh is the whole ship, so only `hull.core` names a model. Until §5.2
 * clusters the geometry into per-component meshes the drives and jets are numbers-only.
 */
const ShipDesign KESTREL{
    "Kestrel",
    {
        {.id = "hull.core", .model = "kestrel", .mount = {0, 0}, .dry_mass = 38000,
         .heat_capacity = 50, .cooling = 0.025},
        {.id = "hab.core", .mount = {0, 8}, .dry_mass = 16000, .heat_capacity = 30,
         .cooling = 0.025},
        {.id = "tank.fore", .mount = {0, 16}, .dry_mass = 4000, .propellant = 8000,
         .heat_capacity = 5},
        {.id = "tank.aft", .mount = {0, -14}, .dry_mass = 4000, .propellant = 8000,
         .heat_capacity = 5},
        {.id = "drive.main.port", .mount = {-16, -42}, .dry_mass = 9000, .thrust = 800000,
         .heat_capacity = 5, .cooling = 0.0025},
        {.id = "drive.main.starboard", .mount = {16, -42}, .dry_mass = 9000, .thrust = 800000,
         .heat_capacity = 5, .cooling = 0.0025},
        {.id = "rcs.fore.port", .mount = {-26, 5}, .dry_mass = 500, .thrust = 0.025},
        {.id = "rcs.fore.starboard", .mount = {26, 5}, .dry_mass = 500, .thrust = 0.025},
        {.id = "rcs.aft.port", .mount = {-26, -22}, .dry_mass = 500, .thrust = 0.025},
        {.id = "rcs.aft.starboard", .mount = {26, -22}, .dry_mass = 500, .thrust = 0.025},
    },
};

/** The Mule: a fat core with the hab sphere on top at (0, 24), two drive pairs on the transom, and
 *  the jets out on the (±34, ±24) sponsors. */
const ShipDesign MULE{
    "Mule",
    {
        {.id = "hull.core", .model = "mule", .mount = {0, 0}, .dry_mass = 62000,
         .heat_capacity = 60, .cooling = 0.03},
        {.id = "hab.core", .mount = {0, 24}, .dry_mass = 20000, .heat_capacity = 40,
         .cooling = 0.035},
        {.id = "tank.fore", .mount = {0, 16}, .dry_mass = 5000, .propellant = 15000,
         .heat_capacity = 5},
        {.id = "tank.aft", .mount = {0, -14}, .dry_mass = 5000, .propellant = 15000,
         .heat_capacity = 5},
        {.id = "drive.main.port", .mount = {-10, -47}, .dry_mass = 11000, .thrust = 487500,
         .heat_capacity = 10, .cooling = 0.0025},
        {.id = "drive.main.starboard", .mount = {10, -47}, .dry_mass = 11000, .thrust = 487500,
         .heat_capacity = 10, .cooling = 0.0025},
        {.id = "drive.boost.port", .mount = {-25, -46}, .dry_mass = 11000, .thrust = 487500,
         .heat_capacity = 10, .cooling = 0.0025},
        {.id = "drive.boost.starboard", .mount = {25, -46}, .dry_mass = 11000, .thrust = 487500,
         .heat_capacity = 10, .cooling = 0.0025},
        {.id = "rcs.fore.port", .mount = {-34, 24}, .dry_mass = 1500, .thrust = 0.0085},
        {.id = "rcs.fore.starboard", .mount = {34, 24}, .dry_mass = 1500, .thrust = 0.0085},
        {.id = "rcs.aft.port", .mount = {-34, -24}, .dry_mass = 1500, .thrust = 0.0085},
        {.id = "rcs.aft.starboard", .mount = {34, -24}, .dry_mass = 1500, .thrust = 0.0085},
    },
};

/** The Needle: a slim core, a hab ring, one drive on the centreline at (0, -46), and the jets in
 *  tight at (±14, 14 / -24) because there is no sponsor to hang them on. */
const ShipDesign NEEDLE{
    "Needle",
    {
        {.id = "hull.core", .model = "needle", .mount = {0, 0}, .dry_mass = 21000,
         .heat_capacity = 35, .cooling = 0.02},
        {.id = "hab.core", .mount = {0, 10}, .dry_mass = 8000, .heat_capacity = 20,
         .cooling = 0.02},
        {.id = "tank.fore", .mount = {0, 4}, .dry_mass = 2000, .propellant = 5000,
         .heat_capacity = 5, .cooling = 0.0025},
        {.id = "tank.aft", .mount = {0, -20}, .dry_mass = 2000, .propellant = 5000,
         .heat_capacity = 5, .cooling = 0.0025},
        {.id = "drive.main", .mount = {0, -46}, .dry_mass = 8000, .thrust = 1200000,
         .heat_capacity = 10, .cooling = 0.005},
        {.id = "rcs.fore.port", .mount = {-14, 14}, .dry_mass = 500, .thrust = 0.027},
        {.id = "rcs.fore.starboard", .mount = {14, 14}, .dry_mass = 500, .thrust = 0.027},
        {.id = "rcs.aft.port", .mount = {-14, -24}, .dry_mass = 500, .thrust = 0.027},
        {.id = "rcs.aft.starboard", .mount = {14, -24}, .dry_mass = 500, .thrust = 0.027},
    },
};

}  // namespace

bool is_drive(const Component &component) { return has_prefix(component.id, "drive."); }

bool is_rcs(const Component &component) { return has_prefix(component.id, "rcs."); }

Vec2 centre_of_mass(const ShipDesign &design) {
    Real mass = 0;
    Vec2 moment{};
    for (const Component &component : design.components) {
        mass += component.dry_mass;
        moment.x += component.dry_mass * component.mount.x;
        moment.y += component.dry_mass * component.mount.y;
    }
    if (mass <= 0) return {};
    return {moment.x / mass, moment.y / mass};
}

Real derived_mass(const ShipDesign &design) {
    Real mass = 0;
    for (const Component &component : design.components) mass += component.dry_mass;
    return mass;
}

Real derived_thrust(const ShipDesign &design) {
    // A component's own axis is the ship's forward, (-sin, cos), so its angle to +Y is the mount
    // angle and `cos(angle)` against the cone's own cosine is the whole test.
    Real thrust = 0;
    for (const Component &component : design.components) {
        if (!is_drive(component)) continue;
        if (std::cos(component.angle) < std::cos(DRIVE_ARC)) continue;
        thrust += component.thrust;
    }
    return thrust;
}

Real derived_torque(const ShipDesign &design) {
    // A jet's plume points outboard, so it thrusts the hull inboard along ±x (see `solve_rcs`). The
    // moment arm of an x-directed force about the centre of mass is therefore the block's
    // longitudinal offset from it - the same -y * Fx law the sim's four corner jets already obey.
    // The mounts stop mattering in x; what the centre of mass does is decide the arms.
    const Vec2 com = centre_of_mass(design);
    Real torque = 0;
    for (const Component &component : design.components) {
        if (!is_rcs(component)) continue;
        torque += component.thrust * std::abs(component.mount.y - com.y);
    }
    return torque;
}

Real derived_fuel(const ShipDesign &design) {
    Real fuel = 0;
    for (const Component &component : design.components) fuel += component.propellant;
    return fuel;
}

Real derived_hull(const ShipDesign &design) {
    Real hull = 0;
    for (const Component &component : design.components) hull += component.heat_capacity;
    return hull;
}

Real derived_cooling(const ShipDesign &design) {
    Real cooling = 0;
    for (const Component &component : design.components) cooling += component.cooling;
    return cooling;
}

ShipSpec derive_spec(const ShipDesign &design) {
    ShipSpec spec{};
    spec.name = design.name.c_str();
    spec.role = "";
    spec.mass = derived_mass(design);
    spec.thrust = derived_thrust(design);
    spec.fuel = derived_fuel(design);
    spec.torque = derived_torque(design);
    spec.hull = derived_hull(design);
    spec.length = 0;  // P4 fills this from the concatenated collider's bounds.
    spec.cargo = 0;
    spec.cooling = derived_cooling(design);
    spec.scanScale = 1;
    spec.collectScale = 1;
    return spec;
}

// The collider and the ports deliberately do not appear above: P4 walks this same component list
// once per hull (each component's `model` sidecar carries its shapes) and P8 does it for the ports,
// so a walk here would be that one seam implemented twice. `model` stays unread until they arrive.

const ShipDesign &stock_design(ShipClass shipClass) {
    switch (shipClass) {
    case ShipClass::Mule: return MULE;
    case ShipClass::Needle: return NEEDLE;
    case ShipClass::Kestrel: break;
    }
    return KESTREL;
}

}  // namespace opra
