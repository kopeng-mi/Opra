
# K4: pixel-size LOD selection with hysteresis (plan 05 s2.5)
p = 'src/game/scene.h'
s = open(p, encoding='utf-8').read()
old = """/** Places every instance for one frame: backdrop, rock field, structures, cargo, ore, and the ship. */
void build_scene(SceneBuilder &scene, const ModelSet &models, const World &world,
                 const Backdrop &backdrop, const Camera &camera, float screen_width,
                 float screen_height);"""
new = """/**
 * Places every instance for one frame: backdrop, rock field, structures, cargo, ore, and the ship.
 * `lod_memory` is the hysteresis state (plan 05 s2.5): keyed per object, it survives frames so an
 * object sitting on a threshold does not pop-flicker between representations.
 */
void build_scene(SceneBuilder &scene, const ModelSet &models, const World &world,
                 const Backdrop &backdrop, const Camera &camera, float screen_width,
                 float screen_height, std::unordered_map<unsigned long long, int> &lod_memory);

/**
 * s2.5's selection: the level for an object `px` pixels across, given the level it already drew
 * at. Thresholds 8 / 60 / 250 px with a ten percent deadband - a model at the bold LOD stays
 * there until 275 px or 54 px, which costs nothing and removes the boundary pop-flicker.
 * 0 icon, 1 blocky, 2 bold, 3 detailed.
 */
int lod_level(float px, int previous);"""
assert s.count(old) == 1, 'scene.h build_scene'
s = s.replace(old, new)
if '#include <unordered_map>' not in s:
    old2 = '#include <vector>'
    assert s.count(old2) == 1
    s = s.replace(old2, '#include <unordered_map>\n#include <vector>')
open(p, 'w', encoding='utf-8', newline='\n').write(s)
print('scene.h ok')

p = 'src/game/scene.cpp'
s = open(p, encoding='utf-8').read()

# selection function in the anonymous namespace near the top helpers
old = """/**
 * A hex colour like "#ff9c5c" as a tint, or white when it does not parse: the system file carries
 * each body's albedo tint, and a body without a map still deserves its own hue.
 */"""
new = """/**
 * s2.5's hysteresis: the deadband is the whole point. A crossfade would need a blend pipeline and
 * a second draw per object, for an effect nobody sees at a ten percent deadband.
 */
int lod_level(float px, int previous) {
    const auto stays = [](float value, float lo, float hi) { return value >= lo && value <= hi; };
    switch (previous) {
        case 3:
            if (stays(px, 250.0f, 275.0f)) return 3;  // detailed holds to +10%
            break;
        case 2:
            if (stays(px, 54.0f, 275.0f)) return 2;   // bold holds from 54 to 275
            break;
        case 1:
            if (stays(px, 7.2f, 66.0f)) return 1;     // blocky holds from 7.2 to 66
            break;
        default:
            break;
    }
    return px > 250.0f ? 3 : px >= 60.0f ? 2 : px >= 8.0f ? 1 : 0;
}

/**
 * The model a level draws with. The bold model is the authored one; the blocky LOD is its
 * silhouette-extruded sibling under `<name>_blocky` when the manifest carries one, and the
 * detailed tier falls back to the bold model until an asset names the extra export.
 */
const Model &lod_model(const ModelSet &models, const std::string &name, int level) {
    if (level <= 1) {
        const std::string blocky = name + "_blocky";
        for (const std::string &candidate : models.store.names()) {
            if (candidate == blocky) return models.store.model(blocky);
        }
    }
    return models.store.model(name);
}

/**
 * A hex colour like "#ff9c5c" as a tint, or white when it does not parse: the system file carries
 * each body's albedo tint, and a body without a map still deserves its own hue.
 */"""
assert s.count(old) == 1, 'lod helpers anchor'
s = s.replace(old, new)

# build_scene: signature + LOD gating of model draws
old = """void build_scene(SceneBuilder &scene, const ModelSet &models, const World &world,
                 const Backdrop &backdrop, const Camera &camera, float screen_width,
                 float screen_height) {"""
new = """void build_scene(SceneBuilder &scene, const ModelSet &models, const World &world,
                 const Backdrop &backdrop, const Camera &camera, float screen_width,
                 float screen_height, std::unordered_map<unsigned long long, int> &lod_memory) {"""
assert s.count(old) == 1, 'build_scene sig'
s = s.replace(old, new)

# helper lambda after the frustum setup, then gate each model draw
old = """    // The system's own bodies (plan 05 s2.3): the sky is never empty, at any zoom. Anything beyond
    // the far plane is re-projected exactly, so the flight view at hull scale still carries every
    // planet at its true bearing and angular size.
    add_system_bodies(scene, world, origin, camera.far_z(), models.planet_mesh, screen_height);"""
new = """    // The system's own bodies (plan 05 s2.3): the sky is never empty, at any zoom. Anything beyond
    // the far plane is re-projected exactly, so the flight view at hull scale still carries every
    // planet at its true bearing and angular size.
    add_system_bodies(scene, world, origin, camera.far_z(), models.planet_mesh, screen_height);

    // s2.5's selection, per drawn object: on-screen size decides, never distance, and s2.8's
    // assert falls out - inside the near plane an object is an icon, because its px is tiny.
    const auto object_px = [&](Real x, Real y, Real radius) {
        const double dx = x - origin.x, dy = y - origin.y;
        const double d = std::max(std::sqrt(dx * dx + dy * dy), 1.0);
        return 2.0 * static_cast<double>(radius) / d *
               (static_cast<double>(screen_height) * 0.5) /
               std::tan(static_cast<double>(CAMERA_FOV_Y) * 0.5);
    };
    const auto lod_key = [](unsigned long long id) { return id; };
    const auto select = [&](unsigned long long id, Real x, Real y, Real radius) {
        const float px = static_cast<float>(object_px(x, y, radius));
        const int previous = lod_memory.count(lod_key(id)) ? lod_memory[lod_key(id)] : -1;
        const int level = lod_level(px, previous);
        lod_memory[lod_key(id)] = level;
        return level;
    };"""
assert s.count(old) == 1, 'object_px'
s = s.replace(old, new)

# gate the structure draws on the LOD level (skip geometry under 8 px; the icon owns it)
old = """    // The station turns about its own origin; the ring and arms are its colliders too.
    scene.add_model(models.store.model("station"), relative(origin, STATION.x, STATION.y, 0.0),
                    spin_about_z(static_cast<float>(world.stationSpin)), 1.0f, false, 0.0f);
    scene.add_model(models.store.model("beacon"), relative(origin, RELAY.x, RELAY.y, 0.0),
                    glm::quat(1, 0, 0, 0), 1.0f, false, 0.0f);
    scene.add_model(models.store.model("derelict"), relative(origin, DERELICT.x, DERELICT.y, 0.0),
                    spin_about_z(0.35f), 1.0f, false, 0.0f);"""
new = """    // The station turns about its own origin; the ring and arms are its colliders too. Each
    // structure's LOD rides its own pixel size; under eight px the overlay's icon owns it.
    const auto draw_structure = [&](unsigned long long id, const std::string &name, Real x, Real y,
                                    Real radius, const glm::quat &spin) {
        const int level = select(id, x, y, radius);
        if (level < 1) return;  // icon: geometry stands down (s2.5)
        scene.add_model(lod_model(models, name, level), relative(origin, x, y, 0.0), spin, 1.0f,
                        false, 0.0f);
    };
    draw_structure(1, "station", STATION.x, STATION.y, 78.0,
                   spin_about_z(static_cast<float>(world.stationSpin)));
    draw_structure(2, "beacon", RELAY.x, RELAY.y, 20.0, glm::quat(1, 0, 0, 0));
    draw_structure(3, "derelict", DERELICT.x, DERELICT.y, 41.0, spin_about_z(0.35f));"""
assert s.count(old) == 1, 'structures'
s = s.replace(old, new)

# ship: pick the LOD by its own pixel size
old = """    const int ship_class = static_cast<int>(world.ship.shipClass);
    const float thrust = std::max(0.0f, static_cast<float>(world.ship.thrustLevel));
    // The sim already solved the jets; the renderer only has to fade each cone on its own share.
    const glm::vec4 jets(static_cast<float>(world.ship.rcsJet[0]), static_cast<float>(world.ship.rcsJet[1]),
                         static_cast<float>(world.ship.rcsJet[2]), static_cast<float>(world.ship.rcsJet[3]));
    scene.add_model(models.store.model(SHIP_MODEL_NAMES[ship_class]),
                    relative(origin, world.ship.position.x, world.ship.position.y, 0.0),
                    spin_about_z(static_cast<float>(world.ship.angle)), config::SHIP_SCALE,
                    world.ship.thrustLevel > 0.02f, ui::clamp01(thrust / 1.65f), glm::vec3(1.0f),
                    jets);"""
new = """    const int ship_class = static_cast<int>(world.ship.shipClass);
    const float thrust = std::max(0.0f, static_cast<float>(world.ship.thrustLevel));
    // The sim already solved the jets; the renderer only has to fade each cone on its own share.
    const glm::vec4 jets(static_cast<float>(world.ship.rcsJet[0]), static_cast<float>(world.ship.rcsJet[1]),
                         static_cast<float>(world.ship.rcsJet[2]), static_cast<float>(world.ship.rcsJet[3]));
    const std::string ship_model = SHIP_MODEL_NAMES[ship_class];
    // The own ship follows the same rule as everything else (s2.5): under eight px the chevron in
    // the overlay is the honest mark and the hull stands down.
    if (select(0, world.ship.position.x, world.ship.position.y,
               static_cast<Real>(world.ship.bounds.halfLength)) >= 1) {
        scene.add_model(lod_model(models, ship_model, 2),
                        relative(origin, world.ship.position.x, world.ship.position.y, 0.0),
                        spin_about_z(static_cast<float>(world.ship.angle)), config::SHIP_SCALE,
                        world.ship.thrustLevel > 0.02f, ui::clamp01(thrust / 1.65f), glm::vec3(1.0f),
                        jets);
    }"""
assert s.count(old) == 1, 'ship lod'
s = s.replace(old, new)

# cargo/ore: gate on px
old = """        const bool blackbox = cargo.kind == CargoKind::Blackbox;
        float cant = static_cast<float>(cargo.position.x) * 0.01f;
        if (blackbox) cant += 1.0f;
        scene.add_model(models.store.model("cargo"), at, spin_about_z(cant), 1.0f, false, 0.0f,
                        blackbox ? blackbox_tint : glm::vec3(1.0f));"""
new = """        const bool blackbox = cargo.kind == CargoKind::Blackbox;
        float cant = static_cast<float>(cargo.position.x) * 0.01f;
        if (blackbox) cant += 1.0f;
        if (select(1000 + cargo_index, cargo.position.x, cargo.position.y, 12.0) >= 1) {
            scene.add_model(lod_model(models, "cargo", 2), at, spin_about_z(cant), 1.0f, false, 0.0f,
                            blackbox ? blackbox_tint : glm::vec3(1.0f));
        }"""
assert s.count(old) == 1, 'cargo lod'
s = s.replace(old, new)

# cargo loop needs an index: find the loop and add one
old = """    const glm::vec3 blackbox_tint(1.0f, 0.82f, 0.62f);
    for (const Cargo &cargo : world.cargos) {
        if (cargo.collected) continue;"""
new = """    const glm::vec3 blackbox_tint(1.0f, 0.82f, 0.62f);
    int cargo_index = 0;
    for (const Cargo &cargo : world.cargos) {
        ++cargo_index;
        if (cargo.collected) continue;"""
assert s.count(old) == 1, 'cargo index'
s = s.replace(old, new)

old = """    for (const Ore &chunk : world.ore) {
        const glm::vec3 at = relative(origin, chunk.x, chunk.y, 0.0);
        if (!view.contains(at, 12.0f)) {
            ++culled;
            continue;
        }
        scene.add_model(models.store.model("ore"), at,
                        spin_about_z(static_cast<float>(chunk.id) * 0.7f), 1.0f, false, 0.0f);
    }"""
new = """    for (const Ore &chunk : world.ore) {
        const glm::vec3 at = relative(origin, chunk.x, chunk.y, 0.0);
        if (!view.contains(at, 12.0f)) {
            ++culled;
            continue;
        }
        if (select(2000 + static_cast<unsigned long long>(chunk.id), chunk.x, chunk.y, 7.0) >= 1) {
            scene.add_model(lod_model(models, "ore", 2), at,
                            spin_about_z(static_cast<float>(chunk.id) * 0.7f), 1.0f, false, 0.0f);
        }
    }"""
assert s.count(old) == 1, 'ore lod'
s = s.replace(old, new)

open(p, 'w', encoding='utf-8', newline='\n').write(s)
print('scene.cpp ok')

# caller in frame.cpp
p = 'src/game/frame.cpp'
s = open(p, encoding='utf-8').read()
old = """    build_scene(app.scene, app.models, app.world, app.backdrop, app.camera, static_cast<float>(width),
                static_cast<float>(height));"""
new = """    build_scene(app.scene, app.models, app.world, app.backdrop, app.camera, static_cast<float>(width),
                static_cast<float>(height), app.lod_memory);"""
assert s.count(old) == 1, 'frame.cpp caller'
s = s.replace(old, new)
open(p, 'w', encoding='utf-8', newline='\n').write(s)
print('frame.cpp ok')

# app.h: the lod memory
p = 'src/game/app.h'
s = open(p, encoding='utf-8').read()
old = """    /** Reused every frame so instance and run vectors keep their capacity. */
    SceneBuilder scene;"""
new = """    /** Reused every frame so instance and run vectors keep their capacity. */
    SceneBuilder scene;
    /** The LOD hysteresis (plan 05 s2.5): per-object level, kept across frames. */
    std::unordered_map<unsigned long long, int> lod_memory;"""
assert s.count(old) == 1, 'app.h lod'
s = s.replace(old, new)
if '#include <unordered_map>' not in s:
    old2 = '#include <string>'
    assert s.count(old2) == 1
    s = s.replace(old2, '#include <string>\n#include <unordered_map>')
open(p, 'w', encoding='utf-8', newline='\n').write(s)
print('app.h ok')
