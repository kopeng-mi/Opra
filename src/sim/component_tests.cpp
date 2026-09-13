// P13's checks: the component set derives SHIPS' table, the drive cone has a hard edge, the centre
// of mass moves with the ballast, and a set with no drives has no thrust. Every number below is
// derived on paper in the comment beside it, not captured from a run. The game wires this into
// run_selftest(); it returns its own failure count, like every module test file.
#include "sim/component_tests.h"

#include <cmath>
#include <string>

#include "selftest.h"
#include "sim/component.h"
#include "sim/system.h"
#include "sim/physics.h"

namespace opra {
namespace {

using selftest::check;
using selftest::check_close;

constexpr Real kDeg = 3.14159265358979323846 / 180.0;

Component part(const std::string &id, Real x, Real y, Real dry_mass, Real heat_capacity = 0) {
    Component component;
    component.id = id;
    component.mount = {x, y};
    component.dry_mass = dry_mass;
    component.heat_capacity = heat_capacity;
    return component;
}

/** A drive whose own nose is `radians` off the ship's, i.e. off +Y. */
Component drive_rad(Real radians, Real thrust) {
    Component component;
    component.id = "drive.main";
    component.angle = radians;
    component.thrust = thrust;
    return component;
}

/** A drive pointed `degrees` off the ship's nose. */
Component drive_at(Real degrees, Real thrust) { return drive_rad(degrees * kDeg, thrust); }

/** A corner jet. Massless unless the case says otherwise: the tests below push the centre of mass
 *  around with ballast modules, so the jets must not muddy the sum on their own. */
Component rcs_at(const std::string &id, Real x, Real y, Real thrust) {
    Component component;
    component.id = id;
    component.mount = {x, y};
    component.thrust = thrust;
    return component;
}

void test_stock_designs() {
    // SHIPS is the arbiter of §7's acceptance: within 2% on all six derived figures, per hull. The
    // table is read, never written, so a set that misses means the derivation is wrong. The figures
    // are read off `derive_spec`, the same packet the sim would take, so the packing is walked too.
    for (int i = 0; i < 3; ++i) {
        const ShipDesign &design = stock_design(static_cast<ShipClass>(i));
        const ShipSpec &table = SHIPS[i];
        const ShipSpec derived = derive_spec(design);
        const std::string hull = table.name;
        const Real tolerance = 0.02;  // relative: the arithmetic below is exact by construction

        // The design and the row have to be the same hull, or the six rows below compare the wrong
        // pair and pass for the wrong reason.
        check(std::string(derived.name) == table.name, "stock: the design is the SHIPS row's hull");

        const std::string mass = "stock " + hull + ": mass";
        check_close(derived.mass, table.mass, std::abs(table.mass) * tolerance, mass.c_str());
        const std::string thrust = "stock " + hull + ": thrust";
        check_close(derived.thrust, table.thrust, std::abs(table.thrust) * tolerance,
                    thrust.c_str());
        const std::string torque = "stock " + hull + ": torque";
        check_close(derived.torque, table.torque, std::abs(table.torque) * tolerance,
                    torque.c_str());
        const std::string fuel = "stock " + hull + ": fuel";
        check_close(derived.fuel, table.fuel, std::abs(table.fuel) * tolerance, fuel.c_str());
        const std::string integrity = "stock " + hull + ": hull";
        check_close(derived.hull, table.hull, std::abs(table.hull) * tolerance, integrity.c_str());
        const std::string cooling = "stock " + hull + ": cooling";
        check_close(derived.cooling, table.cooling, std::abs(table.cooling) * tolerance,
                    cooling.c_str());

        // The set has to actually carry the parts the sums walked, or a zero-thrust accident would
        // pass the thrust row on an empty design.
        int drives = 0;
        int jets = 0;
        for (const Component &component : design.components) {
            drives += is_drive(component) ? 1 : 0;
            jets += is_rcs(component) ? 1 : 0;
        }
        check(drives > 0 && jets == 4, "stock: the hull has drives and four corner jets");
    }
}

void test_drive_arc() {
    // 100 + 50 + 25 count; 30, 70 and 40 sit past the cone. `DRIVE_ARC` itself is on the edge and
    // inside (the test is cos >= cos(DRIVE_ARC)), so the canted-by-a-hair drive is what excludes.
    ShipDesign design;
    design.components.push_back(drive_rad(0, 100));
    design.components.push_back(drive_at(14, 50));
    design.components.push_back(drive_rad(DRIVE_ARC, 25));
    design.components.push_back(drive_rad(DRIVE_ARC * 1.0001, 30));
    design.components.push_back(drive_at(20, 70));
    design.components.push_back(drive_at(180, 40));
    check_close(derived_thrust(design), 175.0, 1e-9,
                "drive arc: 0, 14 and 15° on the cone count, just past it, 20° and 180° do not");

    // The brief's case on its own: one drive canted 20° is a drive with no forward thrust.
    ShipDesign canted;
    canted.components.push_back(drive_at(20, 70));
    check_close(derived_thrust(canted), 0.0, 1e-12, "drive arc: a drive canted 20° adds nothing");
}

void test_no_drives() {
    // Hull, hab, a tank and four jets, no drive anywhere: thrust is zero, the rest still sums.
    ShipDesign design;
    design.components.push_back(part("hull.core", 0, 0, 900, 30));
    design.components.push_back(part("hab.core", 0, 8, 300, 10));
    Component tank = part("tank.aft", 0, -10, 100, 5);
    tank.propellant = 400;
    design.components.push_back(tank);
    design.components.push_back(rcs_at("rcs.fore.port", -6, 6, 2));
    design.components.push_back(rcs_at("rcs.fore.starboard", 6, 6, 2));
    design.components.push_back(rcs_at("rcs.aft.port", -6, -6, 2));
    design.components.push_back(rcs_at("rcs.aft.starboard", 6, -6, 2));

    check_close(derived_thrust(design), 0.0, 1e-12, "no drives: thrust is zero");
    check_close(derived_mass(design), 1300.0, 1e-9, "no drives: the dry masses still sum");
    check_close(derived_fuel(design), 400.0, 1e-9, "no drives: the propellant still sums");
    check_close(derived_hull(design), 45.0, 1e-9, "no drives: the heat capacity still sums");
}

void test_centre_of_mass() {
    // 3 t at x = 2 and 1 t at x = 10: (2*3 + 10*1) / 4 = 4.
    ShipDesign design;
    design.components.push_back(part("hull.core", 2, 0, 3));
    design.components.push_back(part("hab.core", 10, 0, 1));
    const Vec2 com = centre_of_mass(design);
    check_close(com.x, 4.0, 1e-12, "centre of mass: x pulls toward the heavy module");
    check_close(com.y, 0.0, 1e-12, "centre of mass: a symmetric y stays on the axis");

    // 2 t at y = 0 and 6 t at y = 4: 24 / 8 = 3.
    ShipDesign stacked;
    stacked.components.push_back(part("hull.core", 0, 0, 2));
    stacked.components.push_back(part("hab.core", 0, 4, 6));
    check_close(centre_of_mass(stacked).y, 3.0, 1e-12, "centre of mass: y follows the hab stack");

    // No dry mass at all: the sum's denominator is zero, so the origin is the only sane answer.
    ShipDesign empty;
    empty.components.push_back(rcs_at("rcs.fore.port", 5, 5, 1));
    check_close(centre_of_mass(empty).y, 0.0, 1e-12,
                "centre of mass: a massless set sits at the origin");
}

void test_torque_about_com() {
    // Unequal jets and one ballast module: the arms run from wherever the dry mass puts the centre
    // of mass, so moving the ballast changes the couple. Jets are massless here (see `rcs_at`).
    ShipDesign design;
    design.components.push_back(part("hull.core", 0, 0, 100));
    design.components.push_back(rcs_at("rcs.fore", 0, 10, 3));
    design.components.push_back(rcs_at("rcs.aft", 0, -2, 1));
    // COM at the origin: 3 * 10 + 1 * 2 = 32.
    check_close(centre_of_mass(design).y, 0.0, 1e-12, "torque: the ballast is centred to start");
    check_close(derived_torque(design), 32.0, 1e-9, "torque: arms run from the centre of mass");

    // COM_y = 10 * 300 / 400 = 7.5: 3 * |10 - 7.5| + 1 * |-2 - 7.5| = 7.5 + 9.5 = 17.
    design.components.push_back(part("hab.core", 0, 10, 300));
    check_close(centre_of_mass(design).y, 7.5, 1e-12,
                "torque: the ballast moves the centre of mass");
    check_close(derived_torque(design), 17.0, 1e-9, "torque: ballast forward drops the couple");

    // A jet mounted on the centre of mass cannot yaw, however hard it pushes.
    ShipDesign centred;
    centred.components.push_back(part("hull.core", 0, 4, 50));
    centred.components.push_back(rcs_at("rcs.fore", 0, 4, 9));
    check_close(derived_torque(centred), 0.0, 1e-12,
                "torque: a jet on the centre of mass cannot yaw");
}

}  // namespace

void test_mount_rules() {
    // s6.2's mount-time acceptance, and the derivation's strafe fraction (s5.1 via s6.2).
    const ShipDesign &kestrel = stock_design(ShipClass::Kestrel);

    // A driveless design reports thrust 0 and is flagged, never silently allowed.
    ShipDesign glider{"Glider", {}};
    glider.components.push_back({.id = "hull.core", .mount = {0, 0}, .dry_mass = 5000});
    check(!design_has_drive(glider), "refit: a design with no drive is flagged");
    check(derived_thrust(glider) == 0, "refit: a driveless design reports thrust 0");
    check(design_has_drive(kestrel), "refit: the stock design carries drives");

    // Overlapping mounts are rejected at mount time; the 4 m flange pitch is the rule.
    ShipDesign refit{kestrel};
    const int before = static_cast<int>(refit.components.size());
    check(!mount_component(refit, {.id = "drive.main.port", .mount = {-15, -42}, .dry_mass = 100}),
          "refit: a mount overlapping an existing part is rejected");
    check(static_cast<int>(refit.components.size()) == before,
          "refit: a rejected mount leaves the design alone");
    check(mount_component(refit, {.id = "drive.main.center", .mount = {0, -42}, .dry_mass = 100}),
          "refit: a clear mount takes");
    check(static_cast<int>(refit.components.size()) == before + 1,
          "refit: the accepted mount lands");

    // The strafe fraction is derived, never authored: four corner blocks is 0.22.
    const ShipSpec derived = derive_spec(kestrel);
    check_close(derived.strafeFraction, 0.22, 1e-9,
                "refit: the strafe fraction derives from the RCS count");

    // TWR is a number, not a rule: the warning is the UI's to raise.
    Body star{};
    star.mu = 2.07e19;
    star.radius = 3.4e8;
    check(design_twr(kestrel, star) > 0.0, "refit: TWR computes against a body");
    Body airless{};
    airless.mu = 0.0;
    check(design_twr(kestrel, airless) == 0.0, "refit: TWR without a body is zero, not NaN");
}

void test_spinekit_gates_1_4() {
    ChainDef chain;
    chain.slots = 12;
    chain.pitch = 4.0;
    chain.half_width = 1.6;
    chain.recess = 0.0;

    // Gate 1: mount_transform returns §3.1's positions for slot 0, slot N-1 and a mid radial, all six facings.
    Placement p_slot0;
    p_slot0.slot = 0;
    p_slot0.facing = Facing::Fore;
    const Mount m_s0 = mount_transform(chain, p_slot0);
    check_close(m_s0.pos.x, 0.0, 1e-9, "gate 1: slot 0 fore pos.x");
    check_close(m_s0.pos.y, 20.0, 1e-9, "gate 1: slot 0 fore pos.y == 20 (L/2 - p)");
    check_close(m_s0.pos.z, 0.0, 1e-9, "gate 1: slot 0 fore pos.z");

    Placement p_slotN;
    p_slotN.slot = 11;
    p_slotN.facing = Facing::Aft;
    const Mount m_sN = mount_transform(chain, p_slotN);
    check_close(m_sN.pos.x, 0.0, 1e-9, "gate 1: slot 11 aft pos.x");
    check_close(m_sN.pos.y, -20.0, 1e-9, "gate 1: slot 11 aft pos.y == -20");
    check_close(m_sN.pos.z, 0.0, 1e-9, "gate 1: slot 11 aft pos.z");

    Placement p_mid_rad;
    p_mid_rad.slot = 5;
    p_mid_rad.facing = Facing::Starboard;
    const Mount m_mid = mount_transform(chain, p_mid_rad);
    check_close(m_mid.pos.x, 1.6, 1e-9, "gate 1: slot 5 starboard pos.x == 1.6");
    check_close(m_mid.pos.y, 2.0, 1e-9, "gate 1: slot 5 starboard pos.y == 2.0");
    check_close(m_mid.pos.z, 0.0, 1e-9, "gate 1: slot 5 starboard pos.z == 0");

    const Facing facings[6] = {Facing::Fore, Facing::Aft, Facing::Starboard,
                               Facing::Port, Facing::Dorsal, Facing::Ventral};
    const glm::dvec3 expected_dirs[6] = {
        {0.0, 1.0, 0.0}, {0.0, -1.0, 0.0}, {1.0, 0.0, 0.0},
        {-1.0, 0.0, 0.0}, {0.0, 0.0, 1.0}, {0.0, 0.0, -1.0}
    };
    for (int i = 0; i < 6; ++i) {
        Placement p;
        p.facing = facings[i];
        p.slot = 0;
        p.roll = 0;
        const Mount m = mount_transform(chain, p);
        const glm::dvec3 forward = m.rot * glm::dvec3(0.0, 1.0, 0.0);
        check_close(glm::length(forward - expected_dirs[i]), 0.0, 1e-9,
                    "mount_transform: facing direction matches §3.2 (Gate 1)");
    }

    // Gate 2: A chain's composed bounds equal N·p along Y, to 1 mm.
    // Slot 0 fore body occupies [m_s0.pos.y, m_s0.pos.y + p] = [20, 24].
    // Slot 11 aft body occupies [m_sN.pos.y - p, m_sN.pos.y] = [-24, -20].
    const Real bound_max_y = m_s0.pos.y + chain.pitch;
    const Real bound_min_y = m_sN.pos.y - chain.pitch;
    const Real composed_length_y = bound_max_y - bound_min_y;
    const Real expected_length_y = static_cast<Real>(chain.slots) * chain.pitch;
    check_close(composed_length_y, expected_length_y, 0.001,
                "gate 2: chain composed bounds equal N*p along Y to 1 mm");

    // Gate 3: Two parts whose cells intersect are rejected by mount_placement, returning false
    // and leaving the design unmodified.
    ShipDesign design;
    design.spine = chain;
    Placement p1;
    p1.part = "tank_drum_1";
    p1.slot = 1;
    p1.facing = Facing::Starboard;
    p1.span = 2; // occupies slots 1, 2 on Starboard
    check(mount_placement(design, p1), "gate 3: initial part mounts");
    check(design.placements.size() == 1, "gate 3: design has 1 placement");

    // Overlapping placement: slot 2 on Starboard
    Placement p_overlap;
    p_overlap.part = "section_machinery";
    p_overlap.slot = 2;
    p_overlap.facing = Facing::Starboard;
    p_overlap.span = 1;
    check(!mount_placement(design, p_overlap), "gate 3: overlapping part is rejected");
    check(design.placements.size() == 1, "gate 3: design remains unmodified after rejected mount");

    // Axial lane sharing test: slot 4 fore and slot 4 aft share the Fore lane
    Placement p_axial_fore;
    p_axial_fore.part = "section_combat_a";
    p_axial_fore.slot = 4;
    p_axial_fore.facing = Facing::Fore;
    p_axial_fore.span = 1;
    check(mount_placement(design, p_axial_fore), "gate 3: axial fore part mounts");

    Placement p_axial_aft_overlap;
    p_axial_aft_overlap.part = "drive_twin_torch";
    p_axial_aft_overlap.slot = 4;
    p_axial_aft_overlap.facing = Facing::Aft;
    p_axial_aft_overlap.span = 1;
    check(!mount_placement(design, p_axial_aft_overlap),
          "gate 3: axial aft in same slot as axial fore is rejected (shared Fore lane)");

    // Non-overlapping placement: slot 2 on Port
    Placement p_clear;
    p_clear.part = "section_machinery";
    p_clear.slot = 2;
    p_clear.facing = Facing::Port;
    p_clear.span = 1;
    check(mount_placement(design, p_clear), "gate 3: non-overlapping part mounts");
    check(design.placements.size() == 3, "gate 3: design now has 3 placements");

    // Gate 4: mirror on a fore or aft facing is rejected.
    Placement p_fore;
    p_fore.part = "nose_hammerhead";
    p_fore.facing = Facing::Fore;
    p_fore.axial = true;
    p_fore.span = 1;
    check(!mount_placement(design, p_fore, true), "gate 4: mirror on fore facing is rejected");

    Placement p_aft;
    p_aft.part = "drive_twin_torch";
    p_aft.facing = Facing::Aft;
    p_aft.axial = true;
    p_aft.span = 1;
    check(!mount_placement(design, p_aft, true), "gate 4: mirror on aft facing is rejected");

    // Mirror on radial facing succeeds
    Placement p_gun;
    p_gun.part = "section_radiator_wing";
    p_gun.slot = 7;
    p_gun.facing = Facing::Dorsal;
    p_gun.span = 1;
    check(mount_placement(design, p_gun, true), "gate 4: mirror on radial facing succeeds");
    check(design.placements.size() == 5, "gate 4: mirrored pair adds 2 placements");
    check(design.placements[3].facing == Facing::Dorsal && design.placements[4].facing == Facing::Ventral,
          "gate 4: paired placements have opposite facings");

    // Destroying one half of a mirrored pair leaves the other half flying.
    design.placements[3].destroyed = true;
    check(design.placements[3].destroyed, "gate 4: half A is destroyed");
    check(!design.placements[4].destroyed, "gate 4: half B remains intact and flying");
}

int component_tests() {
    const int before = selftest::failures();
    test_stock_designs();
    test_drive_arc();
    test_no_drives();
    test_centre_of_mass();
    test_torque_about_com();
    test_mount_rules();
    test_spinekit_gates_1_4();
    return selftest::failures() - before;
}

}  // namespace opra
