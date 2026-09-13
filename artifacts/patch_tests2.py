p = 'src/sim/component_tests.cpp'
s = open(p, encoding='utf-8').read()
old = """int component_tests() {
    test_stock_designs();"""
new = """void test_mount_rules() {
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
    test_stock_designs();
    test_mount_rules();"""
assert s.count(old) == 1, 'component_tests anchor'
s = s.replace(old, new)

# ensure the component_tests file has what it needs
if '#include "sim/system.h"' not in s:
    old2 = '#include "sim/component_tests.h"'
    assert s.count(old2) == 1
    s = s.replace(old2, old2 + '\n\n#include "sim/system.h"')
open(p, 'w', encoding='utf-8', newline='\n').write(s)
print('tests added')
