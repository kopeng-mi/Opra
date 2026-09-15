# AGENTS.md

Instructions and architectural invariants for agents working in the Opra codebase.

## Quick Reference

- **Build**: `cmake --build build --config Release`
- **Configure** (if clean): `cmake -S . -B build -A x64 -DCMAKE_TOOLCHAIN_FILE=C:/Users/raman/vcpkg/scripts/buildsystems/vcpkg.cmake`
- **Run selftest**: `./build/Release/Opra.exe --selftest` (runs ~4,100 headless checks in ~3s)
- **Headless screen capture**: `./build/Release/Opra.exe --view <view> --screenshot <file.bmp> --hidden`
- **Visual regression**: `powershell -ExecutionPolicy Bypass -File tools/shots.ps1 -Check` (re-bless with `-Bless`)
- **Export single model**: `bun tools/export.mjs tools/models/<name>.ts assets` (destination `assets` arg is mandatory)
- **Export all models**: `bun tools/export.mjs --all`

---

## Build & Toolchain Quirks

- **Windows only**: C++20 MSVC (`/W4 /permissive- /utf-8 /Zc:__cplusplus /MP`), Windows SDK `dxc.exe`, vcpkg manifest mode (`vcpkg.json`).
- **CMake source list**: `CMakeLists.txt` explicitly lists every `.cpp` file. There is no globbing. When adding a new `.cpp` file, you **must** add it to `CMakeLists.txt` under `add_executable(Opra ...)`.
- **Shaders**: HLSL compiled to DXIL at build time by `dxc.exe` via custom target `opra_shaders`. Output DXIL files are copied next to `Opra.exe` in `shaders/` post-build.
- **Single-header libraries**:
  - `cgltf.h`: `CGLTF_IMPLEMENTATION` lives strictly in `src/render/gltf.cpp`. Needs `#define _CRT_SECURE_NO_WARNINGS` before include.
  - `stb_image` / `stb_image_resize2`: implementation lives strictly in `src/render/texture.cpp`.
  - `nlohmann/json.hpp`: include as `<nlohmann/json.hpp>`.
  - GLM: requires `#define GLM_ENABLE_EXPERIMENTAL` for `gtx/` headers.

---

## Testing & Verification

1. **Selftest suite** (`./build/Release/Opra.exe --selftest`):
   - Fast, headless test suite verifying determinism, collision, orbits, UI, docking, warp, effects, etc.
   - **No single-test CLI filter**: `run_selftest()` in `src/selftest.cpp` runs all suites unconditionally. To run a single suite during development, temporarily comment out other suites in `src/selftest.cpp:run_selftest()`.
2. **Deterministic simulation**:
   - Simulation steps at a fixed 120 Hz (`dt = 1.0 / 120.0`, semi-implicit Euler).
   - `test_determinism()` in `src/selftest.cpp` asserts positions and velocities down to $10^{-9}$ against golden values.
3. **Screenshots & Visuals**:
   - Views: `startup`, `flight`, `chart`, `help`, `cutter`, `none`, `pause`, `settings`, `models`, `map`, `body`.
   - `tools/shots.ps1 -Check` tests perceptual 8x8 block differences against `tools/golden/`.
4. **Input tapes** (`python tools/drive_input.py [tape]`):
   - Runs deterministic 120 Hz scripted runs via `--tape <name> --json <path>` and compares checkpoint states and capture SHA256 hashes.
   - *Note*: `flow` tape in `src/game/tape.cpp` stops at `launch` checkpoint until updated for the startup contract screen flow.

---

## Architecture & Layer Boundaries

- **Strict isolation between Sim and Render**:
  - `src/orbit/`, `src/render/`, `src/gpu/`, and `src/core/` **must never include** `sim/` headers.
  - `src/game/scene.cpp` is the sole integration boundary that knows both simulation and rendering.
  - The UI (`src/ui/`) and HUD (`src/hud/`) consume plain frame data structs (`HudFrame`, `ChartFrame`), never `World` or simulation objects directly.
- **Large-scale astronomical coordinates (Floating Origin)**:
  - System is Gm-scale (Kepler rails, Nereid recovery zone). World positions are double-precision metres (`Real` = `double`, `Vec2` = `{double x, y}`).
  - In `src/game/scene.cpp: relative()`, origin subtraction is performed in `double` before casting to `float`: `float(world.x - origin.x)`. **Never** cast world coordinates to `float` before subtracting the render origin (floats quantize to kilometres at 26 Gm).
- **Screen Flow State Machine**:
  - Never toggle screens with ad-hoc booleans (e.g. no `app.map = !app.map`).
  - Screen transitions are data-driven via `FLOW[]` (key actions) and `MENU_FLOW[]` (menu actions) tables in `src/ui/flow.cpp`.
  - Stack-based: `Startup` -> `Contract` -> `Shipyard` -> `Flight`, with overlays (`Pause`, `Settings`, `Chart`, `Manual`) pushed onto the stack.
- **Asset path resolution**:
  - `asset_path()` in `src/core/file.cpp` resolves relative to the executable (`build/Release/assets/...`) or two levels up (`build/Release/../../assets/...`). Always pass paths as `"assets/..."` to `asset_path()`.

---

## Models & Three.js Pipeline

- **Coordinate conventions**:
  - Units are metres (1:1).
  - Model space: Nose is **+Y**, Dorsal is **+Z**, Starboard is **+X**.
  - Planar flight forward direction in 2D world space is `(-sin(angle), cos(angle))`.
- **Authoring & Exporting**:
  - Source models live in `tools/models/*.ts`.
  - Sidecars (`assets/<name>.json`) contain AABB, compound colliders, docking ports (`dock.<id>`), hardpoints (`hp.<id>`), and effects (`mesh.userData.effect = true`).
  - Single-file export: `bun tools/export.mjs tools/models/<name>.ts assets`.
  - Inspect GLB contents: `python tools/glb_info.py assets/<name>.glb`.
  - In-game reload: Press `F5` in-game to reload all models from disk, or pass `--debug` to enable 1 Hz disk polling without restarts.
- **Yard Workbench** (`artifacts/yard/`):
  - Scratch workbench for authoring modular ship parts before graduation to `tools/models/`.
  - Gate audits: `bun run gates.mjs <file.ts>` (complexity $\ge 60$, section CV $\ge 0.20$, top material $\le 45\%$), `bun run partgate.mjs parts/<name>.ts`, `bun run iou.mjs designs/*.ts`.

---

## UI & Design Tokens

- Immediate-mode UI (`src/ui/ui.h`): `ui::Context` does not retain GPU state; it fills a `UIBatch`.
- Tokens (`src/ui/tokens.h`):
  - Two materials: `FIELD` / `ETCH` (cool glass marks for flight HUD) and `PLATE` / `VELLUM` (warm ivory ink for chart and data screens).
  - Semantic inks: `NAV` (teal), `DRIVE` (amber), `THREAT` (coral).
  - 1px hairlines. Opacity conveys hierarchy (`LIVE` 1.0, `DORMANT` 0.55, `RULE` 0.28, `GRID` 0.12).
  - Spacing scale is strictly 4-based: `SPACE = {4, 8, 12, 20, 32, 52}`.
