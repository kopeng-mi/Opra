# PLAN-10 — Foundation: module boundaries, cleanup, and the defect list

The first plan of the rebuild. Nothing here adds a feature. It makes the tree honest, draws the
module lines every later plan depends on, and fixes every defect visible in a single flight frame.

Execute this one first and in full. PLAN-11 through PLAN-16 assume its boundaries hold.

---

## 0. Where the code is

Facts established by reading the tree and by running the binary, not assumed.

| fact | where |
|---|---|
| `BlockLayouter::place` increments a cursor with no bound against the column height | `src/hud/blocks.cpp:69-83` |
| `check_block_layout` detects overlap and only logs it | `src/hud/hud.cpp:900` |
| Flight HUD paints collar, blocks, arc, range rings | `src/hud/hud.cpp` |
| `scene.cpp` builds the 3D draw list *and* the whole `HudFrame` | `src/game/scene.cpp`, 1413 lines |
| Camera look is a pure function of the drag from the press point | `src/game/camera_follow.cpp:60` |
| Right-drag pan writes into `FollowState::target`, the spring integrator | `src/game/frame.cpp:439` |
| Follow lead is velocity only, capped | `src/game/camera_follow.cpp:31` |
| A missing texture calls `fatal()` | `src/render/texture.cpp:49` |
| `ModelStore::model()` / `meta()` are fatal on a missing name | `src/render/gltf.h` |
| Rock tiles and the body-map manifest bypassed `asset_path()` | `src/render/renderer.cpp:542,1151,1162` — **fixed** |
| `artifacts/` holds 1616 files / 509 MB; 144 are tracked | `git ls-files artifacts` |
| `build/` holds 78 stray captures | `.gitignore` covers `*.bmp`, not `*.png` |
| `Program::evaluate` takes `const World&` — the director cannot write | `src/sim/program.h` |
| `SystemDef::jump_links` exists, is loaded, and nothing reads it | `src/sim/system.cpp:137` |

---

## 1. Decisions

Taken with the user. Settled — do not relitigate in review.

| id | decision | consequence |
|---|---|---|
| **F1** | **No engine rewrite.** `orbit/`, `gpu/`, `sim/system.cpp`, `sim/dock.*` and `ui/tokens.h` are correct and stay. The rebuild is components, UI, models and a new story layer. | Scope is bounded. A proposal that touches `orbit/` is out of scope. |
| **F2** | **Dependencies point one way** (§2). `sim/` never includes `ui/` or `render/`. `ui/` and `hud/` never write to `World`. | Enforced by a build gate, not by review. |
| **F3** | **`scene.cpp` splits in two.** The 3D draw list stays; `HudFrame` construction moves to `hud/frame.cpp`. | ~500 lines move. Mechanical, no behaviour change, one commit. |
| **F4** | **A missing asset is never fatal in a dev build.** It draws a named placeholder and logs once. | This class of bug made the binary un-runnable. It is not allowed to recur. |
| **F5** | **`warp` keeps its meaning: time acceleration.** The interstellar drive is `Jump`, in every symbol, string and file. | PLAN-13. Reusing the word would gut the time-warp rail. |
| **F6** | **`artifacts/` keeps only the 144 tracked goldens.** The other 1472 files are scratch, deleted, and ignored going forward. | 509 MB down to roughly 30 MB. |
| **F7** | **Every defect in §4 is fixed before any new system lands.** | The game must be assessable before it grows. |

---

## 2. Module boundaries

The target. An arrow means "may include from".

```
core/     units, rng, file, log, crash                    (no deps)
gpu/      device, buffers, pipelines                      (no deps)
orbit/    kepler, conic, transfer, encounter, lagrange    -> core
sim/      world, physics, component, system, dock,        -> core, orbit
          combat, terrain, descent, survey, program
story/    quality, storylet, contract, crew               -> core, sim (const only)   [PLAN-15]
render/   gltf, mesh, text, texture, renderer             -> core, gpu
ui/       tokens, draw, ui, screens, flow                 -> core, render (text only)
hud/      hud, frame, blocks, minimap, director           -> core, orbit, sim (const), ui
game/     app, loop, frame, input, settings, scene        -> everything
```

Three rules, and they are the whole of F2:

1. **`sim/` includes nothing from `ui/`, `hud/`, `render/` or `game/`.** The simulation does not know
   it is being drawn.
2. **`ui/` and `hud/` take `const World&` and return draw data.** They never hold a mutable
   reference to world state. `sim/program.h` already states this rule and enforces it by signature;
   it now applies to the whole of both directories.
3. **`story/` reads the world and writes only its own state.** A storylet may not move the ship.

### 2.1 The `scene.cpp` split (F3)

`src/game/scene.cpp` does two unrelated jobs in 1413 lines.

| moves to | what |
|---|---|
| `src/hud/frame.cpp` (new) | `build_hud_frame(const World&, const App&) -> HudFrame` — the marks loop through the minimap fill, roughly lines 830-1150 |
| `src/game/scene.cpp` (stays) | `add_design()`, `add_model()`, the instance list, effects, the backdrop |

No behaviour change. The gate is that the golden captures are byte-identical across this commit.

### 2.2 Enforcing F2

Add `tools/check_layers.mjs`, run from CMake as a pre-build step. It matches every
`#include "x/..."` against the table below and fails the build on a violation. Twenty lines of node,
and it makes the boundary real instead of aspirational.

```
core/*    may include: core/
gpu/*     may include: gpu/
orbit/*   may include: core/ orbit/
sim/*     may include: core/ orbit/ sim/
story/*   may include: core/ orbit/ sim/ story/
render/*  may include: core/ gpu/ render/
ui/*      may include: core/ render/text.h ui/
hud/*     may include: core/ orbit/ sim/ ui/ hud/
game/*    may include: anything
```

---

## 3. Cleanup

### 3.1 artifacts (F6)

1472 untracked scratch captures. Keep what `git ls-files artifacts` reports; delete the rest.

```sh
git ls-files artifacts > keep.txt
find artifacts -type f | grep -vxF -f keep.txt | xargs rm
rm keep.txt
```

Then extend `.gitignore`:

```
artifacts/**/*.bmp
artifacts/audit/
artifacts/*.txt
build/*.png
```

### 3.2 Dead code and assets

| item | why | action |
|---|---|---|
| `kestrel.glb`/`.ts`, `mule.*`, `needle.*` | PLAN-08 A8: the reference goes once the modular ship reproduces it. It has. | delete |
| `spine_truss_m` | dead under the chain architecture (PLAN-08 A1), not exported | delete from `tools/models/` |
| `PLAN.md` | duplicates PLAN-08 under a name that collides with `plan.md` on NTFS | fold into PLAN-08, delete |
| `orrery::build()` | lost its caller under PLAN-08 A4 | keep `sample_ring` / `sample_belt`, delete the rest |

**Note for any agent:** on Windows, `plan.md` and `PLAN.md` are the same file. New plans use the
`PLAN-NN-name.md` form.

### 3.3 gitignore gap

`*.bmp` is covered; `*.png` under `build/` is not, which is where the 78 strays came from.

---

## 4. The defect list

Every one verified by reading the code or by looking at a captured frame. Fix all of them here.
`D11` is already done.

| id | defect | root cause | fix |
|---|---|---|---|
| **D1** | `VESSEL` and `CIRCULARIZE` blocks overlap | `place()` has no bound against `column_.h`; it cannot fail | clamp to the column; return a zero-height rect the caller skips |
| **D2** | `TWR 533.93` | thrust divided by the zone frame tidal field | §5.1 — replace with `accel_g`; show TWR only against a real surface |
| **D3** | collar ring is a bubble around a smeared ship | `radius = shipRadiusPx + 40`, unclamped | `clamp(shipRadiusPx + 40, 90, 160)` |
| **D4** | two faint arcs cross the whole frame | system-scale conics drawn into flight glass with no zoom gate | fade by `zoom`, clip outside the collar |
| **D5** | `OPRA` and `SR-084 Resolve and recover` collide | two independent anchors, no shared layout | one top-strip layouter owning both |
| **D6** | camera snaps toward home on every drag start | `look_cone_step` **assigns** while `look_cone_release` eases the cone home between drags | store `cone_at_press`; add the drag to it |
| **D7** | right-drag pan is undone instantly; far pans hard-snap | pan writes `FollowState::target`; large offsets trip `snap_distance` | separate `pan_offset`, applied after the spring |
| **D8** | camera does not open up under thrust | `follow_goal` leads on velocity only | add the acceleration term |
| **D9** | one missing asset kills the boot | `texture.cpp:49` and `ModelStore::model()` call `fatal()` | dev build returns a magenta placeholder and logs once; release keeps fatal |
| **D10** | rocks draw untextured | tiles load now; the material to texture-index binding is the other half | bind in `load_rock_tiles`, assert 4 textures present |
| **D11** | binary exits code 1, silently, from `build/Release` | rock tiles and the body-map manifest bypassed `asset_path()` | **done** — `renderer.cpp:542,1151,1162` |

### 4.1 D1 in detail

```cpp
Rect BlockLayouter::place(const char *name, float height) {
    const float remaining = from_top_ ? (column_.y + column_.h) - cursor_
                                      : cursor_ - column_.y;
    if (remaining < MIN_BLOCK_H) return Rect{};        // caller skips a zero-height rect
    height = std::min(height, remaining);
    ...
}
```

Every `build_*_block` gains one line at the top: `if (at.h <= 0.0f) return;`. The HUD then degrades
by dropping its lowest-priority block instead of painting two on top of each other.

### 4.2 D6 in detail

The cone is absolute from the press point while the release is easing it home, so the two disagree
the moment a second drag starts. That is the visible jerk.

```cpp
// App state
glm::dvec2 look_cone{0.0};
glm::dvec2 look_cone_at_press{0.0};
std::optional<glm::vec2> look_anchor;

// on press
app.look_cone_at_press = app.look_cone;
app.look_anchor = input.pointer;

// while dragging
const glm::dvec2 drag = input.pointer - *app.look_anchor;
app.look_cone = look_cone_clamp(app.look_cone_at_press +
                                drag * (config::LOOK_RADIANS_PER_PIXEL * 900.0 / height));
```

`look_cone_step` takes the press-time cone as a new first argument. Its call sites in
`src/game/frame.cpp` follow.

### 4.3 D7 in detail

```cpp
// App state
glm::dvec2 pan_offset{0.0};

// input: never touches app.follow
if (input.right && glass) pan_offset -= glm::dvec2(delta_px) * metres_per_px;
// per frame
pan_offset *= std::exp(-dt / config::PAN_RELEASE_TAU);
// camera centre
centre = follow_step(app.follow, ship, ship_velocity, dt, params) + pan_offset;
```

The spring keeps its own state, the pan is a view offset, and `snap_distance` never sees it.

### 4.4 D8 in detail

```cpp
glm::dvec2 follow_goal(const glm::dvec2 &ship, const glm::dvec2 &velocity,
                       const glm::dvec2 &accel, const FollowParams &p) {
    const double t = p.lead_seconds;
    glm::dvec2 lead = velocity * t + accel * (0.5 * t * t);
    const double len = glm::length(lead);
    if (len > p.lead_max) lead *= p.lead_max / len;
    return ship + lead;
}
```

Under a burn from rest the frame now opens in the direction of thrust, which is the behaviour the
old velocity-only lead could not produce.

---

## 5. Numbers this plan corrects

### 5.1 TWR (D2)

TWR is only meaningful against a surface. In the zone frame the denominator is the tidal field, so
the readout is arithmetically correct and physically meaningless.

```
accel_g = thrust * throttle / mass_total / 9.80665
```

That is the number the crew feels and the headline for the whole game (PLAN-12). Show TWR only
when `world.primary >= 0 && world.air_altitude < ceiling`, against that body's real surface gravity:

```
g_surface = mu / (radius * radius)
TWR       = thrust / (mass_total * g_surface)
```

`src/game/scene.cpp:1010`'s `mdot` division goes with it: PLAN-12 owns the delta-v readout.

---

## 6. Gates

The plan is done when all of these pass.

| # | gate | how |
|---|---|---|
| G1 | `Opra.exe --selftest` passes from `build/Release` | existing harness |
| G2 | `--screenshot --view flight` writes a frame with zero SDL_Log warnings | capture harness |
| G3 | `check_block_layout` reports **0** violations at 1280x720, 1600x900, 1920x1080 | extend `selftest.cpp:800` to all three |
| G4 | the layer check passes | `tools/check_layers.mjs` |
| G5 | deleting any one file under `assets/textures/` still boots, with one log line | manual, dev build |
| G6 | drag, release, drag again produces no camera discontinuity | new test asserting `cone` is continuous across a press |
| G7 | a 2 km right-drag pan then release returns to the ship with no snap | new test on `pan_offset` decay |
| G8 | `du -sh artifacts` under 40 MB and `git status` clean | shell |
| G9 | golden captures byte-identical across the §2.1 split | `tools/shots.ps1 -Check` |

---

## 7. Files touched

```
new      src/hud/frame.cpp            HudFrame construction, moved out of scene.cpp
new      src/hud/frame.h
new      tools/check_layers.mjs       F2 enforcement
edit     src/hud/blocks.cpp           D1
edit     src/hud/hud.cpp              D1 skip-empty, D3, D4, D5
edit     src/game/camera_follow.*     D6, D8
edit     src/game/frame.cpp           D7
edit     src/game/app.h               pan_offset, look_cone_at_press
edit     src/game/scene.cpp           -500 lines (F3), D2
edit     src/render/texture.cpp       D9
edit     src/render/gltf.cpp          D9
edit     src/render/renderer.cpp      D10
edit     src/selftest.cpp             G3, G6, G7
edit     .gitignore                   3.1, 3.3
delete   assets/kestrel.*, mule.*, needle.*, PLAN.md, artifacts scratch
```
