# Plan 07 — the ship kit: spines, parts, assembly, wreckage

> Live plan, with [PLAN-06-instrument.md](PLAN-06-instrument.md). This one is the brief for the
> three.js authoring agent and the engine work that makes a ship an assembly instead of a mesh.
>
> Workbench: **`artifacts/yard/`** — bun + three 0.183.2, wired to the real exporter. Read its
> README first; it runs the gates this plan sets.

---

## 0. Why the hulls fail, measured

Every one of the 33 shipping models passes the existing audit. The Kestrel still reads as a lozenge.
So the audit was measuring the wrong things. Run through `artifacts/yard/gates.mjs`:

```
                complexity    section CV    top material
kestrel          26.0           0.11        lightArmor 61%
mule            500.6           0.35        dark       31%
needle           86.6           0.51        ceramic    34%
station        1795.7           1.00        hullPaint  69%
```

**Complexity** is perimeter²/area. A circle scores 4π = 12.6. A 3:1 rectangle scores 21.3. The floor
was **22** — four points above a rectangle. The Kestrel cleared it by four. It *is* a rectangle with
a nick in it, and the number says so.

**Section CV** is std/mean of the plan-view width sampled per station. The Kestrel's run:

```
kestrel  13.7 16.0 16.3 16.3 13.7 17.0 17.0 16.3 16.3 15.7 13.7 11.7    CV 0.11
mule     25.7 21.3 18.0 25.0 25.0 25.0 25.0 25.0 25.0 25.0 21.7  9.7 12.0 7.0 7.7   CV 0.35
```

Constant to ±1 m for its whole length. Nothing to read along it.

**Top material**: `lightArmor` 61% plus `armor` 19% is **80% of the plan view in one grey family**.
Teal 8%, ochre 8%. That is "no colour differentiation" as arithmetic.

The mule and the station score 20–70× higher on complexity for one reason: **they are assemblies**.
Parts standing proud in space, with gaps between them. An extruded outline cannot reach those
numbers no matter how the outline is drawn — its perimeter is bounded by its convex hull. That is
the case for the spine kit, and it is measured rather than asserted.

The current hulls are also not salvageable by editing: a single extruded plate has no seam to split.
They are deleted and rebuilt.

---

## 1. Decisions

| id | decision |
|---|---|
| **M1** | Three spine families: **truss**, **keel**, **monocoque**. The family is the silhouette. |
| **M2** | **Runtime composition.** One draw per part; the engine concatenates colliders, ports, hardpoints and effect nodes. Forced by M5 — a baked mesh cannot come apart. |
| **M3** | Discrete **stations** at 4 m pitch × **six facings**. Symmetry is an optional per-placement tool, never automatic. |
| **M4** | Every part is its own GLB + sidecar, authored flange-at-origin. |
| **M5** | On destruction **every part detaches** as its own debris body with a particle burst. |
| **M6** | Ships are rebuilt. `prims.ts` stays, and so do stations, rocks, pads, buoys and props. |
| **M7** | Ship complexity floor **22 → 60**; two new gates: section CV ≥ 0.20, top material ≤ 45%. |
| **M8** | Panel lines, rivets and seams stay in the maps, never geometry (unchanged from plan 05 J6). |
| **M9** | The three stock hulls become **designs**, not models. Plan 05 §6.2's 2% reproduction test now has something to bite on. |

---

## 2. Spine families

Ship frame throughout: **nose +Y, starboard +X, dorsal +Z**, metres. Same as `sim/component.h`.

| family | half-width | recess | stations | reads as |
|---|---|---|---|---|
| **truss** | 1.6 m | 0 | 3 / 5 / 8 | open lattice; parts stand fully proud with sky between them. Haulers, tugs, survey. |
| **keel** | 3.2 m | 0.8 m | 3 / 5 / 8 | armoured spine, parts half-sunk in flank bays. Warships. |
| **monocoque** | 4.4 m | 0 | 2 / 3 | short fat pressure hull, parts flush, few stations. Couriers, landers. |

Eight spines: `spine_truss_s/m/l`, `spine_keel_s/m/l`, `spine_mono_s/m`.

The families exist so that three ships are distinguishable **before a single part is bolted on**.
A truss spine alone already scores high on complexity and CV; a monocoque alone scores low, and earns
its variance from what is mounted to it. Both are correct — the gates are applied to the *assembled
design*, not to the bare spine.

### 2.1 Station geometry

A spine declares `stations: n` and `pitch: 4.0`. Station *i* (0-based, foremost first) sits at

```
y_i = (n - 1)/2 * pitch  -  i * pitch
```

n=5, pitch=4 → `y = +8, +4, 0, -4, -8`, centred on the spine's origin. The spine's physical extent is
`±L/2` with `L = n * pitch`, so the end caps sit half a pitch beyond the end stations.

---

## 3. Mounting

### 3.1 The part convention

**Every part is authored with its attachment face at its own −Y end, the flange centred on the
origin of that face, body extending toward +Y.**

This single convention is what makes the mount transform one line instead of a per-part table. A part
authored any other way will mount inside the spine, and that is the first thing to check when one
does.

`FLANGE_RADIUS` is 1.25 m and `flange()` already exists in `prims.ts`. Use it; do not re-author it.

### 3.2 The six facings

```
fore       (0, +1,  0)     q = identity
aft        (0, -1,  0)     q = rotZ(pi)
starboard (+1,  0,  0)     q = rotZ(-pi/2)
port      (-1,  0,  0)     q = rotZ(+pi/2)
dorsal     (0,  0, +1)     q = rotX(+pi/2)
ventral    (0,  0, -1)     q = rotX(-pi/2)
```

Each `q` takes the part's local +Y onto the facing direction. Verify against `rotZ(t): (0,1,0) ->
(-sin t, cos t, 0)` and `rotX(t): (0,1,0) -> (0, cos t, sin t)` — the signs above are the ones that
come out, and plan 02's D-1 was a bug of exactly this kind, so they are written out rather than
inferred.

### 3.3 The mount transform

```
offset_radial = half_width(family) - recess(family)
offset_axial  = L_spine / 2

pos = (0, y_i, 0) + face_dir(f) * (axial(f) ? offset_axial : offset_radial)
rot = q_face(f) * rotY_local(roll)          roll in 90-degree steps
```

For fore/aft the station index is ignored — an axial part bolts to the end cap and stacks outward
from it.

### 3.4 Thrust direction — read this twice

A drive's flange bolts to the spine's aft cap and its bell points **further aft**. With
flange-at-origin and body toward +Y, mounting it aft-facing puts the bell at ship −Y, which is right.
So:

```
thrust_dir = -(rot * (0,1,0))
```

**Negative.** A drive's thrust is opposite its body axis. An aft-mounted drive gives `-(-Y) = +Y`.
Getting this backwards produces a ship that flies nose-first into its own exhaust and looks almost
right while doing it.

`DRIVE_ARC` (15°) becomes `dot(thrust_dir, +Y) >= cos(15°)`.

### 3.5 Overlap

Each placement occupies a set of (station, facing) cells: a radial part of `span` s at station i
takes cells `(i .. i+s-1, f)`. An axial part takes `s` virtual stations beyond the end cap, so a nose
cap and a forward tank stack rather than collide.

```cpp
// 16 stations (8 real + 4 fore + 4 aft) x 6 facings. A mount-time check runs when a human clicks,
// so a bool array is the right amount of cleverness.
// ponytail: std::bitset<96> if a procedural generator ever mounts thousands per second.
std::array<bool, 96> occupied;
```

`mount_component` rejects on intersection and returns false without mounting — the function already
has that contract (`sim/component.h:101`), it just gains an exact test instead of a distance check.

Set intersection is **exact**: no geometry test, no tolerance, no interpenetration possible. Two
parts that respect the pitch cannot overlap, which is the property the flange system was for.

### 3.6 Symmetry

`mirror` on a placement generates a paired placement at the opposite facing (port ↔ starboard,
dorsal ↔ ventral), same station, sharing a `group` id.

| rule | why |
|---|---|
| rejected for fore/aft facings | there is no mirror partner. The shipyard **greys** the toggle rather than silently ignoring the flag |
| the pair's `roll` is negated | a chirally-asymmetric part then reads as mirrored rather than rotated |
| removing one in the shipyard removes both | it is an editing convenience |
| **damage does not propagate across the pair** | they are separate bodies. Losing the port PDC must not lose the starboard one |

That last row is the one that matters in flight and is the easiest to get wrong by treating the group
as a physical link. It is not — it only exists in the editor.

### 3.7 Derived stats

Unchanged formulas from `sim/component.h`, now over real transforms:

```
mass    = sum m_i
com     = sum(m_i * pos_i) / mass                 ( mass <= 0 -> origin )
thrust  = sum thrust_i   over drives within DRIVE_ARC of +Y
torque  = sum | thrust_i * ((pos_i - com) x axis_i) |   over RCS blocks
fuel    = sum propellant_i
hull    = sum heat_capacity_i
cooling = sum cooling_i
bounds_radius = max over parts of ( |pos_i| + r_i )
```

**Port id collision.** Two docking collars each author port `"A"`. Ports declare a *local* id; the
design assigns the final one in placement order — A, B, C… — deterministically, and records the
mapping so a target saved against a port survives a refit that adds another.

Colliders concatenate, each part's shapes transformed by its mount. Plan 05's single-MTV rule is
unchanged: resolve against one shape, never sum corrections across shapes.

---

## 4. The part catalogue

34 parts. `span` is stations occupied; `axial` parts stack on the spine axis, radial parts hang off
it. Mass in tonnes, thrust in kN, propellant in tonnes.

### 4.1 Spines

| part | family | stations | length | mass | note |
|---|---|---|---|---|---|
| `spine_truss_s` | truss | 3 | 12 m | 2.4 | three bays, open; longerons and diagonals only |
| `spine_truss_m` | truss | 5 | 20 m | 3.8 | |
| `spine_truss_l` | truss | 8 | 32 m | 6.0 | |
| `spine_keel_s` | keel | 3 | 12 m | 5.2 | armoured box, flank bays 0.8 m deep at each station |
| `spine_keel_m` | keel | 5 | 20 m | 8.4 | |
| `spine_keel_l` | keel | 8 | 32 m | 13.2 | |
| `spine_mono_s` | monocoque | 2 | 8 m | 4.4 | pressure hull, ogival section |
| `spine_mono_m` | monocoque | 3 | 12 m | 6.6 | |

The truss spines are the ones that must read as **open**. Gaps you can see stars through are not
decoration — they are where the complexity score comes from, and a truss drawn as a textured box
will fail its gate.

### 4.2 Drives — aft facing

| part | span | mass | thrust | note |
|---|---|---|---|---|
| `drive_fusion_heavy` | 2 axial | 9.0 | 2400 | one bell filling the width, long throat |
| `drive_fusion_main` | 1 axial | 5.0 | 1200 | the default |
| `drive_fusion_light` | 1 axial | 2.6 | 520 | short bell, exposed turbopump |
| `drive_ion_cluster` | 1 axial | 3.4 | 90 | nine small throats on a plate; high specific impulse, in the sidecar |

Each carries its own `flame` effect node at the bell mouth.

### 4.3 Tanks

| part | span | mass | propellant | note |
|---|---|---|---|---|
| `tank_sphere` | 1 radial | 1.2 | 12 | pressure sphere in a cradle, reads round against everything else |
| `tank_drum_1` | 1 axial | 1.8 | 20 | |
| `tank_drum_2` | 2 axial | 3.2 | 42 | girth band at the join |
| `tank_saddle` | 1 radial | 1.0 | 9 | conformal, hugs the spine; the low-profile option |

### 4.4 Hab and command

| part | span | mass | hull | note |
|---|---|---|---|---|
| `bridge_forward` | 1 axial | 4.2 | 90 | fore facing. Sensor windows ≥ 1.9 × 2.1 m; no glasshouse |
| `hab_ring_short` | 2 radial | 6.8 | 140 | partial ring, breaks the spine's line hard |
| `hab_can` | 1 radial | 3.0 | 70 | |

### 4.5 Weapons

| part | span | mass | note |
|---|---|---|---|
| `pdc_turret` | 1 radial | 1.4 | traversing drum + barrels. Carries a `pdc.*` hardpoint |
| `torpedo_cell` | 1 radial | 2.2 | four tubes in a block |
| `mining_cutter` | 1 radial | 1.8 | boom + emitter head |

### 4.6 Utility

| part | span | mass | cooling | note |
|---|---|---|---|---|
| `radiator_fin` | 1 radial | 0.8 | 6 kW | |
| `radiator_wing` | 2 radial | 1.6 | 14 kW | the big silhouette-maker; deployed only |
| `rcs_quad` | 1 radial | 0.4 | — | carries an `rcs-jet` effect node |
| `sensor_mast` | 1 radial | 0.6 | — | |
| `docking_collar` | 1 radial | 1.1 | — | declares a port |
| `landing_gear` | 1 radial | 1.3 | — | ventral; declares a ground contact |

### 4.7 Cargo and caps

| part | span | mass | note |
|---|---|---|---|
| `cargo_rack` | 2 radial | 1.4 | open frame; pods visible in it |
| `cargo_pod_ext` | 1 radial | 0.9 | |
| `ore_hopper` | 2 radial | 2.0 | |
| `nose_ogive` | 1 axial | 1.6 | fore facing |
| `nose_blunt_armour` | 1 axial | 3.4 | fore facing |
| `nose_sensor` | 1 axial | 1.2 | fore facing |

### 4.8 Build order

1. `spine_truss_m`, `drive_fusion_main`, `tank_drum_1`, `nose_ogive` — four parts make one ship.
   Assemble and run the gates on the **design** before authoring anything else. If four parts on a
   truss cannot clear complexity 60, the numbers are wrong and it is better to learn that now.
2. The rest of the truss-family kit, then keel, then monocoque.
3. The three stock designs (§7), and the 2% reproduction test.

---

## 5. Authoring rules and gates

Unchanged from plan 05 §3.2: minimum feature **1.5 m**, minimum identity feature **3.0 m**, minimum
material region **6 m²**, identity bands **3.5 m**, panel lines and rivets in maps only (M8).

New, and the reason §0 exists:

| gate | applies to | rule |
|---|---|---|
| complexity | assembled **designs** | ≥ **60** (was 22; a rectangle is 21.3) |
| section CV | assembled designs | ≥ **0.20** — std/mean of plan-view width per station |
| top material | parts **and** designs | ≤ **45%** of plan-view area |
| triangles | each part | 120–400. A 10-part ship lands at 1,200–4,000, the same as one hull today, spent on silhouette instead of surface |
| blocky LOD | each part | ≤ 60 tris, or the part is **omitted** from blocky — small parts vanishing at 8–60 px is correct |
| axial length | axial parts | a whole multiple of the 4 m pitch |
| flange | every part | present at −Y, radius 1.25 m, on the origin |

Exemptions, declared in the sidecar rather than assumed: `tank_*` and `radiator_*` are legitimately
monolithic and are exempt from the top-material rule. Nothing else is.

**Range-based variance does not work** and was tried: it is dominated by the single narrowest band,
scoring 0.31 for the Kestrel against 0.73 for the mule — no usable separation. CV gives 0.11 against
0.35. The yard README carries the working.

**The 60 floor is provisional until step 1 of §4.8.** If the first four-part truss design cannot
clear it, the floor moves and the reasoning gets written down — but the needle already scores 86.6
as a single mesh, so an assembly failing 60 would be surprising and worth understanding.

### 5.1 Bench vs. exporter

The exporter runs `enforce_min_feature` and `consolidate_regions` before auditing; the yard bench
does not. The bench is stricter on purpose — it reports what you authored, not what the exporter
repaired for you. Note which way `consolidate_regions` pushes the third gate: merging stray materials
into the dominant one makes dominance **worse**. `station` reads 69% for that reason.

---

## 6. Runtime composition

### 6.1 Sidecar

Per part, extending the existing schema:

```json
{
  "part": "tank_drum_2",
  "kind": "tank",
  "span": 2,
  "axial": true,
  "exempt": ["top_material"],
  "flange": { "pos": [0, -4, 0], "normal": [0, -1, 0] },
  "mass": 3.2, "propellant": 42.0, "thrust": 0, "cooling": 0, "heat_capacity": 28,
  "collider": { "shapes": [ ... ] },
  "ports": [ ... ], "hardpoints": { ... }, "effects": [ ... ]
}
```

Per spine, additionally: `"family"`, `"stations"`, `"pitch"`, `"half_width"`, `"recess"`.

### 6.2 Draw

`game/scene.cpp:325` currently draws one model per ship. It becomes:

```cpp
const int level = select(id, x, y, bounds_radius);
if (level < 1) return;                       // icon owns it
for (const Placement &p : design.placements) {
    if (p.destroyed || !draws_at(p, level)) continue;
    const Mount m = mount_transform(design, p);         // pure, headless-testable
    scene.add_model(lod_model(models, p.part, level),
                    relative(origin, ship.x, ship.y, 0) + rotate(ship.angle, m.pos),
                    spin_about_z(ship.angle) * m.rot, ...);
}
```

`mount_transform` is a pure function of (spine, placement) and is where the §3.3 maths lives. It gets
a unit test per facing, asserting the six directions come out as §3.2 writes them.

**LOD is selected once per ship, not per part** — otherwise parts pop in and out independently and
the hull appears to shed pieces while flying.

| level | draws |
|---|---|
| 3 (>250 px) | every placement |
| 2 (60–250 px) | `span >= 1`; rcs, sensor and lamp parts drop |
| 1 (8–60 px) | spine and drives only |
| 0 (<8 px) | icon; no geometry |

Instance cost: a 12-part ship × 8 ships is 96 instances against 8 today. Irrelevant to the renderer,
but stated so nobody optimises it pre-emptively.

### 6.3 The one sim change composition forces

`sim/physics.h:87` declares `Real rcsJet[4]` — a fixed four-vector matching four authored corners. A
modular ship can carry two RCS blocks or eight, anywhere. `rcsJet` becomes a vector sized to the
design's RCS placements and the allocator solves per block, indexed by placement.

This is the only sim change M2 forces. Everything else in `sim/` already takes a `ShipDesign`.

---

## 7. Designs, and the acceptance test

The three stock hulls stop being models and become `assets/designs/*.json`: a spine plus a placement
list. `derive_spec()` over each must reproduce today's `SHIPS[]` row **within 2%** (plan 05 §6.2).

That test has been unfalsifiable since it was written, because no design existed to run it against.
It is the gate that proves the kit can express the game that already exists — if a component set
cannot reproduce the Kestrel's mass, thrust, torque, fuel, hull and cooling, the kit is wrong and no
amount of good-looking parts fixes it.

---

## 8. Wreckage (M5)

On a part's integrity reaching zero, or on hull loss, each surviving placement detaches:

```
r      = pos_part - com
n_hat  = normalize(r)                        |r| < 0.1 -> (1,0,0)
s      = clamp(0.6 * sqrt(E_blast / m_total), 4, 60)      m/s
v_sep  = v_ship + omega_ship x r + n_hat * s
omega  = omega_ship + (hash01(index)*2 - 1) * 1.5         rad/s
spawn  = pos_part + n_hat * part.bounds_radius
```

The `omega x r` term is what makes a spinning wreck throw its parts tangentially instead of radially.
Without it a blast looks like an explosion diagram; with it, it looks like a ship coming apart.

`hash01` already exists (`game/effects.h:162`) and is deterministic — no RNG call, matching the
convention the effects layer set so a replay repeats.

The spawn offset is plan 05 §5.5's muzzle rule: a body never starts inside the hull it came from.

### 8.1 Particles

Pool is `POOL_SLOTS 96 / POOL_MAX 192` (`effects.h:85`). A 12-part ship at 12 grains each would be
144 and can coincide with ongoing contacts. Budget: **one 24-grain central puff plus 4 grains per
part** — 72 for a 12-part ship — and raise `POOL_MAX` to 256.

### 8.2 Debris is wreckage, not particles

Detached parts persist for the run as collidable bodies and are salvage targets. Cap **48**; the
oldest retires past that. They draw at the part's own LOD and carry their own collider.

### 8.3 Edge cases

| case | handling |
|---|---|
| blast while docked | offset the spawn by the **station's** bounds radius too, or debris appears inside the station |
| blast during warp | force warp to 1×. Plan 05 §2.7 already does this for rounds in flight; this extends the same rule |
| the spine is destroyed | every remaining placement detaches at once — the spine is what held them |
| the last drive is lost | thrust becomes 0. The HUD reports it; nothing divides by it |
| `m_part <= 0` | skip the sqrt; separation speed is the floor |
| part sitting on the COM | `|r| < 0.1` → `n_hat = +X`, deterministic rather than NaN |
| ship at icon LOD when it blows | spawn debris anyway. It is simulation, not a visual; just do not draw it |
| debris vs. the player on the spawn frame | handled by the spawn offset |
| a mirrored pair | detaches as two independent bodies. The group is an editor concept only (§3.6) |

---

## 9. Phases

| phase | work | depends |
|---|---|---|
| **P0** | `artifacts/yard/` bench — **done**; `export.mjs` CLI guarded so the audit imports cleanly | — |
| **P1** | `mount_transform` + occupancy + symmetry in `sim/`, with per-facing unit tests. No art | — |
| **P2** | Four-part proof design: truss spine, drive, tank, nose cap. Gates run on the **design** | P1 |
| **P3** | Runtime composition in `scene.cpp`; `rcsJet` becomes per-placement | P1, P2 |
| **P4** | The truss-family kit in full | P2 |
| **P5** | Keel and monocoque families | P4 |
| **P6** | The three stock designs + the 2% reproduction test | P3, P5 |
| **P7** | Wreckage: detach, separation, particle budget, the 48-body cap | P3 |
| **P8** | Retire `kestrel.ts` / `mule.ts` / `needle.ts` and their GLBs | P6 |
| **P9** | Raise the exporter's ship floor to 60 and fold both new gates into `--audit` | P6 |

P8 comes after P6 and not before: the old hulls are the reference the 2% test is measured against,
so they are deleted once they have been reproduced, never sooner.

---

## 10. Gates

1. `mount_transform` returns the six §3.2 directions exactly, one assert per facing.
2. Two parts whose cells intersect are rejected by `mount_component`, which returns false and leaves
   the design unmodified.
3. `mirror` on a fore or aft facing is rejected, and the shipyard shows the toggle greyed.
4. Destroying one half of a mirrored pair leaves the other half flying.
5. Every stock design clears complexity ≥ 60, section CV ≥ 0.20, top material ≤ 45%.
6. Pairwise silhouette IoU < 0.70 across the three stock designs (plan 05 §3.4, on designs now).
7. `derive_spec()` over each stock design reproduces its `SHIPS[]` row within 2%, all six figures.
8. A design with no drive is flagged, and launch is refused with a reason (plan 06 §4.3).
9. Blowing up a 12-part ship spawns 12 debris bodies and ≤ 72 particles, and the pool never
   overflows.
10. No debris body's spawn position lies inside the collider it detached from — asserted, per part.
11. A replayed blast from the same state produces byte-identical debris state: no RNG was called.
12. Every part carries a flange at −Y of radius 1.25 m, checked at export.
