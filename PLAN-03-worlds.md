> **Status: complete.** The live plan is [PLAN-05-legibility.md](PLAN-05-legibility.md).

Live plan. [PLAN-01](PLAN.md) and [PLAN-02](PLAN-02-system.md) are complete; their decisions bind.

Plan 02 gave the system a real body: Kepler on rails, patched conics, SOI, warp, a belt, docking
gates, compound colliders, the orrery. This plan makes the system somewhere you *do* things —
orbit it, assist off it, land on it, build on it — and rebuilds the instrument you do it through.

---

## 0. Review of plan 02

**Implemented and verified.** 15,042 lines, 1077 selftest checks, 0 failures, clean Release build.
I spot-checked the defects that mattered most and they are fixed correctly, not papered over:

| Was | Now |
|---|---|
| D-1 cutter 90° off | `physics.cpp:147` `cutter_beam` measures from `angle + π/2`, with the vector convention written down in the comment above it. |
| D-2 sim scaled by refresh rate | `loop.cpp:87` passes real `elapsed`. |
| D-3 upload raced the in-flight frame | `gpu.cpp:78` cycles the destination. |
| D-7 `active_` latched forever | `ui.cpp:33` releases on any pointer-up, non-release frame. |
| RCS never drawn | `scene.cpp:139` feeds four per-jet authorities from the torque solution into the effect transforms. |

One submit per frame in the live loop. The orrery and the startup plate are the best things in
the build.

### What is wrong with it

| ID | Site | Fault |
|---|---|---|
| **R-1** | `hud.cpp:139, 298` | D-18 is half-fixed. The safe inset exists, but the bottom-left cluster stacks wrong: bars land at y = 778 / 812 / 846 and the ship name at `bar_bottom + 6` = 852 — six pixels below the *top* of the "heat" label. "heat" and "Kestrel" overlap. Shift the cluster up ~26 px; a bigger margin will not fix a stacking bug. |
| **R-2** | `belt.cpp:54` | Cluster rocks are `8 + bias² · 78` m radius. An 86 m rock is 172 m across against an 80 m ship — 307 px of a 900 px frame. Under perspective the near ones loom and the field reads as rubble. Cut the top of the range and the density together; the size distribution wants a long tail, not a fat one. |
| **R-3** | `render/backdrop.cpp` | The nebula is brighter and more saturated than the gameplay layer. It competes with the hull and the collar for the eye. It should be the quietest thing on screen. |
| **R-4** | everywhere | The warm/cool material split from plan 02 §4.1 never landed. Map, startup and HUD all read the same cool white, so "two instruments, one darkness" is doing no work. `vellum` is defined in `tokens.h` and barely used. |
| **R-5** | `ui/map.cpp` | The map's header reads `nu`. It is `ν`. The text engine is UTF-8 clean — use it. |
| **R-6** | `ui/title.cpp` | Five identical full-width rules under five menu rows. That is decoration repeating, not structure encoding anything. One rule under the title block is enough; the rows are already a list. |
| **R-7** | repo root | 20 stray `.obj` files from a loose compile are committed at the top level. Delete, and add `*.obj` to `.gitignore`. |
| **R-8** | `ui/map.cpp` | The map labels a transfer (`Nereid -> Halberd`, `window opens in 115 d 13 h`) but draws no transfer conic. The arc is the whole point of the screen. |

R-1 through R-8 are **G0**, and G0 gates everything else.

---

## 1. Decisions

| # | Decision | Consequence |
|---|---|---|
| **F1** | **Camera: fixed pitch, soft 2D follow.** No pitch control during flight — the camera stays at one angle above the plane and moves only *in* that plane. Deadzone + velocity lead + critically-damped smoothing. | Ctrl+mouse pitch is removed from flight. Pitch becomes a settings value. The ship stops being welded to screen centre. |
| **F2** | **Planet verbs, all of them**: orbit / capture / gravity assist; orbital stations, docking, transfer contracts; low passes, scanning, satellite deployment; atmosphere with aerobraking and reentry heating; Lagrange stations; landings; surface bases. | This is three plans' worth of work. §6 phases it so each lands playable on its own. |
| **F3** | **Landing: powered descent to pads** against a 2D radial terrain heightfield, with a touchdown gate. No surface scene, no walking. | Terrain is procedural, not an asset. Reuses the compound collider and the docking gate machinery. |
| **F4** | **Lagrange: computed points, stations co-rotating on rails.** | Patched conics cannot hold an L-point orbit; §3.5 states the cheat plainly and marks it. |
| **F5** | **Minimap: context-driven, toggleable**, showing local contacts or orbital data depending on the task. | Lives in the HUD's layered density system (F10), not as a separate always-on panel. |
| **F6** | **Graphics: all four tracks.** Star as a real light with soft shadows; HDR + bloom + tonemap + grade; PBR from glTF with normal maps; dedicated planet and star shaders. | Requires a new vertex format (tangents + UVs) and a multi-pass frame. §4. |
| **F7** | **Testing: all four layers.** Headless screen-graph assertions, golden-image diffs, scripted input tapes, and a live layout-assertion pass. | §5. The layout pass would have caught R-1 on the frame it was written. |
| **F8** | **Assets: mid-poly geometry with procedural material maps**, both authored as code in the same three.js module and baked by the exporter. Not the current flat low-poly; not hand-painted PBR sets. | §7 is the asset brief. |
| **F9** | **Flight director cues only. The pilot always flies.** Programs compute the solution and draw it; they never write to `FlightInput`. | Every dock, every landing, every burn stays a skill. This also keeps the sim free of an autopilot special case. |
| **F10** | **Layered HUD density**, three levels on a key, with context override. | §4.6. |

Carried and still binding: `sim` is `double`; world positions never live in a `float` outside
`render/scene.cpp`; `orbit/` depends on nothing but `<cmath>` and glm; one-way module dependencies;
every phase ends with a clean `/W4` Release build and `--selftest` exit 0.

---

## 2. Architecture additions

```
assets/
  systems/nereid.json          + atmospheres, terrain seeds, pads, L-point stations
  textures/                    baked procedural maps (from the exporter, committed)
src/
  orbit/
    lagrange.h/.cpp            NEW  L1..L5 from the mass ratio
    encounter.h/.cpp           NEW  SOI entry/exit prediction, assist preview
  sim/
    terrain.h/.cpp             NEW  radial heightfield, sampling, pads, surface collision
    atmosphere.h/.cpp          NEW  density model, drag, stagnation heating
    descent.h/.cpp             NEW  touchdown gate, landed state, hoverslam solution
    survey.h/.cpp              NEW  scan passes, satellite deployment and orbit checks
    program.h/.cpp             NEW  the director: programs, steps, cues. Reads the world, writes nothing.
  render/
    shadow.h/.cpp              NEW  one orthographic cascade from the star
    post.h/.cpp                NEW  bloom chain, tonemap, grade
    planet.h/.cpp              NEW  planet and star draw, LOD by angular size
    material.h/.cpp            NEW  PBR material table, texture binding
  ui/
    minimap.h/.cpp             NEW  contact radar and orbital strip
    director.h/.cpp            NEW  flight-director cross, gate blocks, program panel
    blocks.h/.cpp              NEW  the HUD's composable information blocks
    flow.h/.cpp                NEW  the screen graph, as data. Testable headless.
  game/
    camera_follow.h/.cpp       NEW  deadzone, lead, critical damping
shaders/
  mesh.hlsl        extended: PBR, normal map, shadow lookup
  shadow.hlsl      NEW  depth only
  planet.hlsl      NEW  terminator, atmosphere rim, cloud band, night side
  star.hlsl        NEW  limb darkening, corona
  bloom.hlsl       NEW  threshold, downsample, upsample
  tonemap.hlsl     NEW  ACES + grade
  ui.hlsl          unchanged
```

**New rule:** `sim/program.cpp` may read the whole world and must write **nothing**. A director
that mutates state is an autopilot, and F9 says there is no autopilot. Enforced by making every
`evaluate` function take `const World&`.

---

## 3. The math

### 3.1 Camera follow — `game/camera_follow.cpp`

```
lead     = clamp(ship.velocity · LEAD_SECONDS, 0, LEAD_MAX)
goal     = ship.position + lead
offset   = goal - target
pull     = |offset| <= DEADZONE ? 0 : offset · (1 - DEADZONE/|offset|)
goal'    = target + pull
```

Then critically-damped smoothing, in the numerically stable form that is correct at any `dt`
(a naive `lerp(a, b, dt·k)` is frame-rate dependent and will reintroduce D-2 in the camera):

```
omega  = 2 / TAU
x      = omega · dt
decay  = 1 / (1 + x + 0.48 x² + 0.235 x³)
change = target - goal'
temp   = (velocity + omega · change) · dt
velocity = (velocity - omega · temp) · decay
target   = goal' + (change + temp) · decay
```

Defaults: `LEAD_SECONDS 0.6`, `LEAD_MAX 120 m`, `DEADZONE 48 m`, `TAU 0.35 s`. All in `config.h`;
all four want tuning against the real thing, so leave the knobs.

Pitch is read once from settings and never changes during flight (**F1**). Delete the Ctrl+mouse
pitch binding from the flight path; keep it in the model viewer where inspecting from an angle is
the point.

**The collar assumes the ship is at screen centre.** With a follow camera it is not. `HudFrame`
already carries `shipScreen` — every consumer must use it rather than `screen * 0.5`. Grep for
`0.5f` in `hud.cpp` before calling this done.

### 3.2 Terrain — `sim/terrain.cpp`

A body's surface is one radial heightfield: `N = 4096` samples of `h(θ)` around the
circumference, generated from the system file's seed by summed octaves.

```
r_surface(θ) = R_body + h(θ)
h(θ)         = Σ_{o=0..5}  A · g^o · noise(θ · 2^o · f + phase_o)     A = R_body · 0.004
```

Sampling is linear interpolation between the two bracketing samples; the derivative gives the
local slope, which the touchdown gate needs. 4096 samples over a 6.3e6 m body is a ~9.7 km
spacing — fine for a 2D silhouette, and a pad is a forced-flat span, not a sampled feature.

**Pads** are declared in the system file as `{ theta, width, name }`. Generation forces `h` constant
across the span and tapers into the neighbours over one sample either side, so no pad sits on a
cliff.

**Collision**: convert the ship to polar about the body, take the bracketing samples, build the
segment between them, and run the **existing** compound collider against it as a box of zero
thickness. Nothing new in the narrow phase.

### 3.3 Atmosphere — `sim/atmosphere.cpp`

Exponential, one scale height per body:

```
ρ(h)   = ρ0 · exp(-h / H)                       h = |r| - R_body, clamped at 0
a_drag = -½ ρ |v|² (Cd·A / m) · v̂
```

Stagnation-point heating, Sutton–Graves:

```
q = k · sqrt(ρ / R_nose) · |v|³                 k = 1.7415e-4  (SI)
```

`q` drives the existing `heat` gauge against the existing `cooling`, so reentry and a hard burn
compete for the same budget — which is the reason the heat gauge exists and has never mattered.

Aerobraking is not a feature, it is a consequence: a periapsis inside the atmosphere sheds energy
and drops apoapsis. What the plan owes it is the **prediction** — the map must draw the predicted
post-pass conic, or aerobraking is guesswork. Integrate the pass forward at coarse steps when the
predicted periapsis is below the atmosphere's top, and draw the result.

**Drag breaks the rails.** A ship inside the atmosphere is not on a conic. Warp must clamp to 1×
below the atmosphere's top altitude, alongside the existing auto-drop rules.

### 3.4 Powered descent — `sim/descent.cpp`

Hoverslam / suicide-burn altitude, for the director cue:

```
a_net   = a_max - g_local
h_burn  = v_down² / (2 a_net)
t_burn  = v_down / a_net
```

Touchdown gate, against the pad:

| Quantity | Limit |
|---|---|
| vertical speed | < 4.0 m/s |
| lateral speed | < 1.5 m/s |
| tilt from local up | < 8° |
| distance from pad centre | < 12 m |
| angular rate | < 4°/s |

All satisfied on contact → landed, parented to the body's rotating frame. Any one violated →
the existing hull-damage path, scaled by the excess. Landing on terrain that is not a pad is
allowed and costs hull proportional to the local slope.

### 3.5 Lagrange points — `orbit/lagrange.cpp`

With mass ratio `μ = m₂ / (m₁ + m₂)` and separation `a`:

```
L1  r ≈ a(1 - (μ/3)^⅓)      refine: 3 Newton steps on  (1-μ)/(a-r)² - μ/r² = (1-μ)/a³ · (a-r)
L2  r ≈ a(1 + (μ/3)^⅓)      same, mirrored
L3  r ≈ -a(1 + 5μ/12)
L4  a, +60° from the secondary       stable
L5  a, -60° from the secondary       stable
```

**The cheat, stated plainly.** A station at L1 has a smaller radius than the secondary, so a
Keplerian propagation of it would run at a different period and drift away within days. It is not
a Kepler orbit — it only exists in the three-body problem. So an L-point station is propagated as
a **co-rotating placement**: it takes the secondary's angular position at time `t` and its own
fixed radius, and is not a conic at all.

```
θ_station(t) = θ_secondary(t) + Δθ      r_station = r_L      // NOT a conic
```

A ship near it flies ordinary patched conics about the primary and therefore *will* drift relative
to the station — which is correct for L1/L2/L3 and will read as authentic. L4/L5 are genuinely
stable, so the same cheat there is indistinguishable from the truth.

`ponytail: co-rotating placement, not a conic. CR3BP if station-keeping ever becomes gameplay.`

### 3.6 Encounters and assists — `orbit/encounter.cpp`

Gravity assist needs no new physics — patched conics already produce it. What is missing is the
prediction, without which no one can aim one.

Walk the ship's conic forward looking for the first crossing of each body's SOI: solve for the
time at which `|r_ship(t) - r_body(t)| = r_SOI` by bisection on the interval where the sign of
that difference changes, sampled at one-hundredth of the ship's period. At the crossing, convert
to the body's frame (§ plan 02 3.3), derive the hyperbolic conic, find the exit, convert back.
Repeat up to three encounters deep and stop.

The result is a polyline per leg plus the post-encounter elements, which is exactly what the map
draws and what makes an assist aimable.

### 3.7 Survey and satellites — `sim/survey.cpp`

**Scan pass**: a body carries unscanned arcs. A pass records an arc when altitude is inside
`[h_min, h_max]` and ground-track speed is under a limit — low and slow, so it costs fuel and
risk rather than time.

**Satellite deployment**: a contract names a target orbit as a tolerance box on `(a, e)`. Deploy,
and the satellite is checked against the box at deployment and again one period later, so a
marginal orbit fails honestly rather than at the instant of release.

### 3.8 The flight director — `sim/program.cpp`

**F9: reads the world, writes nothing.** Every program returns a `Cue`; the HUD draws it; the
pilot flies it.

```cpp
enum class Program { None, Circularize, MatchOrbit, Approach, Dock, Deorbit, Land, KillRelative, Transfer };

struct GateTerm { const char* name; Real value; Real limit; const char* unit; bool ok; };

struct Cue {
    bool   active = false;
    Real   heading = 0;        // where to point the nose; the director cross
    Real   throttle = 0;       // commanded, 0..1; drawn against actual
    Real   burn_in = 0;        // seconds until the burn starts, < 0 while burning
    Real   burn_for = 0;       // seconds of burn remaining
    Real   dv_remaining = 0;
    const char* step = "";     // "match velocity", "align corridor", "close"
    std::vector<GateTerm> gates;
};

Cue evaluate(Program, const World&, Real t);   // const World& — the compiler enforces F9
```

Per program, the solution:

| Program | Cue |
|---|---|
| Circularize | Burn at apoapsis, `Δv = sqrt(μ/r_a) - v_a`, prograde. `burn_in` = time to apoapsis. |
| MatchOrbit | Hohmann to the target's `a` (plan 02 §3.5), then phase. Two nodes, two cues in sequence. |
| Approach | Point at the target's predicted position; `throttle` from a closing-rate profile `v = sqrt(2 a_max d)` · 0.6. |
| Dock | The five gate terms from plan 02 §3.6, as `gates`. `step` names the failing one. |
| Deorbit | Retrograde burn sizing periapsis to the target pad's `θ`, solved by bisection on `Δv`. |
| Land | §3.4. `burn_in` from `h_burn`; `heading` to local up-plus-lateral-correction; gates are the touchdown limits. |
| KillRelative | Retrograde on the relative velocity vector, `throttle` proportional to what is left. |
| Transfer | The planned node list; `burn_in` to the next node. |

---

## 4. Graphics

### 4.1 Frame structure

Current: one forward pass to an MSAA target, resolve, UI pass. New:

```
1  shadow pass       → 2048² D32, one ortho cascade along the star direction
2  scene pass        → HDR RGBA16F (MSAA) + depth (reversed-Z)
                       opaque → planets/star → additive effects
3  resolve           → HDR RGBA16F single-sample
4  bloom             threshold → 5 downsamples → tent upsample → combine
5  tonemap + grade   → swapchain LDR
6  UI pass           → swapchain, no tonemap  ← UI is authored in display space
```

The UI must be composited **after** tonemapping or the palette in `tokens.h` stops meaning what it
says. That is not a preference; a tonemapped `#DCE6E8` is not `#DCE6E8`.

One cascade is enough. Gameplay is a plane and the interesting shadow receivers are within a few
km of the ship; fit the cascade to the follow camera's frustum at the plane.

### 4.2 Vertex format

`MeshVertex` becomes:

```cpp
struct MeshVertex {
    glm::vec3 pos;
    glm::vec3 normal;
    glm::vec4 tangent;   // xyz + handedness in w
    glm::vec2 uv;
    glm::vec3 color;
};
```

This ripples through `gltf.cpp` (read `TANGENT`/`TEXCOORD_0`, generate tangents when absent),
`mesh.cpp`, `renderer.cpp` (two more vertex attributes) and `mesh.hlsl`. 48 → 60 bytes per vertex;
at 289k vertices that is 3.5 MB more, which is nothing.

Generate missing tangents per triangle from UV deltas and orthonormalize against the normal
(Gram-Schmidt). glTF only guarantees tangents when a normal map is already assigned.

### 4.3 Materials — `render/material.cpp`

From glTF: `baseColorFactor`, `metallicFactor`, `roughnessFactor`, and optional
`baseColorTexture`, `metallicRoughnessTexture`, `normalTexture`. Bind per run — the scene already
sorts by mesh, and material follows mesh, so this costs no extra draws.

`mesh.hlsl` becomes Cook-Torrance: GGX distribution, Smith visibility, Schlick Fresnel. The key
light is the star (direction and colour from the ship's actual position in the system), a
hemispheric ambient stands in for IBL, and the rim term stays because it is what separates a dark
hull from a dark field.

### 4.4 Planets and the star — `planet.hlsl`, `star.hlsl`

Planets are not meshes with textures; they are a sphere plus a shader, because they must read
correctly from a 2-pixel dot on the map to filling the frame on descent.

| Feature | Approach |
|---|---|
| Terminator | `N·L` with a softened wrap, width from the star's angular size at that distance |
| Atmosphere rim | Screen-space: radial falloff outside the disc, thickness from the scale height, tinted by Rayleigh-ish wavelength weighting |
| Cloud band | Two scrolling FBM layers in a latitude-banded coordinate, only above a distance threshold |
| Night side | City/thermal glow masked to `N·L < 0`, at very low intensity |
| Surface | Triplanar FBM albedo, displaced by the same seed the terrain uses, so the silhouette you land on is the one you saw from orbit |

LOD by **angular size in pixels**, not distance: below 4 px it is a disc with a colour; below 40 px
a shaded sphere; above that the full shader with the terrain displacement on.

The star: limb-darkened disc (`I(μ) = I₀(1 - u(1-μ))`, `u ≈ 0.6`), a corona that falls off as
`1/r²` in screen space, and enough HDR intensity that the bloom chain does the rest.

### 4.5 Backdrop correction (R-3)

The nebula drops to a quiet floor: peak luminance under 15% of the hull's lit value, saturation
halved. Contrast comes from the star and the drive flames, both of which are HDR and bloom — not
from the background. The rule is one line in `backdrop.cpp` and a screenshot.

### 4.6 HUD — `ui/blocks.cpp`, `ui/director.cpp`, `ui/minimap.cpp`

**F10: three density levels on a key, with context override.**

| Level | Blocks |
|---|---|
| 1 | speed, heading, g, hull/prop/heat, contact collar — roughly today |
| 2 | + orbit block, program/director panel, minimap |
| 3 | + per-thruster state, mass flow, thermal budget, power |

Context forces a block on regardless of level: docking forces the corridor block, descent forces
altitude / vertical speed / burn timer, an SOI change forces the orbit block for 10 s, an
out-of-tolerance gate forces its own block.

**Blocks are composable and self-measuring.** Each declares its size; the HUD lays them into the
four corner stacks inside the single safe inset. R-1 exists because clusters place themselves at
hand-computed y values — a block that reports its own height cannot overlap its neighbour, and
the layout assertion pass (§5) proves it every frame.

The orbit block:

```
orbit    Tessera
  alt      412.6 km
  peri     318.1 km    T-  4 m 12 s
  apo    1 204.8 km    T- 31 m 06 s
  ecc        0.0412
  period    1 h 48 m
  Δv        2.41 km/s
```

The director panel (F9 — cues, never control):

```
DOCK  wayfarer.A
  ✓ match velocity      Δv    0.00 m/s
  ✓ align corridor      off    1.2 m
  ▸ close                      8.4 m   0.9 m/s
    capture               —

  ⊕ fly to the cross · throttle 0.24
```

**Minimap (F5)**: one corner, one footprint, contents by context. Local contact radar while
mining or manoeuvring near objects; orbital strip (altitude, periapsis, apoapsis as ticks along a
scale) while coasting or transferring; corridor plan view on approach. A key cycles it manually
and the context sets it otherwise. North is world north, matching the collar, always — a rotating
minimap next to a fixed collar is two instruments disagreeing.

**Warm/cool split (R-4)**: flight glass is `etch`/`nav`/`drive`. Chart surfaces — map, ephemeris,
minimap's orbital mode, startup — are `vellum` on `plate`. This is the plan-02 idea finally doing
work; the test is that a thumbnail of the map and a thumbnail of the HUD are not the same colour.

---

## 5. UI flow and testing

### 5.1 The screen graph as data — `ui/flow.cpp`

```cpp
enum class Screen { Startup, Flight, Map, Manual, Pause, Settings, Hangar, Viewer };
struct Transition { Screen from; Action on; Screen to; bool pushes; };
extern const Transition FLOW[];
```

Every screen change goes through this table. No `app.map = !app.map` anywhere. The table gives:
a testable graph, a guaranteed way back from every screen, and one place to see the whole flow.

Also fixes the class D-13 belonged to: an action is consumed by exactly one screen, decided by the
table, so Tab cannot both cycle a contact and move UI focus.

### 5.2 Four test layers (F7)

| Layer | Runs | Asserts |
|---|---|---|
| **Screen graph** | `--selftest`, headless | Every screen reachable; every screen has a path back to Flight; no action bound twice in one screen; no unreachable transition. |
| **Layout pass** | `--debug`, every frame | Every emitted text and rect is inside the safe area and overlaps no sibling block. Logs loudly with the block name. **This is the one that would have caught R-1.** |
| **Golden images** | `tools/shots.ps1 -Check` | Perceptual diff of all 10+ views against committed references; fail over threshold. Re-blessing is a deliberate, reviewed commit. |
| **Input tapes** | `tools/drive_input.py` | A deterministic tape per flow — boot → menu → launch → fly → mine → dock → map → plan → warp → arrive → deorbit → land — asserting sim state at named checkpoints and capturing frames at named beats. |

Golden images need determinism the build does not yet have: fix the frame clock under
`--screenshot` (already done), and seed every RNG from the system file. Any remaining drift is a
bug the golden test is correctly reporting.

---

## 6. Phases

**G0 gates everything.** After that, dependencies are the only ordering constraint.

| P | Name | Depends | Deliverable |
|---|---|---|---|
| **G0** | Plan-02 cleanup | — | R-1..R-8 fixed. Repo clean. |
| **G1** | Camera + follow | G0 | Fixed pitch, deadzone/lead/damping, every `screen*0.5` in the HUD replaced by `shipScreen`. |
| **G2** | HUD blocks + density | G0 | Self-measuring blocks, three levels, context override, layout assertion pass. |
| **G3** | Screen graph + tests | G2 | `flow.cpp`, screen-graph asserts, golden images, one input tape. |
| **G4** | Vertex format + PBR | G0 | Tangents/UVs, materials from glTF, Cook-Torrance, normal maps. |
| **G5** | HDR + post | G4 | RGBA16F, bloom chain, ACES tonemap, grade. UI composited after. R-3. |
| **G6** | Shadows | G4 | One cascade, fitted to the follow frustum. |
| **G7** | Planets + star | G5 | `planet.hlsl`, `star.hlsl`, angular-size LOD, the orrery's bodies upgraded. |
| **G8** | Director | G2 | `program.cpp`, all nine programs, director cross, gate blocks, panel. |
| **G9** | Minimap | G2, G8 | Contact radar, orbital strip, corridor plan, context switching. |
| **G10** | Encounters + assists | — | SOI crossing prediction, three legs deep, drawn on the map. R-8's transfer arc lands here. |
| **G11** | Lagrange | G10 | L1–L5, co-rotating stations, map treatment. |
| **G12** | Atmosphere | G10 | Density, drag, Sutton–Graves heating, warp clamp, predicted post-pass conic. |
| **G13** | Terrain | G6 | Radial heightfield, pads, surface collision, terrain in the planet shader. |
| **G14** | Descent + landing | G12, G13, G8 | Hoverslam cue, touchdown gate, landed state, damage on a bad set-down. |
| **G15** | Survey + satellites | G10 | Scan arcs, deployment contracts, orbit tolerance checks. |
| **G16** | Bases | G14 | Surface stations on pads: refuel, repair, contracts. No construction. |
| **G17** | Assets | G4 | §7, exported and wired. |

**Playable checkpoints** — each of these is a state worth stopping at: after **G3** the instrument
is right; after **G9** the flight loop is right; after **G12** orbital play is complete; after
**G14** landing works; after **G16** the system is a place.

### Non-goals

No combat. No walkable surface. No base construction (G16 is refuel/repair/contracts on an
existing pad). No second system. No FTL. No multiplayer. No autopilot (**F9**).

---

## 7. Asset brief

For the three.js authoring agent. Same contract as plan 01 §3.1 — one module per asset exporting
`build(): THREE.Object3D` and optional `meta` — with two additions.

**F8 fidelity: mid-poly.** Denser than the current flat-faceted ships: smooth-shaded curved
surfaces where a real one would be curved, bevelled edges on structural members, panel-line relief
as geometry where it reads at the sizes below. Budgets are targets, not limits.

**Procedural material maps.** Each module may export

```ts
export const maps = {
  normal:    (ctx, size) => void,   // draw panel lines, rivets, weld seams
  roughness: (ctx, size) => void,   // wear at edges, polish on glass
  albedo:    (ctx, size) => void,   // stencils, hazard stripes, scorch
};
```

drawn with a 2D canvas API into a 512² buffer that `export.mjs` bakes into the GLB. No hand-painted
texture files; anything an LLM cannot write as code does not exist yet.

Conventions from plan 01/02 still apply: nose +Y, dorsal +Z, metres; `userData.effect = true` on
anything that only draws while firing; `dock.<id>` nodes become docking ports; `hp.<id>` become
hardpoints.

### 7.1 Needed assets

| Asset | Tris | Notes |
|---|---|---|
| **Ships** | | |
| `kestrel`, `mule`, `needle` | 4–8k | Re-author at F8 fidelity. Must keep the exported collider within 5% of current, or the sim retunes. Add `dock.fwd` and landing-leg geometry. |
| **Ship components** (for the modular future, plan 02 §7) | | |
| `drive_main`, `drive_aux` | 800–2000 | Bell, gimbal ring, plumbing. Flame cone as an `effect` child. |
| `rcs_block` | 300–600 | Four-nozzle quad. Each nozzle an `effect` child, named `jet.<n>`. |
| `tank_small`, `tank_large` | 400–900 | Cryo insulation, transfer plumbing, a fill-level decal in `albedo`. |
| `hab_module` | 1500–3000 | Windows as `glass` material, handrails, a `dock.side` port. |
| `cargo_pod` | 500–1200 | Latching lugs matching `hab_module`'s mounts. |
| `radiator_panel` | 300–800 | Deployable, hinged. Two `hp.` nodes so a future editor can fold it. |
| `sensor_mast` | 400–900 | Dish, feed horn, gimbal. `hp.scan` at the boresight. |
| `landing_leg` | 400–900 | Three-segment with a footpad. `hp.contact` at the pad's underside. |
| `docking_collar` | 300–700 | The port ring itself. `dock.A` at its face, normal along +Y. |
| **Stations** | | |
| `station` (Wayfarer) | 12–20k | Re-author at F8. Add `dock.A` … `dock.D` at real ring positions, and approach lighting geometry. |
| `station_depot` | 4–8k | Small fuel depot for low orbits. Two ports, big tanks, no habitation. |
| `station_lagrange` | 15–25k | The L4 base. Long truss, rotating habitat ring, solar wings, four ports. The largest thing in the game — it should read as built over decades. |
| `surface_base` | 8–15k | Landing pads, habitat, fuel plant, a mast. Sits on terrain, so its footprint must be flat and its origin at pad level. |
| `landing_pad` | 600–1500 | Standalone: deck, approach lights, hold-down clamps. `dock.pad` with the normal along local up. |
| **Props** | | |
| `satellite_relay` | 800–1800 | Deployable. Folded and unfolded states as two `hp.`-linked groups. |
| `probe_scanner` | 600–1200 | Small, expendable. |
| `nav_buoy` | 300–700 | Strobe as an `effect` child. |
| `beacon`, `derelict`, `cargo`, `ore` | current +2× | Re-author at F8. `derelict` wants real damage — torn plate, exposed frame, scorch in `albedo`. |
| **Celestial** | | |
| `sphere_lod0/1/2` | 1k / 5k / 20k | Plain UV spheres. The planet shader does everything else (§4.4); these exist only so it has something to rasterize. |

Order of work: ships and `landing_pad` first (G14 needs legs and a pad), then stations, then
components, then props, then celestial spheres. `station_lagrange` last — it is the biggest and
nothing blocks on it.

---

## 8. Review gates

Everything from plan 02 §8, plus:

1. `sim/program.cpp` takes `const World&` everywhere. A director that mutates state violates F9.
2. No `screen * 0.5f` in `hud.cpp`. The ship is not at screen centre any more.
3. Every HUD block reports its own height; the layout assertion pass runs clean at 1600×900,
   1920×1080, 1280×720 and 2560×1440.
4. The UI is composited after tonemapping. `#DCE6E8` in `tokens.h` is `#DCE6E8` on screen —
   sample the pixel and assert it.
5. Golden images re-blessed only in a commit that says what changed and why.
6. Camera smoothing is frame-rate independent: the same tape at 60 and 240 fps ends with the
   camera target in the same place to 1e-3.
7. Lagrange stations carry the `ponytail:` comment from §3.5. The cheat stays documented.
8. Nothing from the §6 non-goals list appears.
