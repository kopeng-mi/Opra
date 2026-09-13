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

int component_tests() {
    const int before = selftest::failures();
    test_stock_designs();
    test_drive_arc();
    test_no_drives();
    test_centre_of_mass();
    test_torque_about_com();
    test_mount_rules();
    return selftest::failures() - before;
}

}  // namespace opra
