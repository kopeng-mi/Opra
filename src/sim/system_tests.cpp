// Assert suite for the system layer: the file loads and validates, the bodies propagate on their
// real conics, the sphere-of-influence walk picks the right primary, and the belt is deterministic.
#include "sim/system_tests.h"

#include <algorithm>
#include <cmath>
#include <string>

#include "core/file.h"
#include "orbit/kepler.h"
#include "selftest.h"
#include "sim/belt.h"
#include "sim/system.h"
#include "sim/world.h"

namespace opra {
namespace {

// The shared vocabulary, unqualified: these cases read as assertions, not as calls.
using selftest::check;
using selftest::check_close;

SystemDef nereid(std::string &path_out) {
    path_out = asset_path("assets/systems/nereid.json");
    return load_system(path_out);
}

orbit::Elements vesk_elements(const SystemDef &system) { return system.bodies[3].elements; }

/** The deepest body holding `position`, starting from the star: -1 means the star itself. */
int primary_at(const SystemDef &system, const std::vector<BodyState> &states,
               const glm::dvec2 &position, int current = -1) {
    return primary_of(system, states, current, position);
}

void test_system_load() {
    std::string path;
    const SystemDef system = nereid(path);
    check(system.bodies.size() == 8, "system: the star, four bodies and three stations load");
    check(system.name == "Nereid", "system: the name comes from the file");
    check(system.jump_links.empty(), "system: jump_links is present and empty (E4)");
    check(system.index_of("tessera") == 2, "system: bodies keep file order");
    check(system.index_of("nope") == -1, "system: an unknown id is not a body");

    const Body &star = system.bodies[0];
    check(star.mu == 2.07e19 && star.parent == -1, "system: the star anchors the frame");

    const Body &vesk = system.bodies[3];
    check(system.bodies[static_cast<size_t>(vesk.parent)].id == "tessera",
          "system: a moon's parent is resolved by id, not by position");
    check(vesk.elements.mu == 1.9e13, "system: a moon's elements carry its parent's mu");
    check(std::fabs(vesk.soi - 5.8619e6) / 5.8619e6 < 0.01,
          "system: Vesk's sphere of influence is ~5.9e6 m");

    const Body &wayfarer = system.bodies[5];
    check(wayfarer.kind == BodyClass::Station && wayfarer.mu == 0.0,
          "system: a station holds nothing in orbit");
    check(wayfarer.elements.a == 2.61e10, "system: Wayfarer's orbit comes from the file");
    check(system.belt.count == 4200 && system.belt.seed == 4712u,
          "system: the belt's count and seed come from the file");
    check(system.belt.parent == 0, "system: the belt orbits the star");

    // The world fields (plan 03 G11-G16). These are the contract the drag, terrain, Lagrange and
    // base code reads; a file that names a pad index that does not exist is fatal at load, so what
    // is checked here is that the values arrive where the callers expect them.
    const Body &tessera = system.bodies[2];
    check(tessera.atmosphere.present && std::fabs(tessera.atmosphere.rho0 - 1.10) < 1e-9 &&
              std::fabs(tessera.atmosphere.scale_height - 7800.0) < 1e-9,
          "system: Tessera carries an atmosphere with its rho0 and scale height");
    check(tessera.terrain.present && tessera.terrain.pads.size() == 2,
          "system: Tessera's terrain carries both pads");
    check(tessera.terrain.pads[0].name == "Tessera Downport" &&
              std::fabs(tessera.terrain.pads[0].width - 2600.0) < 1e-9,
          "system: a pad keeps its name and width");
    check(tessera.surface.present && tessera.surface.pad == 0 &&
              tessera.surface.model == "surface_base",
          "system: Tessera's base sits on pad 0 and names its model");
    check(!system.bodies[1].atmosphere.present && system.bodies[1].terrain.present,
          "system: Cinder has ground and no air");

    const Body &l4 = system.bodies[7];
    check(l4.lagrange.present && l4.lagrange.point == 4 && l4.lagrange.primary == 2,
          "system: the L4 base names Tessera as its primary");
    check(!system.bodies[5].lagrange.present, "system: a plain station has no Lagrange seat");
}

void test_system_propagation() {
    std::string path;
    const SystemDef system = nereid(path);
    std::vector<BodyState> states;
    propagate(system, 0.0, states);
    check(states.size() == system.bodies.size(), "system: propagate fills every body");

    // The star anchors the frame, so its state is the origin and the planets are measured from it.
    check(glm::length(states[0].position) == 0.0, "system: the star sits at the barycentre");
    const Body &tessera = system.bodies[2];
    const double radius = glm::length(states[2].position);
    check(radius > tessera.elements.a * (1.0 - tessera.elements.e) * 0.999 &&
              radius < tessera.elements.a * (1.0 + tessera.elements.e) * 1.001,
          "system: Tessera's radius is inside its own conic");
    const double speed = glm::length(states[2].velocity);
    const double vis_viva = std::sqrt(2.07e19 * (2.0 / radius - 1.0 / tessera.elements.a));
    check_close(speed, vis_viva, 1e-6, "system: Tessera's speed obeys vis-viva at t = 0");

    // A station has no children to inherit: its state is its parent's plus its own relative conic.
    const BodyState &wayfarer = states[5];
    check(glm::length(wayfarer.position) > 2.5e10 && glm::length(wayfarer.position) < 2.7e10,
          "system: Wayfarer orbits inside the belt");

    // Half a period later the moon is on the far side of its planet, and the frame has moved with
    // its planet: the moon's barycentric position is parent + relative, never absolute.
    const double period = orbit::period(vesk_elements(system));
    propagate(system, period * 0.5, states);
    const glm::dvec2 moon_offset = states[3].position - states[2].position;
    check(glm::length(moon_offset) > 4.0e7, "system: a moon stays with its planet after propagation");
}

void test_soi_selection() {
    std::string path;
    const SystemDef system = nereid(path);
    std::vector<BodyState> states;
    propagate(system, 0.0, states);

    check(primary_at(system, states, {0.0, 0.0}) == -1,
          "system: the barycentre belongs to the star");
    check(primary_at(system, states, states[2].position + glm::dvec2(1.0e7, 0.0)) == 2,
          "system: 10 000 km from Tessera is inside Tessera's sphere");
    check(primary_at(system, states, states[3].position + glm::dvec2(3.0e6, 0.0)) == 3,
          "system: 3 000 km from Vesk is inside Vesk's sphere, inside Tessera's");
    check(primary_at(system, states, states[3].position + glm::dvec2(9.0e6, 0.0)) == 2,
          "system: outside Vesk's sphere the primary falls back to Tessera");

    // Hysteresis: the body we are already in keeps the ship out to 1.02x its radius, and no
    // further: that is what stops a boundary skim switching frames every frame.
    const double soi = system.bodies[3].soi;
    const glm::dvec2 just_outside = states[3].position + glm::dvec2(soi * 1.01, 0.0);
    check(primary_at(system, states, just_outside) == 2,
          "system: entering needs to be inside the sphere");
    check(primary_at(system, states, just_outside, 3) == 3,
          "system: a body already held keeps the ship out to 1.02x its sphere");
    check(primary_at(system, states, states[3].position + glm::dvec2(soi * 1.05, 0.0), 3) == 2,
          "system: past the hysteresis band the ship leaves");
}

void test_belt() {
    std::string path;
    const SystemDef system = nereid(path);
    const std::vector<BeltBody> ring = generate_belt(system);
    check(ring.size() == 4200, "belt: the file's count is what is generated");
    check(ring[0].elements.a >= system.belt.inner && ring[0].elements.a <= system.belt.outer,
          "belt: the first rock starts inside the ring");
    double min_a = 1.0e30, max_a = 0.0, max_e = 0.0;
    for (const BeltBody &rock : ring) {
        min_a = std::min(min_a, rock.elements.a);
        max_a = std::max(max_a, rock.elements.a);
        max_e = std::max(max_e, rock.elements.e);
        if (rock.elements.a < system.belt.inner || rock.elements.a > system.belt.outer) {
            check(false, "belt: every rock stays inside the ring");
            return;
        }
    }
    check(min_a >= system.belt.inner && max_a <= system.belt.outer,
          "belt: every rock stays inside the ring");
    check(max_e < 0.01, "belt: the ring is rubble, not planets");
    check(ring[0].elements.mu == system.bodies[static_cast<size_t>(system.belt.parent)].mu,
          "belt: rocks orbit the belt's parent");

    // Deterministic: the same seed must give the same ring, byte for byte, or a screenshot of the
    // belt cannot be reproduced.
    const std::vector<BeltBody> again = generate_belt(system);
    check(again[0].elements.a == ring[0].elements.a && again[4199].elements.M0 == ring[4199].elements.M0,
          "belt: generation is deterministic");

    const std::vector<ClusterRock> cluster = generate_cluster(system.belt, 445, 5200.0);
    check(cluster.size() == 445, "belt: the cluster has the requested size");
    double max_offset = 0.0;
    for (const ClusterRock &rock : cluster) {
        max_offset = std::max(max_offset, std::max(std::fabs(rock.offset.x), std::fabs(rock.offset.y)));
    }
    check(max_offset <= 2600.0, "belt: cluster rocks stay inside the span");
    const std::vector<ClusterRock> cluster_again = generate_cluster(system.belt, 445, 5200.0);
    check(cluster_again[100].offset.x == cluster[100].offset.x &&
              cluster_again[100].radius == cluster[100].radius,
          "belt: cluster generation is deterministic");
}

/**
 * The system's gravity reaches the ship through one term and nothing else, and the sector rides a
 * co-orbiting frame: μ = 0 has to be the port's own numbers, and the zone has to feel the tidal
 * difference rather than the star's full pull.
 */
void test_gravity() {
    // A bare world has no system, so its gravity is exactly zero and its steps are the port's.
    World bare;
    const FlightInput input{1.0, 0.2, 0.0, false, false};
    for (int i = 0; i < 600; ++i) bare.step(input, 1.0 / 120.0);
    check_close(bare.ship.position.x, 23.630688409859413, 1e-12,
                "gravity: a world with no system still steps the golden x");
    check_close(bare.ship.position.y, 164.88502883314655, 1e-12,
                "gravity: a world with no system still steps the golden y");
    check_close(bare.gravity().x, 0.0, 0.0, "gravity: no system means no gravity");
    check_close(bare.gravity().y, 0.0, 0.0, "gravity: no system means no gravity");

    // With Nereid attached the anchor is Wayfarer, and the zone rides its orbit: the star's own
    // 0.03 m/s^2 pull cancels against the anchor's, leaving the tidal term.
    std::string path;
    const SystemDef system = nereid(path);
    World world;
    world.attach_system(system, system.index_of("wayfarer"));
    check(world.anchor_body == 5, "gravity: Wayfarer is the zone's anchor");
    const glm::dvec2 anchor_position = world.anchor.position;
    const double anchor_radius = glm::length(anchor_position);
    check(anchor_radius > 2.5e10 && anchor_radius < 2.7e10,
          "gravity: the anchor sits where the file puts it");

    // Two kilometres out the frame is no longer exactly the anchor's, and the difference is the
    // tidal term: small, real, and strictly non-zero.
    const double span = 2.0e3;
    world.ship.position = {span, 0.0};
    const Vec2 tidal = world.gravity();
    const double star_pull = system.bodies[0].mu / (anchor_radius * anchor_radius);
    check(std::hypot(tidal.x, tidal.y) < star_pull * 1e-4,
          "gravity: the co-orbiting frame cancels the star's own pull");
    check(std::hypot(tidal.x, tidal.y) > 0.0, "gravity: the tidal term is present, not switched off");

    // Ten kilometres out along the anchor's own radius, the tidal term is the field's gradient: the
    // analytic value is exact to second order, and span/R here is 4e-7.
    const glm::dvec2 outward = anchor_position / anchor_radius;
    const double reach = anchor_radius - 1.0e4;
    world.ship.position = {outward.x * 1.0e4, outward.y * 1.0e4};
    const Vec2 radial = world.gravity();
    const double expected = system.bodies[0].mu * (1.0 / (reach * reach) - 1.0 / (anchor_radius * anchor_radius));
    const double felt = std::hypot(radial.x, radial.y);
    check_close(felt, expected, expected * 1e-3,
                "gravity: the tidal term is the field's gradient over the zone");
}

/**
 * The deep end: with the ship placed inside Tessera's sphere, the primary walk has to find it and
 * the pull has to be Tessera's, not the star's.
 */
void test_primary_switching() {
    std::string path;
    const SystemDef system = nereid(path);
    World world;
    world.attach_system(system, system.index_of("wayfarer"));
    std::vector<BodyState> states;
    propagate(system, 0.0, states);

    // Put the ship 3 000 km from Tessera, well inside its 6.6e7 m sphere: the zone offset is what
    // it would have to fly, and the walk runs on the barycentric position that offset produces.
    const glm::dvec2 tessera = states[2].position;
    const glm::dvec2 offset = tessera + glm::dvec2(3.0e6, 0.0) - world.anchor.position;
    world.ship.position = {offset.x, offset.y};
    const Vec2 pull = world.gravity();
    check(world.primary == 2, "gravity: three thousand kilometres from Tessera is inside its sphere");
    const glm::dvec2 here = world.system_position();
    const double distance_to_tessera = glm::length(here - tessera);
    const double expected = system.bodies[2].mu / (distance_to_tessera * distance_to_tessera);
    // The anchor's own pull is subtracted, so the felt value is close to Tessera's, not equal.
    check_close(std::hypot(pull.x, pull.y), expected, expected * 0.02,
                "gravity: inside Tessera's sphere the pull is Tessera's");
    const double star_pull = system.bodies[0].mu / glm::dot(here, here);
    check(expected > star_pull * 10.0, "gravity: and it dominates the star's field there");
}

/**
 * The railed path: above 10x the ship is advanced along its conic, so the advance is exact - two
 * half-steps and one whole step must agree, and a long warp must not change the orbit it is on.
 */
void test_warp_rail() {
    std::string path;
    const SystemDef system = nereid(path);
    World once;
    once.attach_system(system, system.index_of("wayfarer"));
    World twice;
    twice.attach_system(system, system.index_of("wayfarer"));

    // Start the ship somewhere in the zone so the advance is not trivially zero.
    once.ship.position = {1200.0, -800.0};
    once.ship.velocity = {40.0, 12.0};
    twice.ship = once.ship;

    const double span = 3600.0;
    once.warp_step(span);
    twice.warp_step(span * 0.5);
    twice.warp_step(span * 0.5);
    // The elements <-> state round trip is lossless to 1e-9 relative (plan §3.4), so over a
    // 145 km advance the honest bound is that, not machine epsilon.
    check_close(once.ship.position.x, twice.ship.position.x, 1e-3,
                "warp: two half-advances equal one whole advance (x)");
    check_close(once.ship.position.y, twice.ship.position.y, 1e-3,
                "warp: two half-advances equal one whole advance (y)");
    check_close(once.elapsed, span, 1e-12, "warp: the clock advances by the same span");

    // The orbit's shape is a property of the conic, not of how far along it the ship has run.
    const size_t primary = static_cast<size_t>(once.primary < 0 ? 0 : once.primary);
    const orbit::Elements before = orbit::from_state(once.system_position(), once.system_velocity(),
                                                     system.bodies[primary].mu, 0.0);
    once.warp_step(86400.0);
    const orbit::Elements after = orbit::from_state(once.system_position(), once.system_velocity(),
                                                    system.bodies[primary].mu, 0.0);
    check_close(after.a, before.a, std::fabs(before.a) * 1e-9, "warp: the semi-major axis holds");
    check_close(after.e, before.e, 1e-9, "warp: the eccentricity holds");
    check(once.system_position() != twice.system_position(),
          "warp: a day of warp actually moves the ship");

    // And the advance is along the conic: in the system frame the ship is back where it started
    // after one full period. The frame itself has moved by then, so this is the inertial test.
    const double period = orbit::period(before);
    if (period > 0.0 && period < 1.0e9) {
        World full;
        full.attach_system(system, system.index_of("wayfarer"));
        full.ship.position = {1200.0, -800.0};
        full.ship.velocity = {40.0, 12.0};
        const glm::dvec2 start = full.system_position();
        full.warp_step(period);
        check_close(glm::length(full.system_position() - start), 0.0, 100.0,
                    "warp: one period of warp returns the ship to where it started");
    }
}

}  // namespace

int system_tests() {
    const int before = selftest::failures();
    test_system_load();
    test_system_propagation();
    test_soi_selection();
    test_belt();
    test_gravity();
    test_primary_switching();
    test_warp_rail();
    return selftest::failures() - before;
}

}  // namespace opra
