
# ---------------- K11: scan-found discoveries (plan 03 s3.7's missing third leg)
p = 'src/sim/survey.h'
s = open(p, encoding='utf-8').read()
old = """/** True when the elements are inside the box the contract names, within tolerance. */
bool inside_box(const orbit::Elements &elements, double box_a, double box_e);"""
new = """/** True when the elements are inside the box the contract names, within tolerance. */
bool inside_box(const orbit::Elements &elements, double box_a, double box_e);

/**
 * A discovery (plan 05 s6.1): a derelict, an anomalous mass or a cached depot that the survey
 * finds when a scan pass covers its longitude. Positions are deterministic in the body's own
 * seed, so a system file describes them and nothing is hardcoded; `found` is permanent state -
 * once the sweep has crossed it, it stays on the chart for the rest of the run.
 */
struct Discovery {
    std::string name;
    std::string kind;  // "derelict", "anomaly", "depot"
    int body = -1;
    double theta = 0.0;
    bool found = false;
};

/** The system's discoveries, deterministic in the bodies' own seeds. */
std::vector<Discovery> generate_discoveries(const SystemDef &system);

/** A longitude the survey just covered: any unfound discovery under the arc is found for good. */
void note_scan_theta(World &world, int body, double theta);"""
assert s.count(old) == 1, 'survey.h'
s = s.replace(old, new)
open(p, 'w', encoding='utf-8', newline='\n').write(s)
print('survey.h ok')

p = 'src/sim/survey.cpp'
s = open(p, encoding='utf-8').read()
old = """void record_scan(World &world, int body, double altitude, double ground_speed, double theta) {
    if (body < 0 || static_cast<size_t>(body) >= world.scanned.size()) return;
    if (altitude < SCAN_MIN_ALTITUDE || altitude > SCAN_MAX_ALTITUDE) return;
    if (ground_speed > SCAN_MAX_GROUND_SPEED) return;
    merge_arc(world.scanned[static_cast<size_t>(body)], theta - SCAN_ARC, theta + SCAN_ARC);
}"""
new = """void record_scan(World &world, int body, double altitude, double ground_speed, double theta) {
    if (body < 0 || static_cast<size_t>(body) >= world.scanned.size()) return;
    if (altitude < SCAN_MIN_ALTITUDE || altitude > SCAN_MAX_ALTITUDE) return;
    if (ground_speed > SCAN_MAX_GROUND_SPEED) return;
    merge_arc(world.scanned[static_cast<size_t>(body)], theta - SCAN_ARC, theta + SCAN_ARC);
    note_scan_theta(world, body, theta);
}"""
assert s.count(old) == 1, 'record_scan'
s = s.replace(old, new)

old = """std::vector<std::string> update_satellites(World &world) {"""
new = """std::vector<Discovery> generate_discoveries(const SystemDef &system) {
    std::vector<Discovery> out;
    static const char *kinds[] = {"derelict", "anomaly", "depot"};
    static const char *names[] = {"cold hulk", "mass anomaly", "cached depot"};
    // One candidate for every body with ground, at a longitude hashed from the body's own seed:
    // the same file always draws the same thing to find, and a second system is a second set.
    for (size_t i = 1; i < system.bodies.size(); ++i) {
        const Body &body = system.bodies[i];
        if (!body.terrain.present) continue;
        const unsigned int hash = body.terrain.seed * 2654435761u;
        Discovery discovery;
        discovery.kind = kinds[hash % 3];
        discovery.name = std::string(names[hash % 3]) + " - " + body.name;
        discovery.body = static_cast<int>(i);
        discovery.theta = (static_cast<double>(hash >> 8) / 16777216.0) * TWO_PI;
        out.push_back(std::move(discovery));
    }
    return out;
}

void note_scan_theta(World &world, int body, double theta) {
    for (Discovery &discovery : world.discoveries) {
        if (discovery.found || discovery.body != body) continue;
        double delta = std::fabs(theta - discovery.theta);
        if (delta > PI) delta = TWO_PI - delta;
        if (delta <= SCAN_ARC * 2.0) discovery.found = true;
    }
}

std::vector<std::string> update_satellites(World &world) {"""
assert s.count(old) == 1, 'discoveries impl'
s = s.replace(old, new)

old = """constexpr double TWO_PI = 6.283185307179586;"""
new = """constexpr double TWO_PI = 6.283185307179586;
constexpr double PI = 3.14159265358979323846;"""
assert s.count(old) == 1, 'PI'
s = s.replace(old, new)
open(p, 'w', encoding='utf-8', newline='\n').write(s)
print('survey.cpp ok')

# World carries the discoveries
p = 'src/sim/world.h'
s = open(p, encoding='utf-8').read()
old = """    /** Deployed satellites and their orbit checks (plan 3.7). */
    std::vector<Satellite> satellites;"""
new = """    /** Deployed satellites and their orbit checks (plan 3.7). */
    std::vector<Satellite> satellites;
    /** The survey's discoveries (plan 05 s6.1); `found` is permanent for the run. */
    std::vector<Discovery> discoveries;"""
assert s.count(old) == 1, 'world discoveries'
s = s.replace(old, new)
open(p, 'w', encoding='utf-8', newline='\n').write(s)
print('world.h ok')

p = 'src/sim/world.cpp'
s = open(p, encoding='utf-8').read()
old = """    scanned.assign(system.bodies.size(), {});"""
new = """    scanned.assign(system.bodies.size(), {});
    // The survey's finds (plan 05 s6.1), described by the system's own seeds.
    discoveries = generate_discoveries(system);"""
assert s.count(old) == 1, 'attach'
s = s.replace(old, new)
open(p, 'w', encoding='utf-8', newline='\n').write(s)
print('world.cpp ok')

# overlay: drawn discoveries as crossed circles with labels (s2.6's node mark)
p = 'src/game/scene.cpp'
s = open(p, encoding='utf-8').read()
old = """    // ---- icons (s2.6): the honest mark for anything under eight pixels across"""
new = """    // Discoveries (s6.1): the survey's permanent finds, as crossed circles with labels - the
    // chart's own mark for "something is here that was not here when you arrived".
    for (const Discovery &discovery : world.discoveries) {
        if (!discovery.found) continue;
        const glm::dvec2 at_zone =
            world.body_zone_position(discovery.body) +
            glm::dvec2(std::cos(discovery.theta), std::sin(discovery.theta)) *
                world.system.bodies[static_cast<size_t>(discovery.body)].radius;
        glm::dvec2 px;
        if (!projector.to_screen(glm::dvec3(at_zone, 0.0), px)) continue;
        const glm::vec2 mark(px);
        ui::push_arc(ui, mark, 5.0f, 0.0f, 6.2831853f, 1.2f, ui::tokens::NAV);
        ui::push_line(ui, mark - glm::vec2(3.5f, 3.5f), mark + glm::vec2(3.5f, 3.5f), 1.2f,
                      ui::tokens::NAV);
        ui::push_line(ui, mark + glm::vec2(-3.5f, 3.5f), mark + glm::vec2(3.5f, -3.5f), 1.2f,
                      ui::tokens::NAV);
        ui::push_text(ui, discovery.name.c_str(), 12.0f, {mark.x + 8.0f, mark.y - 6.0f},
                      TextAlign::Left, with_alpha(ui::tokens::NAV, 0.9f), TextFace::Label);
    }

    // ---- icons (s2.6): the honest mark for anything under eight pixels across"""
assert s.count(old) == 1, 'overlay discoveries'
s = s.replace(old, new)
open(p, 'w', encoding='utf-8', newline='\n').write(s)
print('scene.cpp ok')

# ---------------- K12: derive strafe, flag driveless designs, reject overlapping mounts
p = 'src/sim/component.h'
s = open(p, encoding='utf-8').read()
old = """/** A stock hull as a component set. `derived_*` over this must reproduce the SHIPS row. */
const ShipDesign &stock_design(ShipClass shipClass);"""
new = """/** A stock hull as a component set. `derived_*` over this must reproduce the SHIPS row. */
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
Real design_twr(const ShipDesign &design, const struct Body &body);"""
assert s.count(old) == 1, 'component.h'
s = s.replace(old, new)
open(p, 'w', encoding='utf-8', newline='\n').write(s)
print('component.h ok')

p = 'src/sim/component.cpp'
s = open(p, encoding='utf-8').read()
old = """ShipSpec derive_spec(const ShipDesign &design) {
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
}"""
new = """ShipSpec derive_spec(const ShipDesign &design) {
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
    // s5.1 via s6.2: the lateral authority is derived from the RCS count, never authored. Four
    // corner blocks - every stock hull's fit - is 22% of main thrust, the figure the flight model
    // already tuned to.
    int rcs = 0;
    for (const Component &component : design.components) rcs += is_rcs(component) ? 1 : 0;
    spec.strafeFraction = std::clamp(0.055 * rcs, 0.10, 0.45);
    return spec;
}

bool design_has_drive(const ShipDesign &design) {
    for (const Component &component : design.components) {
        if (is_drive(component) && component.thrust > 0) return true;
    }
    return false;
}

bool mount_component(ShipDesign &design, const Component &component) {
    // s6.2: overlapping components are rejected at mount time. The flange is a 4 m pitch; two
    // mounts closer than that cannot both be bolted to.
    constexpr Real MIN_PITCH = 4.0;
    for (const Component &placed : design.components) {
        const Real dx = placed.mount.x - component.mount.x;
        const Real dy = placed.mount.y - component.mount.y;
        if (std::hypot(dx, dy) < MIN_PITCH) return false;
    }
    design.components.push_back(component);
    return true;
}

Real design_twr(const ShipDesign &design, const struct Body &body) {
    if (body.mu <= 0.0 || body.radius <= 0.0) return 0;
    const Real weight = static_cast<Real>(derived_mass(design)) *
                        static_cast<Real>(body.mu / (body.radius * body.radius));
    return weight > 0.0 ? static_cast<Real>(derived_thrust(design)) / weight : 0;
}"""
assert s.count(old) == 1, 'component.cpp'
s = s.replace(old, new)

# <algorithm> for clamp
old = """#include <cmath>
#include <string_view>"""
new = """#include <algorithm>
#include <cmath>
#include <string_view>"""
assert s.count(old) == 1, 'include'
s = s.replace(old, new)
open(p, 'w', encoding='utf-8', newline='\n').write(s)
print('component.cpp ok')

# stock table consistency: the derivation gives every four-block hull 0.22
p = 'src/sim/physics.cpp'
s = open(p, encoding='utf-8').read()
old = """    {"Mule", "Heavy salvage tug", 142000, 1950000, 30000, 0.82, 150, 58, 320, 0.075, 1, 1, 0.19},
    {"Needle", "Fast reconnaissance cutter", 43000, 1200000, 10000, 2.05, 75, 31, 40, 0.05, 1, 1,
     0.28},"""
new = """    {"Mule", "Heavy salvage tug", 142000, 1950000, 30000, 0.82, 150, 58, 320, 0.075, 1, 1, 0.22},
    {"Needle", "Fast reconnaissance cutter", 43000, 1200000, 10000, 2.05, 75, 31, 40, 0.05, 1, 1,
     0.22},"""
assert s.count(old) == 1, 'stock strafe'
s = s.replace(old, new)
open(p, 'w', encoding='utf-8', newline='\n').write(s)
print('physics.cpp ok')
