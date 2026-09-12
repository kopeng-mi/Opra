> **Status: complete.** The live plan is [PLAN-04-assets.md](PLAN-04-assets.md).

Live plan. [PLAN-01](PLAN.md) (module split + glTF pipeline) is complete and its decisions still bind.

This plan turns a 5.2 × 4.2 km flight box into a star system you navigate: real gravity, real
orbits, real transfers, real docking. It also rebuilds the view (perspective camera, parallax),
the type system, and the UI's visual language, and clears 17 confirmed defects.

---

## 0. Decisions

Locked in this round. Later phases may not relitigate them.

| # | Decision | Consequence |
|---|---|---|
| **E1** | **Compact real system, true physics.** M-dwarf, orbits 0.05–0.29 AU, periods 6–91 days. Real μ, real ratios. | Positions in `double`, metres, origin at the system barycentre. Transfers take days of sim time, which time warp makes playable. Nothing is fudged. |
| **E2** | **Planets on rails, ship patched-conic.** Bodies from analytic 2D Kepler. Ship integrated under thrust, advanced analytically while coasting. SOI switching. | Warp is exact, not integrated. Hohmann/Lambert planning is possible because there is always a closed-form conic. |
| **E3** | **One continuous space.** No zones, no loading. The belt is a real ring of orbiting bodies; Wayfarer orbits inside it. | Render origin rebases on the camera every frame so `float` never sees a Gm-scale coordinate. |
| **E4** | **Systems are data.** `assets/systems/*.json`, loaded through a registry. Inert `jump_links` field from day one. | Multiple systems and FTL are a later conversation, not a later rewrite. **Nothing may hardcode a body.** |
| **E5** | **Narrow-FOV perspective, constrained orbit camera.** 24° vertical FOV; pitch 15–90° on Ctrl + mouse Y; **yaw locked**, roll always zero. | Replaces `glm::orthoZO`. Parallax, depth and tilt come free. World north stays screen up, so the collar's bearing frame is unchanged. |
| **E6** | **Four parallax layers** — stars (camera-locked), dust, nebula, near motes. | Placed at real depths as real geometry; perspective produces the parallax. No manual per-layer offset factors. |
| **E7** | **Stepped time warp** 1 / 10 / 100 / 1k / 10k / 100k, with automatic drops. | Warp > 10× forces the ship onto its conic (no thrust). |
| **E8** | **Compound colliders, one shape per component.** | Exported from geometry today; when ships become component-assembled each component brings its own. No separate authoring step, ever. |
| **E9** | **Named docking ports** with position, outward normal and size class. Approach gated on corridor, closing rate, alignment and rate. | Ports are hardpoints in the model, so they become a component on modular ships. |
| **E10** | **Type: Hydrogen Whiskey (display) + Barlow Condensed (readout) + Barlow (label).** All shipped in `assets/fonts/`. | No system-font dependency. Tabular figures on every readout. |
| **E11** | **Art direction: orrery + almanac.** Warm ivory chart ink against cool teal flight glass. | Two instruments, one darkness. Detailed in §4. |
| **E12** | **Startup: live orrery behind an almanac title plate.** | The title screen runs the real map renderer. |

Carried forward from PLAN-01 and still binding: `sim` is `double` throughout; dependencies point
one way (`game → {sim, render, ui} → gpu → core`); every phase ends with a clean `/W4` Release
build and `Opra.exe --selftest` exit 0.

**New invariant:** no `float` may hold a world position. World space is `double` metres. The
`float` cast happens exactly once, in `SceneBuilder`, after subtracting the render origin.
A grep for `static_cast<float>(.*\.position` outside `render/scene.cpp` should find nothing.

---

## 1. Defect ledger

17 confirmed, each reproduced. Fix all of these in **D0** before any new system lands — several
of them will otherwise be misdiagnosed as bugs in the new code.

### Critical

| ID | Site | Fault | Fix |
|---|---|---|---|
| **D-1** | `game/frame.cpp:239-244` | The cutter's arc is centred 90° off the nose. `physics.cpp:68-69` defines `forward = (-sin, cos)` and `right = (cos, sin)`; the beam is built as `{cos(h+b), sin(h+b)}` — that is **starboard**. `delta = want - heading` is measured from starboard, so `CUTTER_ARC` clamps around the beam. Aim at a rock dead ahead: `delta ≈ π/2`, clamped to 0.50, and the beam fires 61° off the cursor. | `direction = {-sin(h+b), cos(h+b)}`, and measure `delta` from `h + π/2`. |
| **D-2** | `game/loop.cpp:64` | `update_app` gets the fixed `dt = 1.0/120.0` but runs **once per rendered frame**, and the loop is vsync-paced by `SDL_WaitAndAcquireGPUSwapchainTexture`. Cutter damage, fuel draw, heat and zoom smoothing all scale with display refresh: at 60 Hz rocks take 2× as long to cut, at 240 Hz half. | Pass real elapsed seconds (`(now - previous)/1000.0`, clamped) to `update_app`. |
| **D-3** | `gpu.cpp:78`, `:173` | `SDL_UploadToGPUBuffer(..., false)` — cycle **false** on the destination, while the comment above claims a write "never waits on the frame still in flight". `renderer.instances` and `ui_vertices` are bound by frames still in flight; the copy overwrites the allocation they are reading. Tearing under load. `update_texture` is the same against the live glyph atlas. | `true` as the final argument in both. |
| **D-4** | `frame.cpp:466` → `app.cpp:104` | `apply_settings` mutates `renderer.samples` mid-frame, **after** `ensure_depth` has already sized the depth texture for the old count. That frame binds a 4× colour target and pipeline against a 1× depth texture — an invalid D3D12 render pass. Repro: pause → settings → click 4×. | Stage it: `pending_samples`, applied in `loop.cpp` before `ensure_depth`. |
| **D-5** | `render/text.cpp:134-156` | Each glyph quad is sized from `TTF_GetGlyphMetrics` (`max_x-min_x` × `max_y-min_y`) but its UVs come from the `TTF_RenderGlyph_Blended` surface, which is a **different size** — every glyph is slightly scaled. Then `bearing_x`/`bearing_y` are applied on top of a surface that already bakes the bearing in. `31.7` renders as `31 7`. | Superseded by **D-6**. Do not patch it. |

### D-6 — replace the glyph atlas with SDL_ttf's GPU text engine

`src/render/text.cpp` is 251 lines reimplementing what SDL_ttf 3.2.2 already ships:
`TTF_CreateGPUTextEngine(device)`, `TTF_CreateText(engine, font, str, len)`,
`TTF_GetGPUTextDrawData(text)` → `TTF_GPUAtlasDrawSequence { atlas_texture, xy, uv, indices }`.
It owns the atlas, the packing, the growth, the layout and the kerning, correctly.

This deletes D-5, D-9 and D-10 outright and removes ~200 lines. It needs:
- An **indexed** draw path in the UI pipeline (the sequence hands back `int*` indices).
  The solid-geometry path stays non-indexed, or gets a trivial generated index buffer.
- A `TTF_Text*` cache keyed by (face, px, string), with `TTF_SetTextString` for values that
  change every frame so the object is reused rather than recreated.
- `TTF_SetGPUTextEngineWinding` set to match the UI pipeline's front face.

### Medium

| ID | Site | Fault | Fix |
|---|---|---|---|
| **D-7** | `ui/ui.cpp` `Context::begin` | `active_` is only cleared by the widget that owns it, on a frame where that widget is submitted again. Hold the mouse on the viewer's grid toggle, press F2 to close the viewer, and `active_` stays latched forever: reopening and *releasing* over that widget fires it with no press. On a slider it snaps the value to the cursor the instant you press anywhere inside. | `if (!pointer.down) active_ = 0;` at the top of `begin()`. |
| **D-8** | `frame.cpp:110-111` | `nav.decrease`/`increase` use `input.held`; every other nav action uses `input.pressed`. `Context::slider` applies 5% of range **per frame** — at 120 fps, holding Right sweeps the whole zoom range in 0.17 s and jumps a list to its last row instantly. | `input.pressed(...)` for both. |
| **D-9** | `text.cpp:70-110` | `TextEngine::grow()` has **no call site**. `reserve()` sets `grow_pending_` and nothing reads it. `device_handle_` / `attached_device_` are never assigned. Once the atlas fills, every unpacked glyph is cached with zero size and is invisible for the rest of the session. | Deleted by D-6. |
| **D-10** | `text.cpp:159-168` | A glyph that fails to pack is emplaced into the cache **permanently broken**, not retried next frame. | Deleted by D-6. |
| **D-11** | `game/viewer.cpp:76` vs `:51-67` | `update_viewer` scales `viewer.distance`; `Viewer::camera()` computes everything from `model_extent` and never reads `distance`. Wheel zoom in the model viewer does nothing, though the viewer's own footer advertises it. | Fold `distance` into `half_height` (and into the eye offset after E5). |
| **D-12** | `game/viewer.cpp:61` | `aspect = 16.0f/9.0f` hardcoded in a resizable window. The model viewer — whose whole job is checking exported geometry against the collider overlay — stretches at any other ratio. | Pass the drawable size in. |
| **D-13** | `frame.cpp:153-275` | Gameplay keys stay live while the pause menu is up; only `app.mining` checks `!app.paused`. Tab both cycles the tracked contact and moves UI focus on the same frame. R recovers cargo from inside the menu. F toggles assist and writes `settings.ini`. | Wrap the gameplay block in `if (!app.paused)`. |
| **D-14** | `sim/world.cpp:7-17` | `world.target` is a positional index into a list rebuilt every call, which contains only *uncollected* cargo followed by three fixed marks. Recover a cargo while tracking the Relay and the tracked contact silently becomes Kite's End. | Store identity (a stable id), not a position. |

### Low

| ID | Site | Fault | Fix |
|---|---|---|---|
| **D-15** | `frame.cpp:80` | `const bool reloaded = pressed(input, Action::ModelViewer) == false && false;` — unconditionally false, so the viewer's post-reload index clamp never runs. | Clamp `viewer.selected` unconditionally in `update_viewer`. |
| **D-16** | `app.cpp:126-134` | `reload_models_if_stale` does not refresh `world.ship.collider`, though both `init` and `reset_run` do. F5 after re-exporting a ship draws the new collider and collides with the old one. | Repeat the assignment at the end of the reload path. |
| **D-17** | `renderer.cpp:323-327` | The comment describes a two-pass MSAA resolve scheme the code does not implement (it sets `RESOLVE_AND_STORE` unconditionally and gives the HUD pass a single-sample target). Behaviour is correct, the comment is not. | Rewrite the comment to match. |

Plus two from my own review, not in the audit:

| ID | Site | Fault | Fix |
|---|---|---|---|
| **D-18** | `hud.cpp` corner clusters | Bottom-left and bottom-right clusters are drawn past the window edge — `Kestrel / flight assist / kill velocity` sits at y≈888 in a 900 px window and is clipped. No safe-area margin exists. | §4.4: one safe-area inset, applied by the HUD layout, not per-cluster. |
| **D-19** | `assets/*.json` colliders | `kestrel.json` reports `collider {59, 34}` — the inherited AstraWars constant — while the model's own AABB gives `halfLength ≈ 53` at scale 1.3. The box is 11% long and, more importantly, a single 118 × 68 m box around a hull ~35 m wide means rocks register a hit well off the visible ship. | Superseded by **P4** (compound colliders). |

**D0 acceptance:** all 17 fixed, `--selftest` green, and three new selftest cases —
cutter bearing (a rock dead ahead is hit at `bearing == 0`), frame-rate independence (600 sim
seconds at simulated 60 and 240 fps produce the same fuel and heat to 1e-6), and UI `active_`
release.

---

## 2. Architecture

New and changed modules. Everything else from PLAN-01 stands.

```
assets/
  systems/nereid.json          <- the system definition (E4)
  fonts/  HydrogenWhiskey.otf  BarlowCondensed-{Regular,SemiBold}.ttf  Barlow-Regular.ttf
src/
  orbit/                       NEW — pure math, no SDL, no game types
    kepler.h/.cpp              elements <-> state vector, anomaly solvers, propagation
    conic.h/.cpp               Conic: the drawable/queryable orbit object
    transfer.h/.cpp            Hohmann, phase windows, Lambert, maneuver nodes
    soi.h/.cpp                 sphere of influence, frame switching
  sim/
    system.h/.cpp              NEW — SystemDef, Body, BodyState, registry, JSON load
    belt.h/.cpp                NEW — deterministic belt generation from seeded elements
    dock.h/.cpp                NEW — ports, approach gates, capture
    shapes.h/.cpp              NEW — compound collider, broad + narrow phase
    physics.h/.cpp             gravity added to step_ship; warp-aware
    world.h/.cpp               owns SystemState, ship, belt residency
  render/
    camera.h/.cpp              perspective, orbit constraint, render-origin rebase
    backdrop.h/.cpp            NEW — the four parallax layers
    orrery.h/.cpp              NEW — system map renderer (3D, shared with startup)
    text.h/.cpp                rewritten on TTF_GPUTextEngine (D-6)
  ui/
    tokens.h                   NEW — the design system: palette, type scale, spacing, safe area
    table.h/.cpp               NEW — the almanac table primitive (§4.3)
    hud.h/.cpp                 re-laid-out against tokens.h
    map.h/.cpp                 NEW — the orrery screen's UI layer
    title.h/.cpp               NEW — startup screen
  game/
    warp.h/.cpp                NEW — time warp rail and auto-drop rules
```

**Dependency rule extends:** `orbit/` depends on nothing but `<cmath>` and `glm`. It must be
unit-testable with no SDL, no device, no world. This is what makes the transfer math reviewable.

---

## 3. The math

Everything here is 2D in the ecliptic plane (z = 0). That removes inclination, RAAN and the
plane-change term from every formula — which is the single largest simplification the
"3D graphics, 2D gameplay" premise buys.

### 3.1 Kepler propagation — `orbit/kepler.cpp`

Elements: `{ a, e, ω, M0, t0, μ }`. All `double`, SI.

```
n  = sqrt(μ / a³)                                   mean motion
M  = wrap(M0 + n (t - t0))                          mean anomaly, to [-π, π]
```

Solve Kepler's equation `E - e sin E = M` by Newton with Danby's starter:

```
E  = M + e sin M                                    good to e < 0.6 in 3 iterations
loop 5x:  f  = E - e sin E - M
          E -= f / (1 - e cos E)
          break when |f| < 1e-13
```

Then:

```
ν  = 2 atan2( sqrt(1+e) sin(E/2), sqrt(1-e) cos(E/2) )
r  = a (1 - e cos E)
pos = rot(ω) · (r cos ν, r sin ν)
ṙ   = sqrt(μ a) / r · e sin E
rν̇  = sqrt(μ a (1-e²)) / r
vel = rot(ω) · ( ṙ cos ν - rν̇ sin ν,  ṙ sin ν + rν̇ cos ν )
```

Hyperbolic branch (`e > 1`, a ship on escape): `M = e sinh H - H`, starter `H = asinh(M/e)`,
same Newton loop; `ν = 2 atan2(sqrt(e+1) sinh(H/2), sqrt(e-1) cosh(H/2))`, `r = a(1 - e cosh H)`
with `a < 0`. Near-parabolic (`|e - 1| < 1e-4`) is the one case Newton handles badly — clamp `e`
away from 1 and log it; a real parabolic orbit is a measure-zero case no player will hold.

### 3.2 State vector → elements

```
h      = r.x v.y - r.y v.x                          scalar angular momentum
energy = |v|²/2 - μ/|r|
a      = -μ / (2 energy)                            negative for hyperbolic
e⃗      = ((|v|² - μ/|r|) r⃗ - (r⃗·v⃗) v⃗) / μ
e      = |e⃗|
ω      = atan2(e⃗.y, e⃗.x)                            undefined for e < 1e-8: use 0
ν      = signed angle from e⃗ to r⃗, sign = sign(r⃗·v⃗)
E      = 2 atan2( sqrt(1-e) sin(ν/2), sqrt(1+e) cos(ν/2) )
M0     = E - e sin E     at t0 = now
```

Round-trip (`elements → state → elements`) must close to 1e-9 relative. That is a selftest case.

### 3.3 Sphere of influence — `orbit/soi.cpp`

```
r_SOI = a_body · (m_body / m_parent)^(2/5)
```

The ship's parent is the deepest body whose SOI contains it. Check each frame, cheapest first
(current parent, then its children, then its parent). On a switch:

```
r_new = r_old + pos(old_parent, t) - pos(new_parent, t)      in double, in the barycentric frame
v_new = v_old + vel(old_parent, t) - vel(new_parent, t)
elements = from_state(r_new, v_new, μ_new)
```

Add **hysteresis**: enter at `r_SOI`, leave at `1.02 · r_SOI`. Without it, a ship skimming the
boundary thrashes between frames and the drawn orbit flickers. Force warp to 1× on any switch.

### 3.4 Ship integration — `sim/physics.cpp`

Three regimes, chosen per step:

| Regime | Condition | Method |
|---|---|---|
| Powered | any thrust, RCS, or a contact this step | RK4 on `a = -μ r̂/r² + a_thrust`, dt = 1/120 |
| Coasting | no thrust, warp ≤ 10× | Same RK4. Cheap, and keeps contact response instant. |
| Railed | no thrust, warp > 10× | Advance the conic analytically: `M += n·Δt`, solve, read the state. **Exact at any Δt.** |

Entering *railed*: derive elements once from the current state (§3.2). Leaving it: read the state
back and hand it to the integrator. The conversion is lossless to 1e-9, so warping in and out
does not drift the orbit — which is the whole reason for E2.

Gravity enters `step_ship` as one term. The existing Newtonian flight model is otherwise
untouched: `maxAcceleration = thrust/(dryMass + fuel)`, the same torque, assist and heat
behaviour. **`step_ship`'s existing outputs must not change when μ = 0** — that is a selftest
case, and it protects the PLAN-01 determinism golden.

### 3.5 Transfers — `orbit/transfer.cpp`

**Hohmann**, circular `r1 → r2` about `μ`:

```
a_t  = (r1 + r2) / 2
Δv1  = sqrt(μ/r1) · (sqrt(2 r2/(r1+r2)) - 1)
Δv2  = sqrt(μ/r2) · (1 - sqrt(2 r1/(r1+r2)))
T    = π sqrt(a_t³ / μ)
```

**Departure window.** The target must lead the ship by

```
φ_required = π - n2 · T
```

at the burn. With the current phase difference `φ_now` and synodic rate `n1 - n2`:

```
t_wait = wrap_positive(φ_required - φ_now) / (n1 - n2)
```

That single expression is the "next transfer window" readout on the map. Repeat for successive
windows by adding the synodic period.

**Lambert** (arbitrary `r1 → r2` in a chosen `Δt` — needed for intercepting a rock or a moving
station, not for planet-to-planet): universal-variable formulation, Bate–Mueller–White /
Vallado. ~80 lines, bisection on the universal anomaly `z`, converges in under 20 iterations for
`Δt` within 0.1–10× the Hohmann time. **Defer to P7** — Hohmann plus phasing covers everything
in the first playable.

**Maneuver node**: `{ t, Δv_prograde, Δv_radial }` (normal is zero in 2D). Apply by propagating
the conic to `t`, adding `Δv` in the velocity frame, and re-deriving elements. A node list is
applied in time order to produce the predicted trajectory. The planner is then: drag a node's
prograde handle, watch the resulting conic redraw. No solver needed for the manual case.

### 3.6 Docking — `sim/dock.cpp`

A port is `{ id, local_pos, local_normal, class }`, exported from the model as a hardpoint named
`dock.<id>` (§5.2). World transform applied per frame.

Approach gate, ship port `S` against target port `T`:

```
r_rel   = S.pos - T.pos                             double, barycentric
axial   = dot(r_rel, T.n)                           along the corridor
lateral = |r_rel - axial · T.n|                     off-axis offset
closing = -dot(v_rel, T.n)
align   = angle(-S.n, T.n)
```

Gate (tunable per class, these are class M):

| Quantity | Limit |
|---|---|
| `axial` | 0 < axial < 60 m |
| `lateral` | < 8 m |
| `closing` | 0 < closing < 1.2 m/s |
| `align` | < 12° |
| `ω_rel` | < 3°/s |

All satisfied continuously for 0.4 s → **capture**. Capture drives the ship to hard dock along a
cubic Hermite in the port frame over ~2 s, blending out pilot control; the ship is then parented
to the station's frame. Undock applies a 0.5 m/s separation impulse along `T.n`.

Failing the gate is not an error state — the HUD simply shows which term is out (§4.4). Exceeding
`closing` by more than 3× inside 20 m is a collision, handled by the normal contact path.

### 3.7 Compound colliders — `sim/shapes.cpp`

```cpp
struct Shape {
    enum class Kind { Box, Circle } kind;
    Vec2  local_pos;                 // metres, model frame
    Real  local_angle;               // radians, boxes only
    Real  half_length, half_width;   // boxes
    Real  radius;                    // circles
};
struct Collider {
    std::vector<Shape> shapes;
    Real bounds_radius;              // broad phase, covers every shape
};
```

Broad phase: bounding-circle test. Narrow phase: the **existing** `obb_circle_out` /
`obb_obb_out` per shape, plus trivial circle–circle.

Resolution must accumulate, not apply per shape: gather every MTV, take the one with the largest
penetration, apply it once. Applying each shape's correction in turn double-counts the impulse
and makes a ship bounce off a rock at several times the incoming speed — the failure this design
exists to avoid.

Generation (exporter, §5.2): cluster the merged geometry by connected component in the XY plane,
fit an oriented box per cluster via the minimum-area rectangle of its 2D convex hull, drop
clusters under 3% of total area. Ship the result in the sidecar. `meta.collider` in the model
source overrides it, per component, when a human disagrees.

Expected for the Kestrel: ~4 shapes against today's single 118 × 68 box. The visible win is the
Mule, whose open chassis currently collides across its own gaps.

### 3.8 Camera — `render/camera.cpp`

```
pitch p ∈ [15°, 90°], default 32°.  yaw locked to 0.  roll always 0.
R       = half_height / tan(fov/2)                  fov = 24° vertical
eye     = target + R · (0, -cos p, sin p)
up      = +Z
proj    = perspective_zo(fov, aspect, near, far)
```

**Reversed-Z is required.** The backdrop sits at z ≈ -4000 while the near mote layer is at
z ≈ -60 and hulls are within ±15 — a conventional depth buffer over that range loses the hull
detail to precision. Reversed-Z: `near`/`far` swapped in the projection, clear depth **0.0**,
`SDL_GPU_COMPAREOP_GREATER`. Five lines, and it removes the whole class of z-fighting.

`project` and `unproject` must be rewritten for perspective — the existing versions assume the
orthographic ray bundle. `unproject` becomes a ray/plane intersection against z = 0:

```
ray_dir = normalize(inverse(view_proj) · ndc)
t       = -eye.z / ray_dir.z
world   = eye + t · ray_dir        // undefined when the ray is parallel to the plane:
                                   // clamp pitch ≥ 15° so it never is
```

Everything that consumed the old projection changes with it: HUD world-marks, contact picking,
the cutter's aim, the chart. Each has a selftest case that projects a known world point and
asserts the screen position.

**Render origin.** Per frame, `origin = camera.target` (a `double` Vec2 promoted to `dvec3`).
Every instance position is `float(world_pos_double - origin)`. Never `float(world) - float(origin)` —
at 26 Gm that subtraction has already lost every metre of precision before it happens.

### 3.9 Backdrop — `render/backdrop.cpp`

| Layer | Depth | Content | Motion |
|---|---|---|---|
| Stars | camera-locked | ~1200 points on a sphere | Translation ignored; rotates with the camera only |
| Nebula | z ≈ -4000 | 3–5 large soft quads, additive | Real geometry, real parallax |
| Dust | z ≈ -1200 | ~600 instanced motes | Real geometry |
| Local motes | z ∈ [-90, -30] | ~300, re-wrapped into a box around the ship | Real geometry + own drift |

The near motes are the only layer that reads as speed. Stars at real distances do not move and
must not be faked into moving; that is why the local layer exists.

Re-wrapping: when a mote leaves the box around the ship, teleport it to the opposite face with a
fresh lateral offset. Deterministic from a seeded RNG so a screenshot is reproducible.

---

## 4. Design system

### 4.1 The idea

The subject is a salvage pilot's instrument in a compact red-dwarf system. The design's job is to
answer three questions while the reader is moving: where am I, where am I going, how much is left.

**Two materials, one darkness.** The flight view is *glass you look through* — cool, etched with
marks, mostly bare. The map and every data screen are *a chart you read* — warm ivory ink on a
dark plate, ruled like a printed nautical almanac. Same near-black, two inks. That contrast is
the memorable thing, and it is the one place boldness gets spent.

The orrery carries it: concentric rings at true proportion, true-anomaly ticks, the transfer drawn
as the actual conic rather than a decorative arc.

*Checked against the generic defaults:* near-black with one bright accent is a known AI-design
tell. This is not that — it is near-black with a deliberate warm/cool material split and three
inherited semantic hues. The almanac table, not a card grid, is the structural device, and it is
structural because the data genuinely is tabular. No all-caps labels (Hydrogen Whiskey is
caps-only by design, and it appears only where the typeface *is* the treatment). No eyebrow
labels, no middle-dot meta strings, no `→` on actions, no monospace standing in for "technical".

### 4.2 Tokens — `src/ui/tokens.h`

```
void      #070D15   the field. flight background, deepest ground.
plate     #0E141B   chart ground. one step up, fractionally warmer.
etch      #DCE6E8   cool white. primary marks on glass.
vellum    #E6DCC8   warm ivory. chart ink, ephemeris figures, orrery rings.
nav       #83B9B5   teal. navigation, contacts, things that are where they should be.
drive     #EFB879   amber. propulsion, energy, the active transfer.
threat    #DF8277   coral. out of tolerance, and nothing else.
```

Opacity is the second axis and carries as much meaning as hue: `1.00` live, `0.55` dormant,
`0.28` structural rules, `0.12` grid. A dormant readout is not a different colour, it is the same
ink at lower weight — which is how a real backlit panel behaves.

Spacing: 4 px base, scale 4 / 8 / 12 / 20 / 32 / 52. Safe area 28 px, or 3% of the short edge,
whichever is larger (fixes **D-18**).

### 4.3 Type

| Role | Face | Sizes | Use |
|---|---|---|---|
| Display | Hydrogen Whiskey | 50, 32 | Title, screen names. Caps only, tracked +0.08em. |
| Readout | Barlow Condensed 600 | 25, 20, 16, 13 | Every number. Tabular figures, aligned on the decimal. |
| Label | Barlow 400 | 13, 10 | Prose, units, row headers. Sentence case. |

Scale: 10 / 13 / 16 / 20 / 25 / 32 / 40 / 50 — a ~1.25 ratio, per Bringhurst's default guidance.

**Tabular figures are not optional.** A speed readout counting through 199 → 200 must not shift
width. Enable with `TTF_SetFontStyle`-adjacent feature selection if SDL_ttf exposes it; otherwise
use Barlow Condensed's default lining figures and verify the digit advances are equal by asserting
`measure("0000") == measure("1111")` in the selftest.

### 4.4 Layout

**Flight HUD** — unchanged vocabulary, corrected geometry. Safe-area inset applied once by the
layout, not per cluster. Corner clusters hang *inside* the inset, and the bottom row's baseline
sits on it rather than past it.

```
┌─ 28px safe area ────────────────────────────────────────┐
│ OPRA                                      T+ 04:12:09 ● │
│ SR-084  Resolve and recover                             │
│ ────────────────────                                    │
│                                                         │
│                    ╭─────────╮                          │
│                  ╱   collar    ╲                        │
│                 │      ◈        │  ← ship, marks on the │
│                  ╲   31.7 m/s  ╱     bearing ring       │
│                    ╰─────────╯                          │
│                                                         │
│ hull ▂▂▂▂▂▂▂▂▁▁                          1.67 g         │
│ prop ▂▂▂▂▂▂▂▁▁▁                            006°         │
│ heat ▂▁▁▁▁▁▁▁▁▁                                         │
│ Kestrel        assist F   kill X                        │
└─────────────────────────────────────────────────────────┘
```

**Docking overlay** replaces the collar inside 200 m of a target port — a corridor ladder seen
down the axis, with the four gate terms as live bars that sit at zero when in tolerance:

```
        │         ·         │        lateral  2.1 m   ✓
        │      ╭──┼──╮      │        closing  0.8 m/s ✓
        │      │  ◈  │      │        align    17°     ✗
        │      ╰──┼──╯      │        rate     1.1°/s  ✓
        │         ·         │
     ───┴───────────────────┴───     Wayfarer A   38 m
```

Out-of-tolerance terms in `threat`; in tolerance in `nav` at dormant weight. The reader's eye goes
to the one thing that is wrong, and nothing else moves.

**System map** — orrery left, ephemeris right. The table is the almanac: hairline rule above and
below the header, figures right-aligned, no vertical rules, no zebra striping.

```
┌───────────────────────────────────────┬───────────────────────────┐
│                                       │  body       a/Gm      ν   │
│            ╭───────────────╮          │  ───────────────────────  │
│        ╭───┼────────╮      │          │  Cinder      7.18    142° │
│    ╭───┼───┼──╮     │      │          │  Tessera    17.20     87° │
│    │   │ ☉ │  │     │      │          │   Vesk       0.05    310° │
│    ╰───┼───┼──╯     │      │          │  Halberd    43.38    201° │
│        ╰───┼────────╯      │          │  ───────────────────────  │
│            ╰───────────────╯          │  Kestrel                  │
│                  ◈                    │   r         26.14 Gm      │
│                  ┄┄┄┄ transfer        │   v          31.4 km/s    │
│                                       │   Δv          2.41 km/s   │
│  Tessera → Halberd                    │   arrival    19 d 04 h    │
│  window opens in 3 d 11 h             │   window     3 d 11 h     │
└───────────────────────────────────────┴───────────────────────────┘
```

Bodies are drawn at a minimum glyph size — a planet at true scale is sub-pixel next to its own
orbit, and pretending otherwise is a lie. True radii live in the table, and a `true scale` toggle
shows honestly how empty it is.

**Startup** — the live orrery turning behind the title plate. One orchestrated moment: the rings
draw in once over ~1.2 s, then settle and stay. No looping motion, no per-item entrances.

```
              ·                     ○ Halberd
        ○ Cinder            ·
                    ☉
                            ○ Tessera
              ·                  ·

   O P R A
   ───────────────────────────────────
   Nereid recovery service
   Kestrel-class licence, third renewal

   continue          T+ 04:12:09, docked at Wayfarer
   new contract
   hangar
   settings
```

Left-aligned throughout. Copy is plain and active: `continue`, not `Continue Game`; `new
contract`, not `Start New Mission`. The status line states a fact rather than selling one.

### 4.5 Effects

`blast`, `rcs`, `flame`, `impact` all route through one additive-particle path — the renderer
already has the additive pass from PLAN-01 P4.

| Effect | Trigger | Form |
|---|---|---|
| RCS | `state.rcsActive`, per jet | The `rcs-jet` cones already in every sidecar, opacity from commanded torque. **Currently exported and never drawn** — wire the per-jet transforms from the sim's actual torque solution, not a global flag. |
| Flame | `thrustLevel > 0` | Existing, length scaled by thrust. |
| Impact | A contact with MTV > threshold | Short spark burst at the contact point, magnitude from relative speed. |
| Blast | Rock fracture | Radial dust puff + fragment trails, ~0.8 s. |
| Dock | Capture | A single confirming pulse on the port ring. Nothing more. |

RCS is the detail that sells the flight model, because it is the only visible evidence the ship is
solving a torque problem. Each of the four jets gets its own opacity from its own contribution.

---

## 5. Data formats

### 5.1 System — `assets/systems/nereid.json`

```json
{
  "name": "Nereid",
  "epoch": 0,
  "star": { "name": "Nereid", "mu": 2.07e19, "radius": 3.4e8, "color": "#ff9c5c" },
  "bodies": [
    { "id": "cinder",  "name": "Cinder",  "parent": "nereid",
      "mu": 2.2e12, "radius": 4.1e6, "model": "planet_rock",
      "a": 7.18e9, "e": 0.011, "omega": 0.42, "M0": 2.48 },
    { "id": "tessera", "name": "Tessera", "parent": "nereid",
      "mu": 1.9e13, "radius": 6.3e6, "model": "planet_temperate",
      "a": 1.720e10, "e": 0.034, "omega": 1.97, "M0": 1.52 },
    { "id": "vesk",    "name": "Vesk",    "parent": "tessera",
      "mu": 8.1e10, "radius": 1.4e6, "model": "moon_grey",
      "a": 5.2e7, "e": 0.002, "omega": 0.0, "M0": 5.41 },
    { "id": "halberd", "name": "Halberd", "parent": "nereid",
      "mu": 3.4e13, "radius": 8.9e6, "model": "planet_ice",
      "a": 4.338e10, "e": 0.091, "omega": 3.31, "M0": 3.51 }
  ],
  "belt": { "id": "drift", "parent": "nereid", "inner": 2.46e10, "outer": 2.89e10,
            "count": 4200, "seed": 4712 },
  "stations": [
    { "id": "wayfarer", "name": "Wayfarer", "parent": "nereid", "model": "station",
      "a": 2.61e10, "e": 0.004, "omega": 0.9, "M0": 0.0 }
  ],
  "jump_links": []
}
```

`jump_links` is present and empty from day one (**E4**). Nothing reads it yet.

Loading validates: every `parent` resolves, no cycles, `a > parent.radius`, `0 ≤ e < 1`, every
`model` exists in `assets/models.json`. A failure names the field and the file and is fatal —
a silently half-loaded system is worse than no system.

### 5.2 Sidecar additions

The exporter (`tools/export.mjs`) gains two outputs per model:

```json
"collider": { "shapes": [
    { "kind": "box",    "pos": [0, -2],    "angle": 0,     "half": [26, 8] },
    { "kind": "box",    "pos": [-18, -8],  "angle": -0.25, "half": [10, 15] },
    { "kind": "circle", "pos": [0, 34],    "radius": 7 }
  ], "bounds_radius": 54 },
"ports": [
    { "id": "A", "pos": [0, 148], "normal": [0, 1], "class": "M" }
  ]
```

Ports come from `Object3D`s named `dock.<id>` in the model source — the hardpoint convention
PLAN-01 §3.1 already established and nothing has used yet. `normal` is the object's local +Y
rotated by its own quaternion.

The old scalar `collider: {halfLength, halfWidth}` stays in the sidecar for one release so
nothing breaks mid-migration, then goes.

---

## 6. Phases

Each is one agent's work package. **D0 is a hard gate — nothing else starts until it is green.**
After that, the dependency column is the only ordering constraint; independent rows can run in
parallel.

| P | Name | Depends | Agent-sized deliverable |
|---|---|---|---|
| **D0** | Defect sweep | — | D-1..D-4, D-7, D-8, D-11..D-19 fixed, 3 new selftest cases. **D-5, D-6, D-9, D-10 are text-engine work and belong to P3** — leave `text.cpp` alone. |
| **P1** | `orbit/` library | D0 | Kepler, elements, SOI, Hohmann + windows. Pure math, ~40 selftest cases, no SDL. |
| **P2** | Camera + backdrop | D0 | Perspective, reversed-Z, orbit constraint, render-origin rebase, four parallax layers. All consumers of `project`/`unproject` updated. |
| **P3** | Text engine | D0 | `TTF_GPUTextEngine`, indexed UI path, `TTF_Text` cache, shipped fonts, tabular-figure assertion. Deletes ~200 lines. |
| **P4** | Colliders + RCS | D0 | Compound shapes, exporter generation, single-MTV resolution, per-jet RCS from the torque solution. |
| **P5** | System + world | P1 | `SystemDef` loader, registry, body propagation, belt generation, one continuous space, render-origin integration. |
| **P6** | Warp + flight | P5, P2 | Powered/coasting/railed regimes, the warp rail, auto-drop rules, gravity in `step_ship`. |
| **P7** | Transfers + planner | P1, P6 | Maneuver nodes, predicted conic, drag-to-plan, warp-to-node. Lambert lands here. |
| **P8** | Docking | P4, P5 | Ports, gates, capture spline, undock, the docking overlay. |
| **P9** | Design system | P3 | `tokens.h`, `table.h`, HUD re-laid-out against tokens, safe area, D-18. |
| **P10** | Orrery + map screen | P5, P9 | The 3D orrery renderer and the map screen. Shared with P11. |
| **P11** | Startup | P10 | Title plate over the live orrery, entry list, the one load moment. |
| **P12** | Effects | P4 | Impact, blast, dock pulse on the existing additive path. |
| **P13** | Modular ship data model | P4, P8 | **Data model only, no editor.** §7. |

### Hand-off contract

Every package is handed to an agent with: this file, the phase's row, and these standing rules.

1. **Read before you write.** Trace the existing flow end to end. The module boundaries in §2 are
   not suggestions — a cross-layer include is a rejected PR.
2. **One package, one PR.** Do not fix an adjacent thing you noticed. File it.
3. **Numbers, not adjectives.** Every performance claim carries before/after instance, triangle
   and submit counts from `--debug`.
4. **A runnable check per non-trivial unit.** `selftest.cpp` cases, assert-based, no framework.
   Math in `orbit/` needs the most: closure, round-trip, energy conservation, known-value cases
   against hand-computed results in the file's comments.
5. **Screenshots for anything visual**, captured with `tools/shots.ps1`, attached.
6. **`ponytail:` comments** on deliberate shortcuts, naming the ceiling and the upgrade path.

### Deliberate non-goals

Stated so no agent builds them: no combat, no contracts beyond the existing placeholder, no ally
or hostile AI, no shipyard **editor** (the data model only, P13), no FTL, no multiplayer, no
second system, no atmospheric flight, no landing. Aerobraking and surface operations are not in
scope even where the orbital math would support them.

---

## 7. Modular ships — the data model only

P13 lays the foundation and stops. The editor is a later plan. What it must establish:

```cpp
struct Component {
    std::string id;        // "drive.main", "tank.port", "hab.core"
    std::string model;     // a model name; carries its own mesh, collider shapes and ports
    Vec2  mount;           // attachment point in the parent's frame
    Real  angle;
    // everything the sim reads is derived from the component set, never authored per ship:
    Real  dry_mass, propellant, thrust, torque, heat_capacity, cooling;
};
struct ShipDesign {
    std::string name;
    std::vector<Component> components;
};
```

`ShipSpec` — today a hardcoded table of three — becomes a **function of the component set**:

```
mass    = Σ dry_mass
thrust  = Σ thrust of drives whose axis is within 15° of +Y
torque  = Σ (thrust × moment arm) for RCS blocks, about the computed centre of mass
fuel    = Σ propellant
cooling = Σ cooling
collider = concat of every component's shapes, transformed to the ship frame   (P4 gives this free)
ports    = concat of every component's ports                                   (P8 gives this free)
```

The three stock hulls are then expressed as component sets and must reproduce today's
`SHIPS[3]` table **within 2%** — that is the acceptance test, and it is what proves the derivation
is right before any editor exists.

This is why E8 and E9 chose per-component colliders and model-borne ports: both systems are built
once, in P4 and P8, and the modular work inherits them instead of replacing them.

---

## 8. Review gates

1. Clean `/W4` Release build, no new warnings.
2. `Opra.exe --selftest` exit 0.
3. `grep -rn 'include "sim/' src/render src/gpu src/core src/orbit` empty; `grep -rn 'include' src/orbit` shows only `<cmath>`, `<vector>` and `glm`.
4. No `float` holding a world position outside `render/scene.cpp`.
5. No file over ~600 lines.
6. Screenshots for every visual change, un-flipped, via `tools/shots.ps1`.
7. Orbital math: energy and angular momentum conserved to 1e-10 relative over 10⁴ propagations;
   elements → state → elements closes to 1e-9.
8. Nothing from the §6 non-goals list appears.
