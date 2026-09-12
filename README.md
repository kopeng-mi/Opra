# Opra — build, run, and the model loop

An Expanse-flavoured flight and salvage game: a Newtonian plane in the Nereid recovery zone, a
co-orbiting debris cluster inside a real star system, rendered with SDL3's GPU API, with every
ship, station and rock exported from three.js sources.

The sector is one continuous space. The ship flies in the zone's frame — the frame of Wayfarer,
the station it co-orbits — while the system itself is Gm-scale: bodies on analytic Kepler rails,
a real belt ring, and a transfer from one orbit to another measured in days. Time warp rides the
ship's own conic above 10x, so a warp step is exact rather than a bigger integration step.

## Build

Requirements: Windows with the Windows SDK (for `dxc`), CMake 3.21+, Visual Studio 2022, and
vcpkg (the manifest mode pulls SDL3, SDL3_ttf, glm, cgltf and nlohmann-json on first configure).

```bat
git clone <this repo> && cd Opra
cmake -S . -B build -A x64 -DCMAKE_TOOLCHAIN_FILE=<vcpkg>/scripts/buildsystems/vcpkg.cmake
cmake --build build --config Release
build\Release\Opra.exe
```

Assets are read from `assets/` next to the executable when it is installed, and from the source
tree two levels up when running out of `build/<config>/`, so no copy step is needed in development.

## Flags

| Flag | Effect |
|---|---|
| `--selftest` | headless checks: determinism, fracture, collision, models, UI, settings, orbital math, the system, docking, warp, text, effects, the orrery |
| `--screenshot <file.bmp>` | simulate, render one frame, write a BMP, exit |
| `--seconds <n>` | seconds of scripted flight before the capture lands |
| `--view startup\|flight\|chart\|help\|cutter\|none\|pause\|settings\|models` | which screen to capture |
| `--zoom <n>` | starting camera scale (0.6 .. 3.0) |
| `--frames <n>` | run n frames then exit |
| `--hidden` | create the window hidden |
| `--debug` | GPU validation, the per-frame budget log, and a 1 Hz asset poll |
| `--dump-models` | print each model's parts, effects and bounds, plus the collider check |

Controls are in-game: `H` opens the manual, which is generated from `src/game/bindings.cpp`.
`F2` opens the model viewer, `F5` reloads every model from disk, `Esc` pauses, `,`/`.` step the
time warp down and up, `Ctrl` + mouse Y tilts the orbit camera between 15 and 90 degrees.

## Flight

| System | Where |
|---|---|
| Kepler propagation, spheres of influence, Hohmann, Lambert, maneuver nodes | `src/orbit/` — pure math, no SDL, only `<cmath>`, `<vector>` and `glm` |
| The system file, the belt, compound colliders, the docking gate | `src/sim/` — `assets/systems/nereid.json` names every body; nothing is hardcoded |
| Perspective camera, reversed-Z depth, the render origin, the parallax backdrop | `src/render/` |
| Time warp, the frame loop, capture, effects | `src/game/` |

Docking is gated on the corridor, the closing rate, the alignment and the rate of rotation, all of
which must hold for 0.4 s; the capture that follows has two seconds to reach hard dock along a
cubic Hermite in the port's frame, and the pilot's controls are out until `R` releases the ship.
Ports are hardpoints in the model (`dock.<id>`), so a modular hull brings its own.

## Authoring a model

One file per model in `tools/models/`, one contract:

```ts
import * as THREE from 'three';
/** World units are metres. Nose +Y, dorsal +Z, starboard +X. */
export function build(): THREE.Object3D { /* ... */ }
/** Optional. Copied into the sidecar JSON; `collider` overrides the derived box. */
export const meta = { name: 'kestrel', scale: 1.3, collider: { halfLength: 59, halfWidth: 34 } };
```

```bat
cd tools && bun install          :: once: three 0.183.2
bun tools/export.mjs --all       :: or: bun tools/export.mjs tools/models/foo.ts ../assets
```

The exporter merges every static mesh per material, writes `assets/<name>.glb`, a sidecar with the
AABB, lit AABB, the fitted compound collider, the docking ports, effect nodes, hardpoints and
counts, and rewrites `assets/models.json`. Two runs produce byte-identical output.

The loop: write `tools/models/foo.ts`, run the export, press `F5` in game (or run with `--debug`
and let the 1 Hz poll do it) and look at it in the model viewer (`F2`). No rebuild, no restart.

Conventions the exporter enforces and the loader relies on:

| Convention | Meaning |
|---|---|
| Axes | Nose +Y, dorsal +Z, starboard +X, metres. No axis conversion anywhere. |
| `mesh.userData.effect = true` | Flame, jet or glow: drawn only while firing, in the additive pass. |
| `Object3D` named `hp.<id>` | A hardpoint. Not drawn; its position goes in the sidecar. |
| `Object3D` named `dock.<id>` | A docking port: position, outward normal and size class, in the sidecar. |
| Material `color` | Becomes `baseColorFactor`, so instances can tint one mesh. |
| `geometry.attributes.color` | Preserved as `COLOR_0`. |
| No `CanvasTexture`, no DOM | The exporter is headless; canvas-mapped meshes are skipped with a warning. |

The collider in the sidecar is in **model-local units** (the game multiplies by the manifest scale).
A hull's collider is a *set* of boxes and circles, fitted per connected component: an irregular hull
stops registering hits in the gap between its parts, and resolution takes the deepest single MTV so
a contact is never counted twice.

## Layout

```
assets/systems   the star system's definition (bodies, belt, stations, jump_links)
assets/fonts     the shipped faces: Hydrogen Whiskey, Barlow Condensed, Barlow (E10)
src/core         file, logging, the shared Real/Vec2/Rng vocabulary
src/orbit        Kepler, conics, SOI, Hohmann, Lambert, maneuver nodes — pure math
src/gpu          device, dynamic and static uploads, the frame's submit budget
src/render       mesh and glTF loading, cameras, text on SDL_ttf's GPU engine, scene builder, draw
src/sim          the fixed-step Newtonian plane, the system, collision, docking, the components
src/game         app, bindings, settings, the frame loop, warp, capture, effects, scene assembly
src/ui           immediate-mode context, tokens, the almanac table, screens, menus, the title plate
src/hud          the flight HUD and the docking overlay, in the mark language
tools/           the three.js model sources and the glTF exporter
```

`src/render`, `src/gpu`, `src/core` and `src/orbit` never include the simulation: the only module
that knows both is `src/game/scene.cpp`.

## Captures

`tools/shots.ps1` renders every view into `artifacts/` as PNG, un-flipped:

```powershell
powershell -ExecutionPolicy Bypass -File tools/shots.ps1
```
