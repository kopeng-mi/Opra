# PLAN-13 — Jump: the gravity gate and the second system

Depends on PLAN-12. Adds interstellar travel, gated by local gravity, and makes the engine hold more
than one star system at a time.

---

## 0. The naming rule — read this first

**`warp` in this codebase already means time acceleration.** `Warp`, `WARP_RATES`, `Warp::railed()`,
`World::warp_step()`, the F-key rail and every HUD string. It is correct and it stays.

The interstellar drive is **`Jump`**, in every symbol, string, file and comment. An agent that
renames or reuses `warp` will gut the time-acceleration rail, which is load-bearing for every long
transit in PLAN-12.

| concept | symbol | what it does |
|---|---|---|
| time acceleration | `Warp`, `warp_step` | advances the clock, ship rides its conic |
| interstellar drive | `Jump`, `jump_step` | moves the ship between `SystemDef`s |

---

## 1. Where the code is

| fact | where |
|---|---|
| `SystemDef::jump_links` is loaded from JSON and read by nothing | `src/sim/system.cpp:137`, `system.h:114` |
| `World::attach_system(const SystemDef&, int anchor)` already exists | `src/sim/world.h` |
| `load_system(path)` reads a system file; malformed is fatal | `src/sim/system.cpp` |
| `primary_of()` walks spheres of influence with hysteresis | `src/sim/system.cpp` |
| `World::primary` is the deepest body holding the ship, -1 for the star | `src/sim/world.h` |
| `World::soi_switched` is set when the last step crossed a boundary | `src/sim/world.h` |
| `propagate()` fills every body state at `t` | `src/sim/system.h` |
| The zone is an offset from `anchor_body`; local coords stay small | `src/sim/world.h` |
| Only one system file exists | `assets/systems/nereid.json` |

---

## 2. Decisions

| id | decision | consequence |
|---|---|---|
| **J1** | **Jump is gated on local gravity**, not on distance or on a station. | §3. This is the user rule: it only works outside high gravitation fields. |
| **J2** | **The threshold is authored per system**, in the system file, as `jump_gravity_max`. | §3.1. Data-driven, like everything else in `SystemDef`. |
| **J3** | **Gravity is summed over every body**, not taken from the primary alone. | §3. A ship between two masses is held by both. |
| **J4** | **The drive charges.** `JUMP_CHARGE_SECONDS` of continuous legality; any thrust, contact, or re-entry into the gate aborts the charge. | §4. Gives the moment weight and makes the gate readable. |
| **J5** | **Systems are loaded lazily and cached.** `SystemStore` holds every `SystemDef` seen this run. | §5 |
| **J6** | **A jump arrives at the destination's own jump line**, on a circular orbit, at a bearing derived from the link. Never inside a gravity well. | §5.2. Symmetric: you leave from the line, you arrive at the line. |
| **J7** | **The zone rebuilds on arrival.** Rocks, cargo and contacts are regenerated from the destination's seed. | §5.3 |
| **J8** | **Time warp drops to 1x on arrival.** | §5.2. Same rule as `soi_switched`. |

---

## 3. The gravity gate (J1, J3)

```
g_local = SUM over all bodies i:   mu_i / r_i^2          [m/s^2]

jump_legal = g_local <= system.jump_gravity_max
```

Summed over every body (J3), so a ship parked between a planet and its moon is correctly held by
both. Stations have `mu = 0` and contribute nothing, which is already true in `SystemDef`.

### 3.1 The threshold and where the line falls

`jump_gravity_max = 5.0e-3 m/s^2`, authored in the system file (J2).

For Nereid (`mu = 2.07e19`), the star alone puts the line at

```
r_jump = sqrt(mu / g_max) = sqrt(2.07e19 / 5.0e-3) = 6.43e10 m = 64.3 Gm
```

Against the system as it stands:

| place | radius | local g | jump? |
|---|---|---|---|
| Cinder orbit | 7.2 Gm | 0.400 m/s^2 | no |
| Tessera orbit | 17.2 Gm | 0.070 m/s^2 | no |
| **Wayfarer / the belt** | **26.1 Gm** | **0.0304 m/s^2** | **no** |
| Halberd orbit | 43.4 Gm | 0.0110 m/s^2 | no |
| **the jump line** | **64.3 Gm** | **0.0050 m/s^2** | **yes** |

The line sits about 1.5x beyond the outermost planet. **Getting to it is a leg of the mission.**

> **The table above is against `nereid.json` as it ships today.** PLAN-17 §1.1 rescales the system
> for a real K1V primary (`mu = 1.206e20`), which moves the line to 1.038 AU / 155.3 Gm and the belt
> to 0.40 AU. The *rule* is unchanged — only the numbers the rule produces. Implement the gate
> against `system.jump_gravity_max` and the live `mu`, never against a constant, and the reskin costs
> nothing here.

### 3.2 What the pilot reads

Three numbers, one bar (PLAN-14 §3.3):

```
JUMP
  local g     0.0304          threshold 0.0050
  line        38.2 Gm out
  charge      ----------      locked
```

`line` is `sqrt(mu_dominant / g_max) - r_current` when outside every SOI, and "inside <body>"
otherwise. It is a hint, not a solution: the true gate is the summed `g_local`.

---

## 4. Charging (J4)

```cpp
struct Jump {
    enum class Block { None, Gravity, Thrust, Contact, Charging, NoLink, NoFuel };

    double charge = 0.0;          // seconds accumulated
    Block  block  = Block::None;
    int    link   = -1;           // index into system.jump_links

    bool legal(const World &w) const;
    void update(const World &w, Real dt);   // charges or resets, never jumps
    bool ready() const { return charge >= JUMP_CHARGE_SECONDS; }
};
```

`JUMP_CHARGE_SECONDS = 120.0` of **simulator** time, so time warp shortens the wait in real seconds
while keeping it a real interval in the fiction.

The charge resets to zero on any of:

- `g_local > jump_gravity_max` (drifted back inside)
- `ship.thrustLevel > 0.01` (the drive cannot charge under thrust)
- `world.contact_recent()` (something hit the hull)
- `world.combat_active()` (rounds or torpedoes in flight)

This mirrors `Warp::update`'s drop rules exactly, and for the same reason: the player must always be
able to read why the thing they asked for is not happening.

### 4.1 Cost

A jump spends transit propellant:

```
jump_cost_kg = JUMP_COST_PER_LY * link.distance_ly * mass_total / 1000.0
```

`JUMP_COST_PER_LY = 0.08`, so a 4.4 ly link on a 100 t ship costs about 35 t of transit propellant.
That is deliberately most of a tank: **a jump is the most expensive thing the ship does**, and the
return leg has to be planned for before departure.

---

## 5. Multiple systems

### 5.1 The store (J5)

```cpp
class SystemStore {
public:
    const SystemDef &get(const std::string &id);   // loads and caches on first ask
    bool has(const std::string &id) const;
private:
    std::unordered_map<std::string, SystemDef> loaded_;
};
```

`assets/systems/<id>.json`. `jump_links` becomes a list of objects rather than bare strings:

```json
"jump_links": [
  { "to": "rigil", "distance_ly": 0.21, "arrival_bearing": 2.47 }
]
```

Backwards compatible: a bare string is read as `{to, distance_ly: 1.0, arrival_bearing: 0.0}`, so
`nereid.json`'s existing empty list stays valid and `system_tests.cpp:41` keeps passing.

### 5.2 Executing a jump (J6, J8)

```
1. assert jump.ready() and link >= 0
2. deduct jump_cost_kg from prop_transit
3. dest = store.get(link.to)
4. r_arrive = sqrt(dest.dominant_mu / dest.jump_gravity_max)
   theta    = link.arrival_bearing
   pos      = r_arrive * (cos theta, sin theta)
   vel      = sqrt(dest.dominant_mu / r_arrive) * (-sin theta, cos theta)   // circular, prograde
5. world.attach_system(dest, dest.default_anchor)
6. world.ship.position/velocity = pos/vel relative to the new anchor
7. warp.drop_to_real_time(Warp::Drop::Sphere)
8. rebuild the zone (5.3)
9. jump.charge = 0
```

Step 4 places the arrival **on the destination's own jump line, on a circular orbit** (J6). You
never arrive inside a well, and you can always leave again — which matters, because a jump that
stranded the player would be a softlock.

### 5.3 Rebuilding the zone (J7)

`attach_system` sets the bodies and the anchor. The local sector contents do not follow
automatically:

```
world.rocks.clear(); world.fragments.clear(); world.ore.clear(); world.cargos.clear();
world.grid.rebuild();
world.rng = Rng(dest.zone_seed);
world.rocks  = create_obstacles(world.rng);
world.cargos = create_cargo(world.rng);
world.target = -1;                    // the tracked contact is gone
world.nodes.clear();                  // the plan was for the old system
```

`SpatialGrid` holds raw pointers into the `rocks` deque, so the clear-then-refill order matters and
`grid.rebuild()` must run after the deque is repopulated, never before. This is the one place in the
jump path that can dangle, and it is why `rocks` is a `deque` in the first place.

---

## 6. The chart

The jump line draws as a dashed circle at `r_jump` in the chart register, `VELLUM_RULE`. Inside it,
the ship's marker carries a small locked glyph; outside, the glyph opens and the charge bar appears.

Each `jump_link` draws as a labelled ray from the line outward at its `arrival_bearing`, with the
destination name and the propellant cost against the current tank. A link the ship cannot afford
draws in `THREAT`.

---

## 7. Gates

| # | gate | how |
|---|---|---|
| G1 | `g_local` at Wayfarer equals 0.0304 m/s^2 to 3 sf | unit |
| G2 | the jump line for Nereid is 64.3 Gm to 3 sf | unit |
| G3 | `jump_legal` is false everywhere inside Halberd's orbit | unit, sampled |
| G4 | summing over two bodies exceeds either alone at the midpoint | unit |
| G5 | thrust, contact or combat each reset the charge to exactly 0 | unit, one case each |
| G6 | a jump leaves `prop_transit` non-negative or is refused | unit |
| G7 | arrival is always outside the destination's gate | unit, over every link |
| G8 | arrival velocity is circular at the arrival radius to 1e-6 relative | unit |
| G9 | after a jump the grid holds no pointer into a freed rock | ASAN or a pointer-identity check |
| G10 | `system_tests.cpp:41` still passes with the new link schema | existing test, unchanged |
| G11 | the string `warp` appears in no jump symbol and vice versa | grep |

---

## 8. Files touched

```
new   src/sim/jump.h/.cpp        Jump, gravity gate, charge, execute
new   src/sim/system_store.h/.cpp SystemStore
new   assets/systems/rigil.json  the second system (PLAN-16 authors the contents)
edit  src/sim/system.h/.cpp      JumpLink struct, jump_gravity_max, zone_seed, default_anchor
edit  src/sim/world.h/.cpp       jump execution, zone rebuild
edit  src/game/app.cpp           SystemStore replaces the single load_system
edit  src/game/frame.cpp         the jump key, charge update
edit  src/hud/frame.cpp          jump readouts
edit  src/ui/screens.cpp         chart: jump line, links, costs
edit  assets/systems/nereid.json jump_gravity_max, zone_seed, one link to rigil
new   src/sim/jump_tests.cpp     G1-G11
```
