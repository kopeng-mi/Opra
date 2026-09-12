> **Status: complete.** The live plan is [PLAN-05-legibility.md](PLAN-05-legibility.md).

# Opra — base engine plan

Target: SDL3 + SDL3_GPU (D3D12/DXIL), C++20, 3D render / 2D gameplay plane.
Source of the current code: a hand port of `AstraWars` (three.js). This plan replaces the
hand port of **geometry** with a real asset pipeline, splits `main.cpp` into modules, and
fixes the confirmed defects. **Gameplay scope is frozen** at what exists today.

## 0. Locked decisions

| # | Decision | Consequence |
|---|---|---|
| D1 | Models: three.js → **glTF/GLB** via a Node/bun exporter, loaded with **cgltf** | `tools/export.mjs`, `assets/*.glb`, `src/render/gltf.cpp`. Hand-written C++ ship builders are **deleted**. |
| D2 | Gameplay scope: **flight + models only** | No combat/contracts/ally/shipyard port. Do not add gameplay systems. Mining cutter, cargo recovery, chart, manual stay as-is. |
| D3 | UI: **in-house immediate mode**, extended with layout + hit-test | No Dear ImGui. One draw language for HUD and menus. |
| D4 | Platform: **Windows / DXIL only**, no D3D-isms in HLSL | SPIR-V stays a build-config change, not a rewrite. Keep shader compilation in `tools/`. |

Non-negotiable through every phase:
- `src/sim/*` numerics stay bit-identical to today. `Real` is `double`. No `float` creeps in.
- Nothing in `src/core`, `src/gpu`, `src/render`, `src/ui` may `#include "sim/..."` or
  `"game/..."`. Dependencies point one way: `game → {sim, render, ui} → gpu → core`.
  `src/ui/hud.*` is the one allowed exception and it takes a plain `HudFrame` POD, not a `World`.
- Every phase ends green: `cmake --build build --config Release` clean at `/W4`, and
  `Opra.exe --selftest` exits 0 (P0 creates it).

## 1. Target layout

```
tools/
  package.json            three (pinned), no bundler — bun runs .ts directly
  export.mjs              three.js module -> .glb + .json sidecar
  models/                 LLM-authored three.js model sources  <-- the input surface
    kestrel.ts mule.ts needle.ts station.ts beacon.ts derelict.ts cargo.ts ore.ts
assets/
  models.json             manifest: name -> glb, sidecar, default scale
  *.glb *.json            committed build output
shaders/ mesh.hlsl  ui.hlsl
src/
  core/     log.h  file.h/.cpp                      SDL_Log wrappers, fatal, asset paths
  gpu/      gpu.h/.cpp                              device, buffers, textures, pipelines, uploads
  render/   mesh.h/.cpp      primitive builders (asteroid + ore only after P3)
            model.h/.cpp     Model, MeshPart, MeshLibrary, ModelMeta
            gltf.h/.cpp      cgltf -> Model
            camera.h/.cpp    Camera, view_projection, project, unproject
            scene.h/.cpp     SceneBuilder, cull, sort, opaque/additive split
            text.h/.cpp      glyph atlas
            renderer.h/.cpp  owns GPU state; draw_scene + draw_ui
  ui/       draw.h/.cpp      UIBatch primitives (moved out of hud.cpp)
            ui.h/.cpp        immediate-mode core + widgets
            hud.h/.cpp       flight HUD (unchanged behaviour)
            screens.h/.cpp   chart, manual, pause, toasts, model viewer
  sim/      collision.h/.cpp  physics.h/.cpp  world.h/.cpp
  game/     app.h/.cpp  input.h/.cpp  config.h
  main.cpp                                          args, init, loop, shutdown. <=150 lines.
  selftest.cpp                                      --selftest asserts
```

## 2. Phases

Dependency order. P1/P2 can run in parallel with P3's exporter half.

| P | Name | Depends | Deliverable |
|---|---|---|---|
| P0 | Bug sweep + selftest | — | Confirmed defects fixed, `--selftest` exists |
| P1 | Module split | P0 | `main.cpp` <= 150 lines, layout above, zero behaviour change |
| P2 | GPU layer + frame upload | P1 | `gpu.h`, one submit per frame, persistent transfer buffers |
| P3 | Model pipeline | P0 | exporter, `assets/*.glb`, `gltf.cpp`, manifest, hot reload |
| P4 | Render quality | P2, P3 | per-pixel lighting, additive pass, MSAA, glyph atlas |
| P5 | Scene optimization | P2 | frustum cull, static star field, run reuse |
| P6 | UI core | P1 | layout/hit-test/widgets, pause + settings screens |
| P7 | Model viewer | P3, P6 | `--view models`, the acceptance test for D1 |
| P8 | Docs + capture harness | all | `README`, `tools/shots.ps1`, reviewed screenshots |

---

## P0 — Bug sweep + selftest

Confirmed defects. Each line is a real, reproduced fault, not a style note.

| ID | Site | Fault | Fix |
|---|---|---|---|
| B1 | `main.cpp:453` | `SDL_PIXELFORMAT_RGBA8888` is a **packed** format (bytes A,B,G,R LE) uploaded into `R8G8B8A8_UNORM`. Channels rotate: every string renders as an opaque cyan block with a white glyph. Visible in every screenshot. | `SDL_PIXELFORMAT_ABGR8888` |
| B2 | `main.cpp:745-746` | `fragments.push_back(f); grid.add(&fragments.back());` — the grid stores raw pointers into a growing `vector`. Past the `reserve(1024)` (`main.cpp:710`) every stored pointer dangles. ~2-3 fragments per fracture, never removed. | Store `int id` in the grid; `World` keeps `fragments` in a `std::deque` **or** index-based cells. Pick the deque: smallest diff. |
| B3 | `main.cpp:1041` `beam_target` | Linear scan over all `world.rocks` per frame, and fragments are never tested — fragments are unbreakable. | Walk the grid along the swept segment; include `world.fragments`. |
| B4 | `main.cpp:796-801` | Star field is re-seeded from `camera.eye` **every frame**, so stars re-randomize as the camera moves — the backdrop shimmers. | Generate once in world space at startup; store in `World` or a `StarField`. |
| B5 | `World::step` | `for (fragment : fragments) resolve_collision(...)` is O(n) over every fragment every step. | Query the grid once (B2 already puts fragments in it). |
| B6 | `main.cpp:1629,1655,1774` | Camera is computed **before** the physics steps, so it trails the ship by one frame at speed. | Step sim, then build the camera from the post-step state. |
| B7 | `physics.cpp:424-426` | `ore.erase(begin()+i)` inside a forward loop skips the next element. | Iterate backwards, or `std::erase_if`. Verify against the JS original before changing behaviour — if the JS has the same bug, **keep it** and comment it. |
| B8 | `main.cpp:149-160, 186-200` | `upload_into` allocates a transfer buffer and submits **its own command buffer** per call — 2 extra submits per frame (instances + UI). | P2 replaces this. In P0 just note it. |

`src/selftest.cpp`, run by `Opra.exe --selftest`, no window, no GPU. Asserts only — no framework:
1. **Determinism**: 600 steps of `World::step` with a fixed input; assert ship position/velocity/fuel match a hardcoded golden to 1e-9. Regenerate the golden only with a stated reason.
2. **Fracture**: break 200 rocks; assert no dangling grid entry (B2) and that fragment count is bounded.
3. **Collision**: `obb_circle_out` and `obb_obb_out` against hand-computed cases, including the just-touching and fully-contained edges.
4. **glTF** (after P3): load every `assets/*.glb`; assert part count > 0, all indices in range, AABB finite and within 10x the sidecar's.

Acceptance: B1–B7 fixed, `--selftest` exits 0, a fresh `--screenshot` shows readable text on transparent ground.

---

## P1 — Module split

Pure motion. **No behaviour change.** Verify by capturing `--screenshot` before and after and
diffing the BMPs byte-for-byte (they must be identical for `--view flight --seconds 6`).

Moves:
- `main.cpp` render helpers (`read_file`, `asset_path`, `make_shader`, `create_buffer`,
  `upload_into`, `upload_buffer`, `upload_texture`, `create_*_target`) → `gpu/`, `core/file`.
- `PipelineSet`, `create_mesh_pipeline`, `create_ui_pipeline`, `Renderer`, `TextEngine`,
  `GpuMesh`, `InstanceRun`, `upload_mesh_library` → `render/renderer`, `render/text`.
- `Camera`, `camera_for`, `unproject`, `view_projection`, `project` → `render/camera`.
- `SceneBuilder`, `build_scene`, `spin_about_z`, `ModelSet` → `render/scene`, `render/model`.
- `World`, `Contact` → `sim/world`.
- `Input`, `flight_input_from` → `game/input`.
- `build_chart`, `build_help`, `Toast` → `ui/screens`.
- `hud::push_*` primitives, `UIVertex`, `UIBatch`, `TextDraw` → `ui/draw`. `hud.h` keeps only
  `HudFrame`, `CollarMark`, `build_flight_hud`.
- `App`, `update_app`, `make_hud_frame` → `game/app`.
- `save_bmp`, the screenshot branch of `run()` → `game/capture.cpp` (still behind `--screenshot`).
- Tunables (`FLIGHT_HALF`, `SHIP_SCALE`, `ZOOM_*`, `CUTTER_*`, `TILT_*`) → `game/config.h`.

`main.cpp` keeps: arg parse, `SDL_Init`/`TTF_Init`, window+device creation, the frame loop,
shutdown. Nothing else.

Every `.h` gets a one-line purpose comment matching the style already in `sim/physics.h`.
CMake: keep listing sources explicitly, no `GLOB`.

---

## P2 — GPU layer + frame upload

`src/gpu/gpu.h` — the only file that names `SDL_GPU*` outside `render/`:

```cpp
namespace opra::gpu {
struct Device { SDL_GPUDevice* handle; SDL_Window* window; SDL_GPUTextureFormat swap_format; };
Device  create_device(SDL_Window*, bool debug);
void    destroy_device(Device&);

/** Vertex/index buffer that grows by 1.5x and keeps one transfer buffer for its lifetime. */
struct DynamicBuffer {
    SDL_GPUBuffer* buffer; SDL_GPUTransferBuffer* transfer;
    Uint32 capacity, size;
    void ensure(Device&, Uint32 bytes);
    /** Stages into the persistent transfer buffer and records the copy on `cmd`. */
    void write(Device&, SDL_GPUCommandBuffer* cmd, const void* data, Uint32 bytes);
};
SDL_GPUBuffer*  upload_static(Device&, SDL_GPUBufferUsageFlags, const void*, Uint32);
SDL_GPUTexture* upload_texture(Device&, Uint32 w, Uint32 h, const void* rgba);
SDL_GPUTexture* create_depth(Device&, Uint32 w, Uint32 h, SDL_GPUSampleCount);
SDL_GPUTexture* create_color(Device&, Uint32 w, Uint32 h, SDL_GPUTextureFormat, SDL_GPUSampleCount);
}
```

Fixes B8. `DynamicBuffer::write` maps with `cycle = true` so a write never stalls on the
in-flight frame, and records `SDL_UploadToGPUBuffer` on the **frame's own** command buffer in a
copy pass opened before the render pass. Result: **one** `SDL_SubmitGPUCommandBuffer` per frame.

Pipelines move to `render/renderer.cpp` and stay cached by swapchain format (the existing
`pipelines_for` is correct — keep it, it handles HDR/format changes on monitor switch).

Acceptance: one submit per frame (assert with a counter under `--debug`), identical screenshot.

---

## P3 — Model pipeline  ← the core of this plan

### 3.1 Authoring contract

An LLM writes one file per model in `tools/models/`. The **only** contract:

```ts
import * as THREE from 'three';
/** World units are metres. Nose +Y, dorsal +Z, starboard +X. */
export function build(): THREE.Object3D { ... }
/** Optional. Copied verbatim into the sidecar JSON. */
export const meta = { name: 'kestrel', collider: 'auto', scale: 1.3 };
```

Conventions the exporter enforces and the loader relies on:

| Convention | Meaning |
|---|---|
| **Axes: nose +Y, dorsal +Z** | Matches the existing `sim` frame and today's C++ builders. The exporter writes local coordinates unchanged — glTF is Y-up but performs **no** axis conversion on node transforms, so what three.js holds is what cgltf reads. Verified by the bounds dump in 3.5. |
| `mesh.userData.effect = true` | Engine flame, RCS jet, glow — drawn only while firing, in the additive pass. Survives as glTF `extras.effect`. |
| `mesh.visible = false` | Kept (`onlyVisible: false`). Effect geometry is authored hidden; visibility is a runtime decision. |
| `Object3D` named `hp.<id>` | A hardpoint/anchor. Not rendered; its world position+quaternion goes to the sidecar. |
| Material `color` | Becomes `baseColorFactor` → `MeshPart.color`, so instances can still tint. |
| `geometry.attributes.color` | Preserved as `COLOR_0` (asteroids need this). |
| No `CanvasTexture`, no DOM | The exporter is headless. Meshes carrying a canvas map are **skipped with a warning** — the AstraWars stencil label is the only casualty. Upgrade path: `@napi-rs/canvas`, noted, not built. |

### 3.2 Exporter — `tools/export.mjs`

`bun tools/export.mjs tools/models/kestrel.ts assets/` and `bun tools/export.mjs --all`.

Steps, in order:
1. `import(file)`, call `build()`, `root.updateWorldMatrix(true, true)`.
2. **Merge per material.** Walk meshes that are not `userData.effect`, have no canvas map and no
   transparency; group by material; `geometry.applyMatrix4(mesh.matrixWorld)` then
   `mergeGeometries`. This is exactly `fleet.ts:batchPlates`, done at build time.
   A 60-mesh Kestrel becomes ~7 merged meshes + 2 flames + 8 jets.
   *This step is what makes D1 compatible with an instanced renderer* — `three.BoxGeometry` is
   allocated fresh per call in the AstraWars sources, so glTF's own geometry dedup finds nothing.
3. `GLTFExporter().parseAsync(root, { binary: true, onlyVisible: false, includeCustomExtensions: false })`
   → `assets/<name>.glb`.
4. Sidecar `assets/<name>.json`:
   ```json
   { "name":"kestrel", "scale":1.3,
     "aabb":{"min":[-32.5,-41,-4],"max":[32.5,41,15]},
     "collider":{"halfLength":59,"halfWidth":34},
     "effects":[{"node":"flame","pos":[16,-42,0]},...],
     "hardpoints":{"gun.port":{"pos":[-12,8,13],"rot":[0,0,0,1]}},
     "stats":{ ...whatever `meta` carried... },
     "counts":{"meshes":9,"vertices":4210,"triangles":2680} }
   ```
   `collider` is `halfLength = (aabb.max.y-aabb.min.y)/2 * scale`, `halfWidth` likewise on X,
   unless `meta.collider` overrides. Today's `HULL_BOXES = {{59,34},{65,43},{62,27}}` are the
   reference values — **the exporter must reproduce them within 5%** or the port drifted.
5. Rewrite `assets/models.json` (the manifest).
6. Print a one-line report per model: name, merged meshes, tris, AABB, collider, skipped meshes.

`tools/package.json` pins `three` to the same version AstraWars uses (`^0.183.2`) so the
geometry generators match the reference. Run under `bun` (installed; runs `.ts` with no build).

### 3.3 Porting the AstraWars models

Copy, do not rewrite. `tools/models/` gets `models.ts` + `fleet.ts` from
`../AstraWars/src`, stripped to the geometry path:
- Drop `stencil()`/label planes (D1 canvas rule), `PointLight`, `STOCK_MOUNTS`/`buildGunMount`
  (D2: no combat).
- Keep `box`, `cylinder`, `hull` (ExtrudeGeometry), `plate`, `drive`, `thrusters`, `buildAsteroid`.
- Export one `build()` per class: `kestrel.ts`, `mule.ts`, `needle.ts`, plus `station.ts`,
  `beacon.ts`, `derelict.ts`, `cargo.ts`, `ore.ts` from `models.ts`.
- Mark flame cones and RCS jets `userData.effect = true` (fleet.ts already does).

Then **delete** `build_ship`, `build_station`, `build_beacon`, `build_derelict`, `build_cargo`,
`build_ore`, `add_plate`, `add_hull`, `add_drive`, `add_thrusters` and the `prism`/`torus`/
`annulus` primitives from `render/mesh.cpp`. What stays: `meshes::asteroid` and
`meshes::icosahedron` (procedural, seed-driven, 96 buckets — no reason to ship 96 GLBs) and
`meshes::cube` for the star field. `render/mesh.cpp` should drop from 593 to ~200 lines.

### 3.4 Loader — `src/render/gltf.h`

```cpp
namespace opra {
struct ModelMeta {
    std::string name;
    float scale = 1.0f;
    glm::vec3 aabb_min{0}, aabb_max{0};
    HullBoxes collider{};
    std::vector<std::pair<std::string, glm::vec3>> hardpoints;
};
/** Loads `<dir>/<name>.glb` into `library`, returns its parts. Sidecar fills `meta`. */
bool load_gltf(const std::string& path, MeshLibrary& library, Model& out, ModelMeta& meta);

/** Every model named by assets/models.json. Lookup by name; missing name is a fatal. */
class ModelStore {
public:
    void load(const std::string& manifest_path);
    const Model&     model(const std::string& name) const;
    const ModelMeta& meta(const std::string& name) const;
    MeshLibrary&     library();
    /** True if any .glb changed on disk since load; the caller re-uploads. */
    bool reload_if_stale();
};
}
```

Implementation notes the agent must honour:
- `cgltf_parse_file` → `cgltf_load_buffers` → `cgltf_validate`. Any failure is a **fatal with
  the file name**, never a silent empty model.
- Walk `scene->nodes` recursively, accumulating a `glm::mat4`. For each `node->mesh`, per
  primitive: read `POSITION`, `NORMAL`, `COLOR_0` (default white), indices (widen to `uint32`).
  Reject non-`triangles` primitive types with a warning.
- Decompose the accumulated matrix to T/R/S. The instance format is pos+quat+scale and the
  shader computes `rotate(pos * scale)`, which **cannot represent shear**. So: decompose, then
  recompose and compare against the original matrix; if it differs by more than 1e-4, bake the
  matrix into the vertices and emit an identity part. Do not skip this check.
- `MeshPart.color` = material `baseColorFactor.rgb`. `MeshPart.effect` = `extras.effect == true`.
- **Mesh dedup**: hash the final vertex+index bytes (FNV-1a); identical geometry across models
  shares one `MeshLibrary` slot. The station ring and the cargo crates will hit this.
- Normals: if `NORMAL` is absent, generate flat face normals the way `mesh.cpp:push_triangle`
  already does — do not leave them zero.

### 3.5 Verification

- `Opra.exe --dump-models` (already exists) prints per-model part count and local AABB.
  After the port, `kestrel/mule/needle` bounds must land within **5%** of today's C++ output,
  and the derived collider within 5% of `HULL_BOXES`. Record both tables in the PR.
- `--screenshot` at identical camera before/after: the ships must be recognisably the same
  hull. Exact pixels will differ (merged geometry, different tessellation order).
- selftest case 4 (see P0) covers every GLB on every run.

### 3.6 Hot reload

`F5` in `--debug` builds calls `ModelStore::reload_if_stale()` and re-runs
`upload_mesh_library`. mtime poll on the manifest's files, once a second, nothing fancier.
This is what makes "an LLM writes a ship, you look at it" a 5-second loop instead of a rebuild.

---

## P4 — Render quality

| Item | Change | Why |
|---|---|---|
| Per-pixel lighting | `mesh.hlsl`: pass world normal to PS, do N·L there. Add a fill light (opposite, 0.25) and a rim term `pow(1-saturate(dot(N,V)), 3)` in `NAV`. | Today it is one directional term computed **per vertex** (`mesh.hlsl` VSMain). Low-poly hulls read as flat dark blobs — see any current screenshot. |
| Additive pass | Split `InstanceRun` into opaque and effect lists in `SceneBuilder`. Second pipeline: `BLENDFACTOR_SRC_ALPHA`/`ONE`, depth test on, **depth write off**, drawn after opaque. | Flames, glow rings and glass carried the three.js look and are currently drawn opaque. |
| Flame shape | Port the `fleet.ts` exhaust fragment shader (`a = pow(1-uv.y, 1.65)`, edge term) into a 3rd tiny pipeline, or approximate with vertex alpha on the cone. Take the vertex-alpha version first. | Cheaper, no new UV plumbing. Upgrade if it looks wrong. |
| MSAA 4x | `SAMPLECOUNT_4` on both pipelines, an MSAA color target + resolve to swapchain. | Low-poly silhouettes on a near-black field alias badly. One-line pipeline change, ~15 lines of target plumbing. Gate behind `config.h`. |
| Glyph atlas | `render/text.cpp`: one **RGBA8** 1024² atlas per (face, px), ASCII 32..126 packed by a shelf packer, white RGB + coverage in A. Advance/kern from `TTF_GetGlyphMetrics`. | Today every distinct string is its own texture and its own `SDL_BindGPUFragmentSamplers` + draw (`main.cpp:1467-1500`), with a nuke-everything at 512 entries. The atlas makes **all** text one draw and needs **no shader change** — `ui.hlsl` already does `color * tex.Sample(...)`. |
| Font fallback | `C:\Windows\Fonts\bahnschrift.ttf` / `segoeui.ttf` are hardcoded (`main.cpp`). Ship the two fonts in `assets/fonts/`, fall back to the system path. | The exe is not portable today. |

Acceptance: side-by-side screenshots in the PR; text is one draw call (assert under `--debug`);
the ship is legible as a ship at default zoom.

---

## P5 — Scene optimization

Baseline today: **839 instances, 143 mesh runs, 290,788 triangles** for an empty 6-second
flight, and the mesh library is **275,706 vertices** (96 asteroid buckets dominate).

1. **Frustum cull.** The camera is orthographic top-down: the visible region is an
   axis-aligned rect in XY, `half_height * aspect` by `half_height`, centred on the ship, plus a
   margin of the largest object radius. Reject rocks/fragments/ore/cargo before `SceneBuilder::add`.
   At default zoom that is ~900 x 500 m out of a 5200 x 4200 m sector — expect an ~8x cut.
2. **Static star field** (also B4). Generate once, upload as its own instance buffer, never
   rebuild. Draw with the scene's other instances by keeping it at the head of the buffer.
3. **Sort cost.** `SceneBuilder::sorted` does a `stable_sort` over an index vector and copies
   every instance, every frame. Replace with a counting sort by mesh id: the id space is small
   and known (`library.size()`). O(n) and no comparator.
4. **Run reuse.** `runs` and the instance vector are rebuilt from scratch every frame — reserve
   them once on the `Renderer` and `clear()`, do not reallocate.
5. **Asteroid LOD.** `meshes::asteroid` uses icosahedron detail 3 above r=46 (~1280 tris each).
   Pick detail from **screen size**, not world radius: at zoom 0.6 a big rock is 40 px.
   Two buckets (detail 1 / detail 2-3), chosen per frame. Only if P5.1 is not enough.

Acceptance: instance count and triangle count logged per frame under `--debug`; report before/
after for `--seconds 6` and for a stress case (`--seconds 120`, after 50 fractures).
Target: < 250 instances and < 60k triangles at default zoom.

---

## P6 — UI core

`src/ui/ui.h`. Roughly 300 lines. Immediate mode, ids by string hash, one batch out.

```cpp
namespace opra::ui {
struct Rect { float x, y, w, h; bool contains(glm::vec2 p) const; };
struct Pointer { glm::vec2 at; bool down, pressed, released; float wheel; bool valid; };

struct Context {
    UIBatch batch;
    Pointer pointer;
    glm::vec2 screen;
    uint32_t hot = 0, active = 0;       // widget under the cursor / being dragged

    void begin(glm::vec2 screen, const Pointer&);
    void end();                          // clears hot if nothing claimed it

    // layout: a stack of rects, each `cut_*` consumes from its parent
    void  push(Rect); void pop();
    Rect  cut_top(float h);  Rect cut_bottom(float h);
    Rect  cut_left(float w); Rect cut_right(float w);
    Rect  inset(Rect, float) const;

    // widgets — all return the interaction, all draw in the HUD's mark language
    bool  button(const char* id, Rect, const char* label);
    bool  toggle(const char* id, Rect, const char* label, bool& value);
    bool  slider(const char* id, Rect, const char* label, float& value, float lo, float hi);
    void  label(Rect, const char* text, float px, TextAlign, glm::vec4, TextFace);
    bool  list(const char* id, Rect, const char* const* items, int count, int& selected);
};
}
```

Rules for the implementer:
- `hot` is set during the widget call when the rect contains the pointer; `active` latches on
  press and releases on release. A widget "fires" on release-inside. This is the standard
  IMGUI state machine — do not invent a new one.
- ids: FNV-1a of the id string. In a loop, salt with the index (`button(id, i)` overload).
- Drawing uses **only** `ui/draw.h` primitives and the `hud::` palette tokens. No new colours.
  The design rule from `PLAN-HUD.md` holds for menus too: marks, not surfaces — a button is a
  label plus a rule that brightens, not a filled pill.
- Keyboard: `Tab`/arrows move focus, `Enter` fires. Focus is one more id on the context.
- The context owns no GPU state. `Renderer::draw_ui(const UIBatch&)` already exists.

Screens moved onto it in this phase: **pause** (resume / settings / quit), **settings** (zoom
default, attitude assist, reduced motion, MSAA, show debug stats). Chart and manual stay as
static draws — they have no input, converting them buys nothing.

`game/input.cpp` grows a binding table (`Action -> SDL_Scancode[]`) so keys are data, and the
manual screen renders **from** that table instead of the hardcoded `Row rows[]` in
`build_help` — the manual can then never drift from the bindings.

Acceptance: pause menu navigable by mouse and keyboard; settings persist to
`%LOCALAPPDATA%/Opra/settings.ini` (plain `key=value`, 30 lines, no dep).

---

## P7 — Model viewer  ← the acceptance test for the whole model pipeline

`Opra.exe --view models`, or `F2` in-game. A screen, not a separate app.

- Left: a `ui::list` of every name in `assets/models.json`.
- Centre: the model on a turntable, orbit with drag, zoom with wheel, `G` toggles a 10 m grid,
  `N` toggles normals, `B` toggles the AABB + the derived collider box, `E` toggles effect parts.
- Right: the sidecar, rendered as text — part count, unique meshes, triangles, AABB, collider,
  hardpoints, and the **scale at which the sim uses it**.
- `F5` reloads from disk (P3.6).

This is the deliverable that proves D1: an LLM writes `tools/models/foo.ts`, runs
`bun tools/export.mjs --all`, presses F5, and sees the ship. If that loop is not smooth, P3 is
not done.

---

## P8 — Docs + capture harness

- `README.md`: build (vcpkg manifest + CMake), run, flags, the authoring contract from 3.1,
  and the one-paragraph model loop. Nothing else.
- `tools/shots.ps1`: renders `flight / chart / help / models / cutter / scene` to
  `artifacts/*.png`, converting BMP→PNG with `System.Drawing` **without flipping**.
  The existing `build/*.png` are vertically flipped by whatever produced them — the renderer is
  correct, the converter was not. Do not "fix" the renderer to match them.
- `vcpkg.json`: add `cgltf`.
- `.gitignore`: keep `assets/*.glb` **tracked** (they are build output, but they are the thing
  the game ships and the thing a reviewer diffs).

---

## 3. Review gates

I check each phase against these before the next one starts:

1. `cmake --build build --config Release` — clean, `/W4`, no new warnings.
2. `Opra.exe --selftest` — exit 0.
3. `Opra.exe --screenshot` for every view — attached to the PR, converted un-flipped.
4. The dependency rule from §0 holds. `grep -r 'include "sim/' src/render src/gpu src/core` is empty.
5. Line budget: no file over ~600 lines. `main.cpp` <= 150 after P1.
6. Numbers, not adjectives: every optimization claim comes with the before/after instance,
   triangle and submit counts from `--debug`.
7. Nothing outside the frozen scope (D2) appears. A PR that adds guns gets sent back.
