  ## 0. Where the code actually is

  Facts established by reading the tree, not assumed. Every one of these is load-bearing below.

  | fact | where |
  |---|---|
  | `mount_transform()` implements plan 07's **spine + end-cap** model: axial pa | `src/sim/component.h:139` |
  | The three yard designs are **chains** — no spine object, modules butt flange-to-flange at 4 m | `artifacts/yard/designs/variant_a_hammerhead.ts`, `DESIGN-DIRECTION-v2.md` |
  | The ship draws as **one merged model**, `SHIP_MODEL_NAMES[class]` | `src/gam
  | The collider comes from **one sidecar**, converted at load | `App::sync_ship_collider`, `src/game/app.cpp:143` |
  | `ShipState::spec` points into the static `SHIPS[3]`; `derive_spec()` is usedsim/physics.cpp:67-81`, `src/sim/component_tests.cpp:54` |
  | `rcsJet[4]` is **visual only** — it scales the drawn jet cones and nothing else | `src/sim/physics.cpp:52`, `src/render/scene.cpp:24-33` |
  | `state.rcsActive` and `spec.torque` are what actually turn the ship | `src/s
  | Part masses in the yard are **tonnes**, thrust **kN**; the sim is kg and N | `artifacts/yard/parts/*.ts` `meta`, `src/sim/physics.cpp:13` |
  | `prims.flange()` costs **920 triangles**; the per-part budget is 120–400 | ``PLAN-07 §5` |
  | The exporter grows any mesh under 1.5 m by `(1.5/smallest)²` before auditing | `enforce_min_feature`, `tools/export.mjs:195` |
  | `ModelStore::model()` / `meta()` are **fatal** on a missing name | `src/rend
  | `ui::Context` has no clock — no widget can animate today | `src/ui/ui.h:44` |
  | The plate's rings are drawn in the **additive backdrop pass** as unit cubes.:build`, `src/render/orrery.cpp:116-134` |
  | CMake lists every `.cpp` explicitly | `CMakeLists.txt:88` |
  | **The HUD's ΔV is 1000× low**: it divides thrust by `mdot = 14000.0` while `game/scene.cpp:1010` vs `src/sim/physics.cpp:130` |

  That last row is a real bug and this plan fixes it, because the shipyard has t
  the flight HUD does and there is no point shipping two wrong numbers that agree.

  ---

  ## 1. Decisions

  Taken with the user, this session. They are settled; do not relitigate them in code review.

  | id | decision | consequence |
  |---|---|---|
  | **A1** | **Chain architecture.** There is no spine object. A design is N axial slots at a 4 m pitch, modules flange-to-flange, radial pods bolted to a slot. | `mount_transform()` is
  rewritten (§3.1). Plan 07 §3.3's `offset_axial = L/2 + k·pitch` is **supersede grew out of both ends of a spine that no longer exists. |
  | **A2** | **Longer chains, `SHIPS[]` holds.** Stock designs are 10–14 modules (40–56 m) and the part stat table is re-authored so `derive_spec()` reproduces today's `SHIPS[]` rows. |
  §2's table. Flight feel, warp rail, heat and fuel tuning are untouched. The drm against 16 m) and slightly shorter; the collider follows the geometry, sonothing needs a second edit. |
  | **A3** | **`flange()` is re-authored at ~156 triangles** in `prims.ts`. One shipping models re-export. Visually identical past 10 m; a 2.5 m flange is 7.5px at the home framing. |
  | **A4** | **Full cinematic diorama** on the plate: the flagship berthed at Waa, real backdrop. The ephemeris survives as **2 D chart ink** behind the titleblock. | `orrery::build()` loses its caller (§9.4). The module stays — its `sample_ring`/`sample_belt` are what the 2 D ephemeris draws with. |
  | **A5** | **Full free assembly** in the shipyard: any catalogue module into aemove, live derived stats and hover deltas. | §8. This is PLAN-06 §4.3 andPLAN-07 §3 as written. |
  | **A6** | **The RCS allocator stays a sign test.** `rcsJet` only scales drawnres or NNLS allocator for a value no force reads is dead weight. | §3.6. Thefour-corner case must stay bit-identical to `solve_rcs` — `shapes_tests.cpp:167` asserts it. |
  | **A7** | **No blocky exports for modules** this plan. `lod_model()` already when `<name>_blocky` is absent. | Add them when a 12-module ship at 60 pxmeasurably costs frames, and not before. |
  | **A8** | Old hulls (`kestrel`/`mule`/`needle` `.glb` + `.ts`) **stay** untileen, then go. | Plan 07 P8's rule: the reference is deleted once it has beenreproduced, never sooner. |

  ---

  ### 2.1 Unit contract

  Authored in the module `meta` (tonnes, kN) — converted **once**, in the sidecar writer, and never
  again:

  | field | authored | sidecar | sim |
  | `heat_capacity` | points | points | `ShipSpec::hull` |
  | `cooling` | fraction/s | fraction/s | `ShipSpec::cooling` |
  | `rcs_authority` | rad/s² per metre of arm | same | feeds `ShipSpec::torque` |
  | `rcs_jets` | count of quads | same | feeds `strafeFraction` |

  `rcs_authority` is **not** newtons. `derived_torque()` sums `authority × |arm|
  `ShipSpec::torque`, which `step_ship` uses as an angular acceleration in rad/s²
  (`physics.cpp:144`). Authoring it in newtons and expecting the sim to divide b
  not compute is the mistake this row exists to prevent.

  ### 2.2 The re-authored part table

  Replaces the `mass` / `propellant` / `thrust` / `heat_capacity` / `cooling` lines in each
  `artifacts/yard/parts/*.ts` `meta`. Geometry, maps and names are **unchanged**
  per file. The three stock designs in §2.3 land on `SHIPS[]` exactly with these numbers.

  | part | kind | mass t | prop t | thrust kN | hull | cooling | rcs_jets | rcs_auth |
  |---|---|---|---|---|---|---|---|---|
  | `nose_hammerhead` | nose_command | **7.5** | 0 | 0 | **12** | 0 | **2** | **0.01534** |
  | `section_combat_a` | section_weapons | **9.0** | 0 | 0 | **14** | 0 | 0 | 0
  | `section_tank_saddle` | section_tank | **3.0** | **8.0** | 0 | **4** | 0 | 0 | 0 |
  | `section_machinery` | section_utility | **6.5** | 0 | 0 | **6** | **0.0075**
  | `section_radiator_wing` | section_utility | **4.0** | 0 | 0 | **3** | **0.020** | 0 | 0 |
  | `drive_twin_torch` | drive_fusion | **11.5** | 0 | **1600** | **6** | 0 | **
  | `nose_pushbow` | nose_armored | **12.0** | 0 | 0 | **20** | 0 | **2** | **0.00789** |
  | `section_freight` | section_cargo | **12.6** | 0 | 0 | **12** | 0 | 0 | 0 |
  | `section_reactor` | section_tank | **9.0** | **10.0** | 0 | **10** | 0 | 0 | 0 |
  | `drive_quad_block` | drive_fusion | **16.5** | 0 | **1950** | **19** | **0.0
  | `nose_stealth_needle` | nose_sensor | **2.5** | 0 | 0 | **7** | 0 | **2** | **0.02847** |
  | `section_delta_fore` | section_weapons | **4.5** | 0 | 0 | **9** | 0 | 0 | 0
  | `section_stealth_combat` | section_weapons | **6.0** | 0 | 0 | **11** | 0 | 0 | 0 |
  | `section_delta_aft` | section_utility | **4.0** | 0 | 0 | **6** | **0.025**
  | `drive_stealth_twin` | drive_fusion | **8.5** | 0 | **1200** | **10** | 0 | **2** | **0.02847** |
  | `tank_drum_1` | tank | **1.5** | **5.0** | 0 | **3** | 0 | 0 | 0 |
  | `nose_ogive` | cap | 1.6 | 0 | 0 | 20 | 0 | 0 | 0 |
  | `drive_fusion_main` | drive | 5.0 | 0 | 1200 | 30 | 0 | 0 | 0 |
  | `spine_truss_m` | spine | 3.8 | 0 | 0 | 40 | 0.01 | 0 | 0 |

  The last three are the plan-07 §4.8 proof parts. They are **not** used by a stock design and their
  numbers are left alone; `spine_truss_m` is dead under A1 and is not exported.

  Why `drive_quad_block` carries cooling and `drive_twin_torch` does not: B's dr
  block with its own radiator crown and it is the only place the Mule's 0.075 fits without inventing
  a part. A's drive vents through the hull. Write that in the file, in one line,
  does not "fix" the asymmetry.

  ### 2.3 The three stock designs

  ```
  dry      7.5 + 4(9.0) + 2(3.0) + 2(6.5) + 2(4.0) + 11.5   =  82.0 t   SHIPS  8
  fuel     2(8.0)                                            =  16.0 t   SHIPS  16.0   0.0 %
  thrust   1600 kN                                           = 1.60 MN   SHIPS
  hull     12 + 4(14) + 2(4) + 2(6) + 2(3) + 6               =   100     SHIPS   100   0.0 %
  cooling  2(0.020) + 2(0.0075)                              =  0.055    SHIPS 0
  com_y    (see below)                                       = -0.02 m
  torque   0.01534 · 2 · (22.02 + 21.98)                     =  1.350    SHIPS
  strafe   0.055 · (2 + 2)                                   =  0.220    SHIPS  0.22   0.0 %
  ```

  `com_y = Σ m·y / Σ m = −2.0 / 82.0 = −0.024 m`, so the nose arm is 22.02 m and

  **Mule** — `N = 14`, 56 m, Variant B parts:


  ```
  dry      12.0 + 5(12.6) + 3(9.0) + 3(6.5) + 4.0 + 16.5     = 142.0 t   SHIPS 1
  fuel     3(10.0)                                           =  30.0 t   SHIPS  30.0  0.0 %
  thrust   1950 kN                                           = 1.95 MN   SHIPS
  hull     20 + 5(12) + 3(10) + 3(6) + 3 + 19                =   150     SHIPS   150  0.0 %
  cooling  0.020 + 3(0.0075) + 0.0325                        =  0.075    SHIPS 0
  com_y    350 / 142                                         = +2.46 m
  torque   0.00789 · 2 · (23.54 + 28.46)                     =  0.821    SHIPS
  strafe   0.055 · 4                                         =  0.220    SHIPS  0.22  0.0 %
  ```

  **Needle** — `N = 10`, 40 m, Variant C parts:

  ```
  dry      2.5 + 2(4.5) + 2(1.5) + 2(6.0) + 2(4.0) + 8.5     =  43.0 t   SHIPS
  fuel     2(5.0)                                            =  10.0 t   SHIPS  10.0  0.0 %
  thrust   1200 kN                                           = 1.20 MN   SHIPS
  hull     7 + 2(9) + 2(3) + 2(11) + 2(6) + 10               =    75     SHIPS    75  0.0 %
  cooling  2(0.025)                                          =  0.050    SHIPS 0
  com_y    -114 / 43                                         = -2.65 m
  torque   0.02847 · 2 · (20.65 + 15.35)                     =  2.050    SHIPS
  strafe   0.055 · 4                                         =  0.220    SHIPS  0.22  0.0 %
  ```

  `cargo`, `role`, `scan_scale` and `collect_scale` are **not** derivable from p
  economy numbers, not physics — so the design file carries them and `derive_spec` copies them
  through. `length` is filled from the composed collider's bounds, which is what
  said it would be.

  ### 2.4 If you change a layout, the numbers move

  Every torque figure above depends on the slot order through `com_y`. Reordering two modules of
  different mass moves the centre of mass and both arms. **The calibration test
  deltas per ship.** When it goes red after an edit: adjust the drive's `rcs_authority` first — it has
  the longest arm and the largest lever on the result — and never adjust a mass

  ---

  ## 3. The maths

  Ship frame throughout: **nose +Y, starboard +X, dorsal +Z**, metres, right-han

  ### 3.1 The chain mount transform (replaces plan 07 §3.3)

  A design is `N` slots at pitch `p`. Total length `L = N·p`. Every module is au
  with its body toward local +Y and is one pitch long (§4.2 holds that to a whole multiple of `p`).

  ```
  slot i (0 = foremost), i ∈ [0, N-1]
  ```

  **Axial, fore-facing** (everything but the drive): the flange bolts to the slo
  body runs forward into the slot.

  ```
  ```

  **Axial, aft-facing** (the drive, and a stern pod): the flange bolts to the slot's forward face and
  the body runs aft.

  ```
  ```

  Both occupy exactly `[plane_aft(i), plane_fore(i)]`. A 12-slot chain spans `[−
  centred on the origin — which is what the flight model, the collar and the camera all assume.

  **Radial** (`starboard`, `port`, `dorsal`, `ventral`) — a pod on a slot's flank:

  ```
  pos = (0, centre(i), 0) + face_dir(f)·(half_width − recess)
  ```

  `half_width` is the design's, not the part's: the chain declares the flank pla
  (1.6 m for A and C, 2.4 m for B). `recess` is 0 under A1 and is kept in the struct because the keel
  family may come back.

  `q_face` and `face_dir` are already correct in `component.h:66-104` and their
  asserted by gate 1. **Do not touch them.** Only the position arm changes.

  Worked check, `N=12, p=4`: slot 0 fore → `pos.y = 24 − 4 = 20`, body `[20, 24]` ✓.
  Slot 11 aft → `pos.y = 24 − 44 = −20`, body runs to `−24` ✓. Slot 5 starboard
  `pos = (1.6, 24 − 22, 0) = (1.6, 2, 0)` ✓.

  ### 3.2 Thrust direction

  Unchanged from plan 07 §3.4 and it is still the easiest thing here to get backwards:

  ```
  thrust_dir = −(rot · (0,1,0))
  ```

  An aft-facing drive has `rot = rotZ(π)`, so `rot·(0,1,0) = (0,−1,0)` and `thrust_dir = (0,+1,0)`.
  A drive counts toward `derived_thrust` only when `dot(thrust_dir, +Y) ≥ cos(DR
  15° of the nose. A fore-mounted drive gives `−(0,1,0)` and contributes **nothing** — that is
  correct, not a bug, and the shipyard says so in words (§8.5).

  ### 3.3 Collider concatenation

  Part sidecars carry 2 D shapes in the model's XY plane (`ModelShape`, `gltf.h:
  placement:

  ```
  f     = rot · (0,1,0)
  yaw   = atan2(−f.x, f.y)                        // rotZ(θ) takes (0,1,0) → (−sinθ, cosθ)
  o     = (pos.x, pos.y) · design_scale
  for each shape s:
      c'      = R(yaw)·(s.pos · design_scale) + o
      angle'  = s.angle + yaw
      half'   = s.half · design_scale             // rotation lives in angle', n
      append {kind, c', angle', half', radius' = s.radius · design_scale}
  bounds_radius = max over shapes of |c'| + (box ? hypot(half'.x, half'.y) : rad
  ```

  Use the box's **circumradius** `hypot(hx, hy)`, not `|half|` summed componentwise: a 45°-canted box
  reaches `hypot` and a broad phase that rounds down drops contacts.

  **Dorsal/ventral pods**: `f = (0,0,±1)`, so `atan2(−0, 0) = 0`. That is fine a
  pod is above or below the play plane, its XY footprint is its own unrotated extent, and its Z offset
  is dropped. Guard the case explicitly (`|f.x| + |f.y| < 1e-6 → yaw = 0`) so no
  `atan2(0,0)`.

  `world.ship.bounds = collider_bounds(collider)` as today; `ShipSpec::length` is then
  `2 · bounds.halfLength`.

  ### 3.4 Derived figures

  Over live placements (`!destroyed`), with `m_k` the part's dry mass and `pos_k

  ```
  M_dry   = Σ m_k
  ```

  The torque arm is the **longitudinal** offset only, matching `component.cpp:142-152`: a jet's plume
  points outboard, so it pushes the hull along ±x, and the moment of an x-force
  `−y·Fx`. The x offset of the pod does not enter. This is the existing law, not a new one.

  ### 3.5 ΔV, TWR and the thermal margin

  One implementation, in `sim/component.h`, called by the HUD **and** the shipyard:

  ```
  mdot   = 14.0 kg/s                                 // step_ship's full-throttl
  isp_g0 = thrust / mdot                             // m/s; thrust ≤ 0 → 0
  Δv     = m_wet > m_dry · (1+1e-7) ? isp_g0 · ln(m_wet / m_dry) : 0
  TWR    = thrust / (m_wet · g)                      // g = μ/r² at the body; no body → no TWR, not 0
  ΔQ     = cooling_total − heat_load                 // heat_load: §8.4
  ```

  Kestrel: `isp_g0 = 1.6e6/14 = 114 286 m/s`, `Δv = 114 286 · ln(98/82) = 20.4 km/s`. The HUD prints
  20 m/s today because `scene.cpp:1010` uses `14000.0`. Fix the constant, move t
  shared helper, and have `make_hud_frame` call it. **Expect the flight HUD's ΔV readout to change by
  three orders of magnitude** — that is the fix landing, not a regression.

  Shipyard TWR uses `g₀ = 9.80665` and says so on the row (`TWR @ 1 g`), because
  no body to measure against and a TWR against nothing is a lie.

  ### 3.6 The RCS allocator

  `rcsJet` scales drawn cones and nothing else (A6). Generalise the four-corner sign test to a cluster
  at arbitrary `r = pos_k − com`:

  ```
  fx_sign  = r.x >  0.5 ? −1 : r.x < −0.5 ? +1 : 0      // outboard plume pushes inboard; 0 = centreline
  tor_sign = −sign(r.y) · fx_sign                       // the −y·Fx law; 0 when

  lat = (fx_sign  == 0) ? 1 : (lateral_demand · fx_sign  > 0 ? 1 : 0)
  tor = (tor_sign == 0) ? 1 : (torque_demand  · tor_sign > 0 ? 1 : 0)
  ```

  A centreline cluster (`|r.x| ≤ 0.5`) serves both signs: a quad has nozzles on both flanks.

  At the four ideal corners this reduces to `JET_THRUST_X` / `JET_TORQUE` exactly, which is what
  `shapes_tests.cpp:167-197` asserts — **those tests must keep passing untouched

  ### 3.7 Occupancy

  `OccupancyGrid` (`component.h:158`) is `array<bool,96>` = 16 stations × 6 faci
  indexing bakes in the old fore/aft virtual-station scheme (`component.cpp:196-230`). Under A1 it
  becomes, plainly:

  ```
  index(slot, facing) = slot · 6 + facing            slot ∈ [0, 15], facing ∈ [0, 5]
  axial part  (fore|aft) at slot i, span s  → cells (i..i+s−1, Fore)     // one
  radial part at slot i, span s             → cells (i..i+s−1, facing)
  ```

  Axial modules all share the **Fore** lane whichever way they face, because an
  fore-facing tank in the same slot are the same 4 m of hull. `can_mount` rejects on any cell already
  set, and on `slot + span > N` or `slot < 0`. Set intersection is exact: no tol
  test, no interpenetration possible.

  `N ≤ 16` is now a hard limit and it is the grid's. A design file asking for more is rejected at load
  with the file name and the count.

  ---
  [ ] P3  design format + loader      §7      headless, testable
  [ ] P4  runtime composition         §7.4    the ship is an assembly in flight
  [ ] P5  the 2 % reproduction test   §5.3    the kit proves it can express the game
  [ ] P6  shipyard rebuild            §8      full free assembly
  [ ] P7  UI vitality                 §9      the clock, the states, the meters
  [ ] P8  startup diorama             §10     the plate

  Each phase ends with `cmake --build build --config Release` clean at /W4 and
  `build/Release/Opra.exe --selftest` exiting 0. A phase that cannot end green d

  ---

  ## 5. Gates

  The acceptance list. Every one is a test or a named screenshot, never an opini

  | # | gate | where it runs |
  |---|---|---|
  | 1 | `mount_transform` returns §3.1's positions for slot 0, slot N−1 and a micomponent_tests.cpp` |
  | 2 | A chain's composed bounds equal `N·p` along Y, to 1 mm | `component_tests.cpp` |
  | 3 | Overlapping placements are rejected; the design is unmodified | existing
  | 4 | `mirror` on fore/aft is rejected; the shipyard greys the toggle | existing gate 3 + §8.6 |
  | 5 | **`derive_spec()` on each stock design reproduces its `SHIPS[]` row withdesign_tests.cpp` |
  | 6 | The four-corner RCS case reproduces `solve_rcs`'s table bit-for-bit | `shapes_tests.cpp` (unchanged) |
  | 7 | Every part sidecar carries a flange at −Y, radius 1.25, on the origin |
  | 8 | Every part is 120–400 own triangles with the new flange included | `artifacts/yard/partgate.mjs` |
  | 9 | Each stock design clears complexity ≥ 60, section CV ≥ 0.20, top materiates.mjs` |
  | 10 | Pairwise silhouette IoU < 0.70 across the three designs | `gates.mjs` |
  | 11 | A design with no drive refuses launch **with a reason line** | `flow_te
  | 12 | A 12-module ship blows up into 12 debris bodies and ≤ 72 particles; pool never overflows | `effects_tests.cpp` |
  | 13 | No debris body spawns inside the collider it left | `effects_tests.cpp`
  | 14 | Composed collider shape count ≤ 64 per ship; a design over it is rejected at load | `design_tests.cpp` |
  | 15 | The plate draws no instance in `InstanceLayer::Backdrop` from the orrerce dump |

  ### 5.3 The reproduction test (gate 5)

  New file `src/sim/design_tests.cpp`, registered in `CMakeLists.txt` and called
  `run_selftest()`. For each of the three designs it loads `assets/designs.json`, derives the spec,
  and prints a table even when it passes:

  ```
  Kestrel   mass 82000/82000 +0.00%   thrust 1.600/1.600 +0.00%   fuel 16000/16000 +0.00%
            torque 1.350/1.350 +0.00%   hull 100/100 +0.00%   cooling 0.0550/0.0
  ```

  Printing on success is the point: when someone reorders a slot in six months, the diff in this
  output is the first thing they see.

  ---
  ### 6.1 `flange()` at 156 triangles (A3)

  `tools/models/prims.ts:1156`. Same silhouette, same `FLANGE_RADIUS = 1.25`, sa
  node name `flange`:

  | element | now | new | tris |
  | | | | **920 → 180** |

  180, not 156 — the keys stay, because they are the only warm mark on the interface and they are how
  a loader reads the clocking. The budget in §5 gate 8 is the part's **own** geo
  the flange, i.e. ≤ 580 total, which `partgate.mjs` already reports both ways
  (`partgate.mjs:12-24`).

  Re-export every model afterwards (`bun tools/export.mjs --all`) and eyeball `d
  `station` and the three hulls in the model viewer (`--viewer`, or the `Viewer` screen): those are
  the assets where a flange is large on screen.

  ### 6.2 The modular feature floor

  `enforce_min_feature` (`export.mjs:195`) grows anything under 1.5 m. On a 4 m
  radiator plates, nav lights, hazard panels and — the one that matters — the flange's own 0.10 m ring
  plate. Plan 05 §3.2's floor is right for a 60 m hull at 3 px/m and wrong for a
  alone.

  ```js
  ```

  `meta.modular: true` goes in all 16 shipping module files. The 1.5 m floor sti
  **assembled designs** and to every non-modular asset, which is where it was always meant to bite.
  `audit_legibility`'s `feature_ok` check (`export.mjs:945`) takes the same floo
  not fail what the grower was told to leave alone.

  ### 6.3 Sidecar: the part block

  `exportModel` (`export.mjs:587`) gains one object in the sidecar, written **only** when
  `meta.kind` is present:

  ```json
  ```

  Masses and thrust are converted to kg and N **here** (§2.1) — `meta.mass * 1000`,
  `meta.thrust * 1000` — and nowhere else. The flange block is measured from the
  `meta`: find the node named `flange`, assert `|pos| < 1e-6` and the radius, and **fail the export**
  if it is missing (gate 7). A part that silently exports without a flange mount
  neighbour and the failure looks like a modelling mistake three days later.

  `collider`, `ports`, `effects`, `hardpoints`, `counts` are unchanged — the existing derivation
  already does the right thing per part.

  ### 6.4 Graduation
  cd artifacts/yard
  bun partgate.mjs parts/*.ts                      # gate 8, per part
  bun run gates.mjs designs/variant_a_hammerhead.ts # gates 9, 10, per design (×3)
  cp parts/*.ts ../../tools/models/                 # 16 modules; not spine_trus
  cd ../.. && bun tools/export.mjs --all
  ```

  `--all` rewrites `assets/models.json` by upsert (`export.mjs:1064-1078`), so t
  appended and the existing 33 keep their order and their `scale`. **Module entries carry
  `scale: 1`** — the 1.3 on the stock hulls is a legacy fudge for a model author
  modular part is authored in true metres.

  Then, in the engine: `Opra.exe --viewer` and step through the 16 new names. A part that loads,
  draws, and sits with its flange at the origin is a part that will mount.

  ---
  ```

  Optional per placement: `"roll": 0..3`, `"span": 1`, `"group": 0`. `facing` is
  `fore aft starboard port dorsal ventral`, lower case, and an unknown string is a **load error
  naming the file, the index and the string** — not a silent fallback to `fore`.

  ### 7.2 `SpineDef` becomes `ChainDef`

  Keep the struct, rename the fields that lie. `stations` → `slots`; drop `famil
  kept for the file's own record; `mass`, `heat_capacity`, `cooling` on the chain itself go to **zero**
  and stay there — under A1 there is no spine to weigh. Leaving them non-zero do

  `ShipDesign` (`component.h:165`) keeps `components` for the legacy path (the t
  `component_tests.cpp:54-180` still use it) and gains nothing else: `spine` and `placements` are
  already there.

  ### 7.3 Loading

  New `src/sim/designs.cpp/.h` (sim layer: it owns `ShipDesign`, and it must not

  ```cpp
  /** Every design assets/designs.json names, by name. A malformed file is fatal, like the manifest. */
  class DesignStore {
  public:
      void load(const std::string &path);
      const ShipDesign &design(const std::string &name) const;   // fatal on miss, like ModelStore
      const std::vector<std::string> &names() const;
  };
  ```

  The part stats come from the **model sidecars**, which live in `render/`. The
  (`render/` may not include `sim/`) is satisfied the way the collider already is: `game/app.cpp` is
  the one module that knows both, and it fills a plain `PartSpec` table the sim

  ```cpp
  // sim/component.h — layer-neutral, mirrors the sidecar's "part" block.
  struct PartSpec {
      Real dry_mass = 0, propellant = 0, thrust = 0, cooling = 0, heat_capacity = 0;
      Real rcs_authority = 0;
      int rcs_jets = 0, span = 1;
      bool axial = true;
  ```

  `App::build_part_table()` walks `models.store.names()`, reads each `ModelMeta:
  once after `models.build()` and again after every hot reload. `derive_spec(design, parts)` takes it
  by const reference. A placement naming a part the table does not have is a **l
  every missing name at once** — one message, not one per part, so a stale designs.json is one fix and
  not sixteen runs.

  ### 7.4 Drawing (`game/scene.cpp:320`)

  One helper, used by flight, the shipyard and the diorama — three callers, one
  chance of the yard showing a ship the flight view does not:

  ```cpp
  /** Places every live placement of a design. `lod` is chosen once for the whol
  void add_design(SceneBuilder &scene, const ModelSet &models, const ShipDesign &design,
                  const glm::vec3 &origin, const glm::quat &rotation, float scal
                  float thrust, const std::vector<Real> &jets);
  ```

  ```

  `glm::vec4(a)` — all four cone slots of one module get that module's own authority, because the
  cones in one module are one cluster. `MeshPart::jet` still resolves the corner
  position (`gltf.cpp:261`) and that still works: it just indexes four equal numbers now.

  **LOD is selected once per ship**, from `bounds.halfLength`, exactly as today
  (`scene.cpp:323`). Parts must not pop independently — a hull that sheds pieces
  bug this rule exists to prevent.

  ```
  draws_at(p, lod):
    lod >= 3   every placement
    lod == 2   axial modules + radial pods with span >= 1        (rcs/sensor/lam
    lod == 1   axial modules only
    lod == 0   nothing; the overlay's chevron owns it
  ```
  ### 7.5 The sim side

  - `ShipState::rcsJet` becomes `std::vector<Real>`, sized to `design.placements.size()` for a modular
    ship and to 4 for the table path. `solve_rcs` takes the per-placement arms (
  - `World` gains `ShipDesign design;` and `ShipSpec derived;`. `ship.spec = &world.derived`.
  - **`ShipSpec::name` is a `const char *` aliasing `design.name.c_str()`.** Two
    real crashes if missed: (a) the design must outlive the spec — it does, both are World members;
    (b) `world = World{}` in `App::reset_run` (`app.cpp:237`) leaves `ship.spec`
    **old** World. After every assignment to `world`, call `world.rebuild_from_design(parts)` which
    re-derives the spec, re-points `ship.spec`, re-composes the collider and re-
    for `world = World{` before declaring this done; there is one today and there must be no second
    one that skips the call.
  - `App::sync_ship_collider` (`app.cpp:143`) composes from placements (§3.3) when
    `!design.placements.empty()`, and keeps its current single-sidecar path othe
    from every placement's `pdc.*` hardpoints through the same transform.
  - `App::sync_ports` concatenates: ports in placement order, local ids renamed
    kept on the design so a dock target saved before a refit survives one. The World uses the first
    port; the aft-face fallback (`app.cpp:196`) stays for a design that declares

  ---


  ```
  ┌──────────────────────────┬────────────────────────────────────┬─────────────────────────┐
  │ CATALOGUE          260px │        TURNTABLE  (3D)             │ DERIVED
  │ ───────────────────────  │                                    │ ─────────────────────── │
  │ ▸ command / nose         │              ▲ slot 0              │ DRY MASS
  │ ▸ hull sections          │           ╔══╗                     │ PROPELLANT    16.0 t    │
  │ ▸ propellant             │       ○───╢  ╟───○   slot 5 pods   │ ALL-UP
  │ ▸ utility / thermal      │           ╠══╣                     │                         │
  │ ▸ propulsion             │           ║  ║                     │ THRUST
  │ ▸ pods                   │           ╚══╝                     │ TWR @ 1g       1.66     │
  │                          │              ▼ slot 11 (drive)     │ ΔV
  │   nose_hammerhead   7.5t │                                    │ TORQUE     1.35 rad/s²  │
  │   nose_pushbow     12.0t │  drag orbit · wheel zoom           │ HULL
  │ ▸ nose_stealth      2.5t │  click a ring to place             │ THERMAL      +2 kW      │
  │                          │  right-click a module to remove    │
  │ [x] mirror radial        │  1-4 camera presets                │ ✓ drive installed       │
  │ [x] show flanges         │                                    │ ✓ TWR over 1
  │                          │                                    │ ✗ thrust line off 0.8 m │
  ├──────────────────────────┴────────────────────────────────────┴─────────────
  │  [ < back ]                                            [ launch — 1 fault ]             │
  └─────────────────────────────────────────────────────────────────────────────
  ```

  ### 8.1 State

  `ui::ShipyardState` (`screens.h:75`) is replaced. The current one holds a hull name and a bag of
  `ShipyardPart` positions and cannot express a chain at all.

  ```cpp
  struct ShipyardState {
      ShipDesign design;            // the thing being edited; the ship that lau
      int category = 0;             // catalogue tab
      int held = -1;                // index into the current tab's item list, −
      int hovered_slot = -1;        // slot under the pointer, −1 = none
      Facing hovered_facing = Facing::Fore;
      int selected = -1;            // placement index selected for inspect/remove
      bool mirror = true;
      bool show_flanges = true;
      // turntable

  `held_part`, `hull` and `parts` are deleted. `design_has_drive()` forwards to
  `opra::design_has_drive(design)` — the real one — instead of returning a stored bool.

  ### 8.2 Turntable


  ```
  k = 1 − exp(−dt / 0.09)                       // 90 ms time constant, framerat
  yaw      += (yaw_target − yaw) · k            // yaw wraps; damp the shortest way around
  pitch    += (pitch_target − pitch) · k        // clamp target to [−20°, +80°]
  distance += (distance_target − distance) · k  // clamp target to [0.35, 3.0]
  ```

  Never damp toward a target that is itself being damped — the drag writes `*_ta
  writes the current. `shipyard_camera` (`screens.cpp:565`) keeps its shape and takes the framing from
  the **design's** composed bounds rather than a hull model's AABB:

  ```
  ```

  Camera presets on `1`–`4`: bow ¾ (`yaw 0.6, pitch 0.35`), plan (`0, 1.396`), s
  (`1.571, 0.10`), stern ¾ (`3.74, 0.35`). They write the targets, so the camera swings; it does not
  cut.

  ### 8.3 Picking a slot — no raycast

  Project, do not intersect. For each slot `i` and each offered facing, take the
  `mount_transform`, run it through `view_projection(camera)`, divide by w, map NDC to pixels, and keep
  the nearest candidate inside 26 px of the pointer.

  ```
  if (clip.w <= 0) skip                                     // behind the eye
  px = ((clip.x/clip.w) · 0.5 + 0.5) · width
  py = (1 − ((clip.y/clip.w) · 0.5 + 0.5)) · height         // the UI batch is y
  ```

  O(slots × facings) = 96 projections at the very worst, once a frame, against the alternative of a
  ray/cylinder intersection nobody will debug at 3 a.m. When a part is held, onl
  offered; when nothing is held, only **occupied** ones (so a click selects the module for removal).

  Ghost rings: at each offered cell draw a ring of 24 segments, radius 1.25 (the flange), in
  `NAV` at α 0.35, and the hovered one in `DRIVE` at α 0.9. When the held part *
  (`can_mount` false), the ring is `THREAT` and the click is refused with a one-line reason instead of
  a silent no-op.

  ### 8.4 The derived panel

  Recomputed on any mutation — placement, removal, mirror toggle — never per fra
  nothing changed. Cache the `ShipSpec` in the state and dirty it on edit.

  Rows, in order, with the unit on every one:
  ALL-UP        t        (M_dry + M_fuel) / 1000
  THRUST        kN       thrust / 1000
  TWR @ 1g      —        thrust / ((M_dry + M_fuel) · 9.80665)      < 1.0 → THRE
  ΔV            km/s     §3.5                                        0 → dash, not 0.00
  TORQUE        rad/s²   derived_torque
  HULL          —        derived_hull
  THERMAL       kW       (cooling · HEAT_SCALE) − heat_load          < 0 → THREA
  ```

  `heat_load` is the drives' own number: `Σ thrust_k · 1.2e-5` kW, i.e. 19.2 kW for a 1600 kN torch,
  so a two-radiator Kestrel (2 × 14 kW = 28 kW nominal) sits at +8.8 kW and a dr
  zero. `HEAT_SCALE = 350 kW` per unit of `ShipSpec::cooling` — chosen so 0.055 reads as 19.3 kW,
  which is the same order as the load it cancels. This is a **display model only
  equation is untouched.

  Hover delta: while the pointer is over a catalogue item, every row that would change prints
  `old → new` with the new value in `DRIVE` when it improves and `THREAT` when i
  not animate this one — a number that is moving while you read it is worse than one that is not.

  ### 8.5 Launch gate

  `launch` is enabled only when every check passes, and each failed check gets its own line under the
  button. One line, present tense, naming the fix:

  | check | fail line |
  |---|---|
  | at least one drive inside `DRIVE_ARC` | `no forward drive: the chain needs a
  | `TWR @ 1g > 0` | `no thrust` |
  | `|com.x| ≤ 0.6 m` | `thrust line off centre by 0.84 m — mirror the port pod`
  | thermal margin ≥ 0 | `heat load exceeds cooling by 6 kW — add a radiator section` |
  | at least one tank | `no propellant` |

  The COM check is on **x only**: a fore/aft imbalance is trim, a lateral one is
  all run. Mirrored pairs make it zero by construction, which is what the mirror toggle is for.

  ### 8.6 Mirror

  `mount_placement(design, p, mirror)` already implements plan 07 §3.6 including the fore/aft
  rejection (`component.cpp:246`). The shipyard's job is to **grey the toggle**
  axial rather than let the click fail — gate 4. Removing either half of a `group` removes both;
  damage in flight does not propagate (the group is an editor concept and nothin

  ---

  ## 9. P7 — vitality

  The screens are inert because the UI has no clock. That is the whole diagnosis
  downstream of one parameter.

  ### 9.1 The clock

  ```cpp
  void Context::begin(UIBatch &batch, const glm::vec2 &screen, const Pointer &po
                      const Nav &nav, double now);      // ui.h:45
  double time() const { return now_; }
  ```

  Every call site passes `app.now`. There are five (`frame.cpp` Startup, Contract, Shipyard, Viewer
  and the flight/overlay path). No other state is added: a pulse that is a pure
  no memory, survives a pause, and screenshots reproducibly at a given clock.

  ### 9.2 Interaction states

  In `Context::button` / `row` / `toggle` (`ui.cpp:150-215`), which is where every screen inherits
  them at once:

  ```
  rest      ink ETCH_DIM (α 0.55)     rule α 0.28
  hover     ink ETCH     (α 1.00)     rule α 0.85   + 6 px corner ticks, 1 px, a
  active    ink DRIVE                 rule DRIVE    + a 2 px left bar
  focus     unchanged (the NAV bar at x−14 stays; keyboard focus is not hover)
  ```

  The press flash: `flash = clamp01(1 − (now − pressed_at) / 0.06)` on the widget that fired, mixed
  into the ink with `lerp_color`. One float on the Context (`last_fired_`, `last
  per widget.

  ### 9.3 Ambient
  ```
  α(t) = α₀ · (0.96 + 0.04 · sin(2.4 · t))         borders of a live panel
  scanline: 1 px rules every 3 px at α 0.03 over FIELD-backed panels, static, not scrolling
  ```

  A scrolling scanline is a screensaver. A static ruling is a material. The plan

  Meters: `draw_meter(UIBatch&, Rect, float value, float threshold, glm::vec4 in
  a segmented bar, 8 segments, the threshold as a 1 px tick, the fill eased toward its target at the
  same 90 ms constant as the turntable. One function; the fuel gauge, the TWR ba
  margin all call it.

  Toasts (`screens.cpp:247`): fade the last 0.4 s with `α = clamp01((until − now) / 0.4)` and prefix a
  glyph by kind (`·` info, `!` warning). Nothing else changes.

  ---

  `orrery::build` (`orrery.cpp:116`) emits every ring segment as a unit cube in
  `InstanceLayer::Backdrop`, which is the **additive, unlit, no-depth** pass. 192 segments per ring,
  overlapping 6 % at every joint, times every body, summed additively, is the wh
  looks straight down (`MAP_PITCH` = 89.5°) at a line drawing with no depth cue and the title plate
  floats over the corner of it.

  ### 10.2 The diorama

  The plate becomes a scene, built in `frame.cpp`'s Startup branch beside the ex

  ```
  camera:  pitch 28° (0.4887 rad), yaw 0.55, half_height 95 m, target = the berth
           slow drift: yaw += dt · 0.012, wrapping — no input needed for the fra
  subject: the player's design at the station's port A, composed by add_design() at lod 3
  station: models.store.model("station"), 213 m across, spun by 0.02·t
  light:   scene.light.direction_to_star = normalize(−0.45, 0.25, 0.35), colour #ff9c5c, intensity 1
  sky:     add_backdrop(scene, backdrop, models, camera, now, 260.0f)   — the re
  star:    one SkyBody at 6 km along the light direction, radius 180, BodyKind::Star
  motes:   the backdrop's own `motes` layer is already the dust; do not add a se
  ```

  The ship sits at the station's port A with its nose along the port normal, `1.5 · bounds.halfLength`
  out along it, so the berth reads as a berth and the hull does not intersect th
  move: a docked ship is docked.

  `half_height` 95 m frames a 48 m hull at just under half the frame height with the station arm
  crossing behind it. Check it against the title plate's 400 px column — the hul
  the wordmark; shift `camera.target` right by `2·(0.5 − 0.62)·half_height·aspect` the same way
  `map_camera` does (`orrery.cpp:230`).

  ### 10.3 The ephemeris, as ink

  The orbits come back as **2 D chart ink in the UI batch**, behind the title bl
  scene and before the plate:

  ```cpp
  void build_ephemeris(UIBatch &batch, const orrery::Frame &frame, const Rect &a
  ```

  - `orrery::sample_ring(body.elements, 96, points)` — the existing pure sampler, in double.
  - Scale: `px_per_m = min(at.w, at.h) · 0.42 / field_radius(frame)`; centre on
  - Draw each ring as a **dashed** polyline: `push_line` on every other segment, 1 px,
    `tokens::VELLUM_RULE` (α 0.28) × `alpha`.
  - One tick per 90°, 4 px, same ink. Not twelve — at this size twelve ticks is a dotted ring.
  - The star: `push_disc` r 3 px in `DRIVE`, plus one `push_arc` at r 7 px α 0.2
    there is no additive pass involved any more.
  - The current body carries its name at 10 px `VELLUM_DIM`, right of its dot, a

  `orrery::build` is then called from nowhere. **Delete nothing** — `sample_ring
  `field_radius`, `glyph_radius` and `map_camera` are all still used, by this function and by
  `orrery_tests.cpp`. Gate 15 asserts the plate emits no orrery backdrop instanc

  ### 10.4 Typography

  ```
  O P R A                                   DISPLAY_TITLE 50 px, VELLUM, letterspaced by hand
  ─────────────────────────────────────     rule, VELLUM at RULE (0.28), 340 px
  NEREID RECOVERY SERVICE                   LABEL 13 px, VELLUM_DIM, caps
  Wayfarer Station · Kestrel-class licence, third renewal
  ```

  Four rows, not five: `Screen::Shipyard` is reached through the contract (`MENU_FLOW`,
  `flow.cpp`), and a title row that jumps the contract would need a new edge in
  contract-less launch path. Not in this plan.

  The plate panel keeps its `PLATE` fill at α 0.66 but gains a 1 px left rule in `VELLUM_RULE` — the
  diorama behind it is busier than the orrery was, and type over a lit hull need

  ---

  ## 11. P9 — wreckage

  Plan 07 §8 is unchanged and is the specification; it is listed here only for t
  the two numbers that need restating:

  ```
  r     = pos_part − com                 |r| < 0.1 → n̂ = (1,0,0), deterministic,
  s     = clamp(0.6 · sqrt(E_blast / m_total), 4, 60)   m/s
  v_sep = v_ship + ω_ship × r + n̂ · s
  ω     = ω_ship + (hash01(index)·2 − 1) · 1.5          rad/s      game/effects.h:162, no RNG call
  spawn = pos_part + n̂ · part.bounds_radius
  ```

  Budget: one 24-grain central puff plus 4 grains per part; raise `POOL_MAX` 192 → 256
  (`effects.h:85`). Debris cap 48, oldest retires. Gates 12 and 13.

  A 12-module ship is the test case because it is the Kestrel minus its pods, an
  pool budget was sized against.

  ---
  Only once gate 5 is green:

  - delete `tools/models/{kestrel,mule,needle,kestrel_blocky,mule_blocky,needle_blocky}.ts`
  - delete the six `.glb` + six `.json` in `assets/`, and their `models.json` en
  - `SHIP_MODEL_NAMES` (`gltf.h`) loses its meaning; `ShipClass` becomes the key into
    `DesignStore`, and `create_ship` takes a design name
  - raise the exporter's ship complexity floor to 60 and fold section CV and top material into
    `--audit` (plan 07 P9)

  `HULL_BOXES` stays as the fallback for a ship created with no design (the self
  and `SHIPS[]` stays as the reference gate 5 measures against. Deleting either is a later plan's
  business.

  ---
  ## 13. Edge cases

  The list is the deliverable. Each row is a real failure mode found by reading
  hypothetical.

  | # | case | handling |
  |---|---|---|
  | 1 | `world = World{}` leaves `ship.spec` dangling into the old World | `rebuild_from_design()` after **every** assignment to `world`; grep before closing P4 |
  | 2 | `ShipSpec::name` aliases `design.name.c_str()` | the design outlives the); never rebind `design.name` while flying |
  | 3 | A design names a part the manifest lacks | collect **all** missing names, fail at load with one message; `ModelStore::model()` is fatal and would otherwise die on the first |
  | 4 | A design declares `slots > 16` | rejected at load with the file, the des |
  | 5 | Two docking collars both export port `"A"` | renamed `A, B, C…` in placement order; the local→global map is kept on the design so a saved dock target survives a refit |
  | 6 | Dorsal/ventral pod: `atan2(−f.x, f.y)` with `f = (0,0,±1)` | guarded `|fXY footprint unrotated, Z dropped (§3.3) |
  | 7 | A design with no drive | `derived_thrust = 0`, `maxAcceleration = 0`; launch refused with a reason (gate 11). Nothing divides by thrust: `burnSeconds` already guards |
  | 8 | A design with no tank | `fuel = 0` → `canBurn` false → the ship is inertefused |
  | 9 | `M_dry ≤ 0` (every module destroyed) | `centre_of_mass` returns the origin (already does, `component.cpp:118`); the ship is debris by then |
  | 10 | Empty tanks: `ln(m_wet/m_dry)` with `m_wet == m_dry` | guarded by `m_wets a dash, not `-inf` |
  | 11 | Asymmetric pods shift COM off the thrust line | shipyard warns over 0.6 m in x; in flight the RCS sign test already leans on the right cluster |
  | 12 | Hot reload (`F5`) while a design is loaded | `reload_models_if_stale` →derive the spec → re-compose the collider → re-size `rcsJet`. Mesh ids aresafe: the whole library is rebuilt and re-uploaded (`app.cpp:257`) |
  | 13 | A part's sidecar has no `part` block (a prop, a station) | absent from ng it fails case 3's check |
  | 14 | Composed collider explodes in shape count | 16 modules × 4 shapes = 64. Cap at 64, reject at load (gate 14); the MTV solver is O(shapes) per contact |
  | 15 | Plan 05's single-MTV rule | unchanged: resolve against **one** shape, n compound. Concatenation does not change collision response |
  | 16 | Two modules in one slot, one fore one aft | the Fore lane is shared by all axial parts (§3.7), so this is rejected — they are the same 4 m of hull |
  | 17 | `mirror` on an axial part | rejected by `mount_placement`; the shipyarde click never happens (gate 4) |
  | 18 | Removing one of a mirrored pair in the yard | removes both (editor convenience). In flight, damage does **not** propagate — losing the port PDC must not lose the starboard |
  | 19 | Blast while docked | offset the spawn by the **station's** bounds radiude the ring |
  | 20 | Blast during warp | force warp to 1× — plan 05 §2.7 already does this for rounds in flight |
  | 21 | Ship at icon LOD when it blows up | spawn the debris anyway; it is simu not draw it |
  | 22 | Turntable pitch at exactly ±90° | clamp to [−20°, +80°]; at the pole the view basis degenerates the same way `MAP_PITCH` documents |
  | 23 | Slot pick with the camera behind the ship | `clip.w <= 0` skips the can is not pickable |
  | 24 | Catalogue item hovered with no design loaded | delta rows print dashes; no division by a zero mass |
  | 25 | `ui::Context::begin` called without a clock | there is no overload withl sites in the same commit or it will not compile, which is the intent |
  | 26 | The plate's diorama before models finish loading | the plate is only reachable after `App::init` completes `models.build()`; if a part is missing the store is already fatal at
  that point |
  | 27 | ΔV readout changes by 1000× after §3.5 | expected. Update any golden screenshot or captured HUD text in `tools/golden/` in the same commit |
  | 28 | A module authored longer than one pitch | gate 8's axial-length rule (w in the sidecar must match or the occupancy is wrong |

  ---

  ## 14. File map

  What each phase touches. Anything not listed here should not appear in the dif

  | phase | files |
  |---|---|
  | P1 | `tools/models/prims.ts` (flange), `tools/export.mjs` (modular floor, `p
  | P2 | `artifacts/yard/parts/*.ts` (stat table, `modular: true`), `tools/models/*.ts` (16 new), `assets/*` |
  | P3 | `src/sim/component.h/.cpp` (chain transform, occupancy, `PartSpec`, der.cpp` (new), `assets/designs.json` (new), `CMakeLists.txt` |
  | P4 | `src/game/scene.cpp` (`add_design`, ship draw, ΔV), `src/game/app.cpp/.h` (part table, collider, ports, rebuild), `src/sim/physics.h/.cpp` (`rcsJet` vector, `solve_rcs`),
  `src/sim/world.h/.cpp` |
  | P5 | `src/sim/design_tests.cpp/.h` (new), `src/selftest.cpp`, `CMakeLists.txt` |
  | P6 | `src/ui/screens.h/.cpp` (shipyard), `src/game/frame.cpp` (input, dispat
  | P7 | `src/ui/ui.h/.cpp` (clock, states), `src/ui/draw.h/.cpp` (`draw_meter`), `src/game/frame.cpp` (5 `begin` call sites), `src/ui/screens.cpp` (toasts) |
  | P8 | `src/ui/title.h/.cpp`, `src/ui/screens.h/.cpp` (`build_ephemeris`), `sranch), `src/game/app.cpp` (`active_camera`) |
  | P9 | `src/game/effects.h/.cpp`, `src/sim/world.cpp`, `src/game/effects_tests.cpp` |
  | P10 | deletions per §12 |

  ---
  Stated so the next session does not read the absence as an oversight.

  - **No blocky LODs for modules** (A7). `lod_model` falls back; add them when a
  - **No keel or monocoque families.** A1 removed the visible spine; the `family` field survives as a
    half-width parameter and nothing more. Plan 07 §2's eight spines are not aut
  - **No NNLS or pseudo-inverse RCS allocator** (A6). `rcsJet` scales cones.
  - **No per-module damage model.** `thrustDamage` and `fuelLeak` stay ship-wide
    integrity arrives with wreckage in P9 and stops at detach.
  - **No save format.** A run is a session (plan 06 L1). The design a player bui
    run ends.
  - **No procedural design generation.** Three stock designs plus whatever the p
  - **`SHIPS[]` and `HULL_BOXES` are not deleted.** They are the reference gate 5 measures against and
    the fallback for a design-less ship.
