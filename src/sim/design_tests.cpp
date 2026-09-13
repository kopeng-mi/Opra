#include "sim/design_tests.h"

#include <cmath>
#include <cstdio>
#include <fstream>
#include <nlohmann/json.hpp>

#include "core/file.h"
#include "selftest.h"
#include "sim/component.h"
#include "sim/designs.h"
#include "sim/physics.h"

namespace opra {

int design_tests() {
    using selftest::check;
    using selftest::check_close;
    const int before = selftest::failures();

    std::ifstream manifest_file(asset_path("assets/models.json"));
    if (!manifest_file.is_open()) {
        check(false, "design_tests: failed to open assets/models.json");
        return selftest::failures() - before;
    }
    nlohmann::json manifest;
    manifest_file >> manifest;

    PartTable parts;
    for (const auto &entry : manifest["models"]) {
        const std::string name = entry.value("name", "");
        const std::string sidecar_rel = entry.value("sidecar", "");
        std::ifstream sc_file(asset_path(sidecar_rel));
        if (!sc_file.is_open()) continue;
        nlohmann::json sc;
        sc_file >> sc;
        if (sc.contains("part")) {
            const auto &p = sc["part"];
            PartSpec ps;
            ps.kind = p.value("kind", "");
            ps.span = p.value("span", 1);
            ps.axial = p.value("axial", true);
            ps.dry_mass = p.value("mass", 0.0);
            ps.propellant = p.value("propellant", 0.0);
            ps.thrust = p.value("thrust", 0.0);
            ps.cooling = p.value("cooling", 0.0);
            ps.heat_capacity = p.value("heat_capacity", 0.0);
            ps.rcs_jets = p.value("rcs_jets", 0);
            ps.rcs_authority = p.value("rcs_authority", 0.0);
            parts[name] = ps;
        }
    }

    DesignStore store;
    store.load(asset_path("assets/designs.json"));

    const std::string stock_names[3] = {"Kestrel", "Mule", "Needle"};

    auto pct = [](Real actual, Real expected) -> double {
        return expected != 0 ? ((actual - expected) / expected) * 100.0 : 0.0;
    };

    for (int i = 0; i < 3; ++i) {
        const ShipSpec &table = SHIPS[i];
        check(store.has(stock_names[i]), ("design_tests: store has " + stock_names[i]).c_str());
        const ShipDesign &design = store.design(stock_names[i]);
        const ShipSpec derived = derive_spec(design, parts);

        const double tol = 0.02; // 2% reproduction gate
        check_close(derived.mass, table.mass, table.mass * tol,
                    ("gate 5: " + design.name + " mass reproduces within 2%").c_str());
        check_close(derived.thrust, table.thrust, table.thrust * tol,
                    ("gate 5: " + design.name + " thrust reproduces within 2%").c_str());
        check_close(derived.fuel, table.fuel, table.fuel * tol,
                    ("gate 5: " + design.name + " fuel reproduces within 2%").c_str());
        check_close(derived.torque, table.torque, table.torque * tol,
                    ("gate 5: " + design.name + " torque reproduces within 2%").c_str());
        check_close(derived.hull, table.hull, table.hull * tol,
                    ("gate 5: " + design.name + " hull reproduces within 2%").c_str());
        check_close(derived.cooling, table.cooling, table.cooling * tol,
                    ("gate 5: " + design.name + " cooling reproduces within 2%").c_str());

        // §5.3: print reproduction table even on success
        std::printf("%-9s mass %.0f/%.0f %+0.2f%%   thrust %.3f/%.3f %+0.2f%%   fuel %.0f/%.0f %+0.2f%%\n",
                    design.name.c_str(),
                    derived.mass, table.mass, pct(derived.mass, table.mass),
                    derived.thrust / 1e6, table.thrust / 1e6, pct(derived.thrust, table.thrust),
                    derived.fuel, table.fuel, pct(derived.fuel, table.fuel));
        std::printf("          torque %.3f/%.3f %+0.2f%%   hull %.0f/%.0f %+0.2f%%   cooling %.4f/%.4f %+0.2f%%\n",
                    derived.torque, table.torque, pct(derived.torque, table.torque),
                    derived.hull, table.hull, pct(derived.hull, table.hull),
                    derived.cooling, table.cooling, pct(derived.cooling, table.cooling));
    }

    return selftest::failures() - before;
}

}  // namespace opra
