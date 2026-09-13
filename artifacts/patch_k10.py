
# 1. bindings: Y = weapons free, T = torpedo
p = 'src/game/bindings.h'
s = open(p, encoding='utf-8').read()
old = """    /** The planner: insert the selected body's transfer burns, and advance to the first of them. */
    PlanNode,
    WarpToNode,
};"""
new = """    /** The planner: insert the selected body's transfer burns, and advance to the first of them. */
    PlanNode,
    WarpToNode,
    /** Close quarters (plan 05 s5): the PDCs' release, and a torpedo away at the tracked contact. */
    WeaponsFree,
    FireTorpedo,
};"""
assert s.count(old) == 1, 'bindings.h'
s = s.replace(old, new)
open(p, 'w', encoding='utf-8', newline='\n').write(s)
print('bindings.h ok')

p = 'src/game/bindings.cpp'
s = open(p, encoding='utf-8').read()
old = """    {Action::WarpToNode, "B", "warp to node", "advances to the next planned burn, exactly",
     SDL_SCANCODE_B, SDL_SCANCODE_UNKNOWN},
};"""
new = """    {Action::WarpToNode, "B", "warp to node", "advances to the next planned burn, exactly",
     SDL_SCANCODE_B, SDL_SCANCODE_UNKNOWN},
    {Action::WeaponsFree, "Y", "weapons free / hold",
     "the point-defence mounts engage the tracked contact inside 700 m",
     SDL_SCANCODE_Y, SDL_SCANCODE_UNKNOWN},
    {Action::FireTorpedo, "T", "torpedo away", "proportional navigation on the tracked contact",
     SDL_SCANCODE_T, SDL_SCANCODE_UNKNOWN},
};"""
assert s.count(old) == 1, 'bindings.cpp'
s = s.replace(old, new)
open(p, 'w', encoding='utf-8', newline='\n').write(s)
print('bindings.cpp ok')

# 2. frame.cpp: read the keys
p = 'src/game/frame.cpp'
s = open(p, encoding='utf-8').read()
old = """    // The planner, live in the flight view (J2 replaced the map screen): N inserts the selected
    // body's transfer burns, B advances to the first of them exactly.
    if (pressed(input, Action::PlanNode)) app.plan_transfer();"""
new = """    // Close quarters (plan 05 s5): the PDCs' release, then a torpedo at whatever is tracked.
    if (pressed(input, Action::WeaponsFree)) {
        app.world.weapons_free = !app.world.weapons_free;
        app.toast(app.world.weapons_free ? "Weapons free" : "Weapons hold");
    }
    if (pressed(input, Action::FireTorpedo)) {
        if (app.world.fire_torpedo()) {
            app.toast("Torpedo away");
        } else {
            app.toast("No torpedo - nothing tracked, or docked");
        }
    }
    // The planner, live in the flight view (J2 replaced the map screen): N inserts the selected
    // body's transfer burns, B advances to the first of them exactly.
    if (pressed(input, Action::PlanNode)) app.plan_transfer();"""
assert s.count(old) == 1, 'frame.cpp keys'
s = s.replace(old, new)
open(p, 'w', encoding='utf-8', newline='\n').write(s)
print('frame.cpp ok')

# 3. app.cpp: PDC mounts from the sidecar's hardpoints
p = 'src/game/app.cpp'
s = open(p, encoding='utf-8').read()
old = """    // The collar radius and the cutter muzzle want one pair of extents, not a shape list.
    world.ship.bounds = collider_bounds(collider);
}"""
new = """    // The collar radius and the cutter muzzle want one pair of extents, not a shape list.
    world.ship.bounds = collider_bounds(collider);
    // The PDC mounts ride the sidecar's hardpoints (s5.2): every `pdc.` anchor in the model is a
    // turret, in ship-frame metres, exactly the conversion the collider shapes went through.
    world.pdc_mounts.clear();
    for (const auto &[id, at] : meta.hardpoints) {
        if (id.rfind("pdc.", 0) != 0) continue;
        world.pdc_mounts.push_back({at.x * scale, at.y * scale});
    }
}"""
assert s.count(old) == 1, 'app.cpp mounts'
s = s.replace(old, new)
open(p, 'w', encoding='utf-8', newline='\n').write(s)
print('app.cpp ok')

# 4. loop.cpp: rounds in flight force warp 1x (s2.7)
p = 'src/game/loop.cpp'
s = open(p, encoding='utf-8').read()
old = """            accumulator = 0.0;
            if (app.warp.update(thrusting, app.world.contact_recent(), app.world.soi_switched,
                                app.world.air_density > 0.0)) {
                app.toast("Warp dropped to 1x");
            }"""
new = """            accumulator = 0.0;
            // s2.7's new rule: any round or torpedo in flight is real time - a warp step would
            // advance them past what their swept segments can be trusted over.
            if (app.world.combat_active()) {
                if (app.warp.drop_to_real_time(Warp::Drop::Zoom)) app.toast("Warp dropped to 1x - weapons live");
            } else if (app.warp.update(thrusting, app.world.contact_recent(), app.world.soi_switched,
                                       app.world.air_density > 0.0)) {
                app.toast("Warp dropped to 1x");
            }"""
assert s.count(old) == 1, 'loop.cpp railed'
s = s.replace(old, new)
old = """            while (accumulator >= dt) {
                app.world.step(flight, dt);
                app.now += dt;
                accumulator -= dt;
            }
            const bool thrusting = flight.thrust != 0.0 || flight.turn != 0.0 ||
                                   flight.strafe != 0.0 || flight.brake;
            app.warp.update(thrusting, app.world.contact_recent(), app.world.soi_switched,
                            app.world.air_density > 0.0);"""
new = """            while (accumulator >= dt) {
                app.world.step(flight, dt);
                app.now += dt;
                accumulator -= dt;
            }
            const bool thrusting = flight.thrust != 0.0 || flight.turn != 0.0 ||
                                   flight.strafe != 0.0 || flight.brake;
            if (app.world.combat_active()) {
                if (app.warp.drop_to_real_time(Warp::Drop::Zoom)) {
                    app.toast("Warp dropped to 1x - weapons live");
                }
            } else {
                app.warp.update(thrusting, app.world.contact_recent(), app.world.soi_switched,
                                app.world.air_density > 0.0);
            }"""
assert s.count(old) == 1, 'loop.cpp step'
s = s.replace(old, new)
open(p, 'w', encoding='utf-8', newline='\n').write(s)
print('loop.cpp ok')
