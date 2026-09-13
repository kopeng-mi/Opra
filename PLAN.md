# PLAN-08: Shipkit Engine Integration, Dynamic Shipyard & UI Vitality

> **Context & Dependencies:**
> - Follows `PLAN-07-shipkit.md`, `artifacts/yard/BUILD-REPORT.md`, `artifacts/yard/TASTE-PROFILE.md`, and `artifacts/yard/DESIGN-DIRECTION-v2.md`.
> - 15 production Three.js modules (5 per variant across Variants A, B, C) and 3 assembled designs authored in `artifacts/yard/`.
> - All parts authored to `prims.ts` primitives and palette; complexity $\ge 60$, section CV $\ge 0.20$, top material $\le 45\%$, silhouette IoU $< 0.70$.
> - This plan specifies the full pipeline: resolving bench/exporter paradoxes, exporting GLBs, integrating runtime modular composition into the engine, rebuilding the 3D Shipyard with live physics/specs, injecting UI vitality, and redesigning the startup screen.

---

## 0. Technical Audit & Resolution of Known Paradoxes

Before exporting any asset or touching the C++ engine, two contradictions identified in `artifacts/yard/BUILD-REPORT.md §4` must be resolved with exact rules.

### 0.1 Paradox A: Triangle Budget vs. `flange()`
- **Problem:** PLAN-07 §5 requires 120–400 triangles per part. PLAN-07 §3.1 mandates `flange(root, metal, [0,0,0], [0,-1,0])` on every part. The standard `flange()` in `prims.ts` consumes **920 triangles** (32-seg ring plate = 128, 32×8 torus = 512, eight 8-seg bolt bosses = 256, keys = 24). At home framing (3 px/m), a 2.5 m flange is ~7.5 pixels wide.
- **Resolution:**
  1. Author a dedicated low-poly `flange_module()` in `prims.ts`:
     - 12-segment ring plate: 24 tris
     - 12×4 torus collar: 96 tris
     - 6 hexagonal bolt bosses: 36 tris
     - Total: **156 triangles**.
  2. The gate audit in `tools/export.mjs` and `artifacts/yard/gates.mjs` measures `own_triangles = total_triangles - flange_triangles`.
  3. Combined with low-poly flange, total part triangles remain under 450 (180 flange + 270 body), keeping a 5-module ship strictly within 1,500–2,200 triangles in memory.

### 0.2 Paradox B: Minimum Feature Floor (1.5 m) vs. Module Legibility
- **Problem:** Plan 05 §3.2 specifies a 1.5 m minimum feature size, and `tools/export.mjs` inflates meshes smaller than 1.5 m via `enforce_min_feature`. On a 4 m module, this inflates radiator vanes, hazard plates, RCS nozzles, and antenna masts.
- **Resolution:**
  - In `tools/export.mjs`, add `meta.modular = true` handling:
    - For modular parts (`meta.modular: true`), enforce a sub-assembly feature floor of **0.25 m** (structural members) and allow functional elements (PDC barrels, antennas, nav lights) to retain scale without synthetic inflation.
    - Global 1.5 m floor applies exclusively to the **assembled design** during design-level auditing.

### 0.3 Paradox C: Flange Radius (2.5 m) on Low-Profile Hulls
- **Problem:** Variant C is 1.45–1.75 m deep; a 2.5 m circular flange protrudes 0.45 m above the deck and below the keel.
- **Resolution:**
  - Standardize `flange_profile`: when `meta.flange_type == "low_profile"`, the flange uses an obround/racetrack collar (2.4 m wide × 1.4 m high) with identical 4-bolt spacing along X. For circular modules (Variants A & B), use standard circular 2.5 m OD flange.

---

## 1. Master Task List (Todos)

```
[ ] TASK 1: Resolve Exporter & Prims Paradoxes
    [ ] 1.1 Add low-poly flange_module() (~156 tris) to tools/models/prims.ts
    [ ] 1.2 Update tools/export.mjs with modular: true floor bypass (0.25m)
    [ ] 1.3 Verify artifacts/yard/gates.mjs passes all 15 parts with new flange

[ ] TASK 2: Export Pipeline & Asset Manifest
    [ ] 2.1 Copy verified parts from artifacts/yard/parts/ to tools/models/
    [ ] 2.2 Copy assembled designs to assets/designs/{hammerhead.json, ingot.json, waverider.json}
    [ ] 2.3 Run tools/export.mjs to generate 15 part GLBs + 3 stock design GLBs + sidecar JSONs
    [ ] 2.4 Update assets/models.json manifest with all 15 parts and designs
    [ ] 2.5 Verify asset integrity in C++ engine via --debug model loader check

[ ] TASK 3: Runtime Modular Ship Composition (sim/ & render/)
    [ ] 3.1 Implement design-based collider concatenation in sim/component.cpp
    [ ] 3.2 Implement docking port and RCS jet concatenation from sidecars
    [ ] 3.3 Dynamic RCS allocator: extend rcsJet solving to N arbitrary placements
    [ ] 3.4 In src/game/scene.cpp, replace monolithic ship drawing with per-placement rendering
    [ ] 3.5 Implement single-ship LOD selection (LOD level chosen per ship, applied to all parts)
    [ ] 3.6 Add wreckage detachment physics: tangential explosion velocity v + omega x r

[ ] TASK 4: Rebuild the Shipyard Screen (src/ui/screens.cpp & src/ui/screens.h)
    [ ] 4.1 3D Turntable view: smooth damped OrbitControls (yaw, pitch, distance, focus)
    [ ] 4.2 Interactive station slot wireframes (Stations 0..4 at 4m pitch along Y)
    [ ] 4.3 Part palette with visual category tabs (Nose, Combat, Tanks, Utility, Drive, Pods)
    [ ] 4.4 Real-time derived physics calculations (Mass, COM, Thrust, TWR, Delta-V, Torque, Heat)
    [ ] 4.5 Launch Readiness verification gate (Drive check, TWR check, Overlap check)
    [ ] 4.6 Mirroring toggle for radial mounts (PDC, radiators, tanks)

[ ] TASK 5: UI Vitality & Polish ("A Bit of Life in UI")
    [ ] 5.1 Tactical oscilloscope/scanline ambient pulses in ui/tokens.h
    [ ] 5.2 Responsive interaction states: button hover borders, active glow pops, sound hooks
    [ ] 5.3 Animated numeric readouts (delta-V fill gauge, TWR safety bar, mass breakdown)
    [ ] 5.4 Toast notification stack improvements (smooth alpha fade, status iconography)

[ ] TASK 6: Startup Screen Overhaul ("Fix Glowing Orbit")
    [ ] 6.1 Tone down additive orrery drawing: subtle astronomical chart ink (VELLUM_RULE)
    [ ] 6.2 Replace harsh concentric circles with dashed/graded ephemeris rings
    [ ] 6.3 3D Diorama framing: showcase player's flagship docked at Wayfarer Station
    [ ] 6.4 Clean typographic layout: military almanac title plate with tactile [ BEGIN ] menu
```

---

## 2. Asset Export & Manifest Integration Logic

### 2.1 Sidecar JSON Schema Extension (PLAN-07 §6.1)
Each modular part GLB emitted by `tools/export.mjs` generates a companion `.json` sidecar. The sidecar structure must be rigorously formatted:

```json
{
  "name": "nose_hammerhead",
  "kind": "nose_command",
  "span": 1,
  "axial": true,
  "mass": 4.2,
  "dry_mass": 4.2,
  "propellant": 0.0,
  "thrust": 0.0,
  "cooling": 0.0,
  "heat_capacity": 60.0,
  "flange": {
    "pos": [0.0, 0.0, 0.0],
    "normal": [0.0, -1.0, 0.0],
    "radius": 1.25
  },
  "collider": {
    "shapes": [
      { "kind": "Box", "pos": [0.0, 2.0], "half": [2.6, 2.0], "angle": 0.0 },
      { "kind": "Box", "pos": [0.0, 0.9], "half": [1.7, 0.9], "angle": 0.0 }
    ],
    "bounds_radius": 2.8
  },
  "ports": [
    { "id": "A", "pos": [0.0, 1.2], "normal": [0.0, 1.0], "class": "M" }
  ],
  "hardpoints": {
    "pdc.fore": [1.4, 2.0, 0.8],
    "sensor.optics": [0.0, 3.8, 0.2]
  },
  "effects": [
    { "name": "rcs-jet", "pos": [2.4, 2.2, 0.0], "dir": [1.0, 0.0, 0.0] }
  ]
}
```

### 2.2 Stock Design Manifest (`assets/designs/*.json`)
Stock ship designs replace hardcoded single-mesh hulls (`kestrel`, `mule`, `needle`). Each design specifies its spine definition and placement list:

```json
{
  "name": "hammerhead_corvette",
  "class": "Kestrel",
  "spine": {
    "family": "Keel",
    "stations": 5,
    "pitch": 4.0,
    "half_width": 1.7,
    "recess": 0.0,
    "mass": 5.0
  },
  "placements": [
    { "part": "nose_hammerhead", "station": 0, "facing": "Fore", "axial": true, "stack_index": 0 },
    { "part": "section_combat_a", "station": 1, "facing": "Fore", "axial": true, "stack_index": 1 },
    { "part": "section_tank_saddle", "station": 2, "facing": "Fore", "axial": true, "stack_index": 2 },
    { "part": "section_radiator_wing", "station": 3, "facing": "Fore", "axial": true, "stack_index": 3 },
    { "part": "drive_twin_torch", "station": 4, "facing": "Aft", "axial": true, "stack_index": 0 }
  ]
}
```

---

## 3. Engine Architecture & Runtime Composition

### 3.1 Mathematical Foundation for Stacking & Placement Transforms
Every part is authored with attachment face at local $-Y$, flange at $(0, 0, 0)$, body extending along $+Y$.

#### Axial Placement ($f \in \{\text{Fore}, \text{Aft}\}$):
Given spine length $L = N \cdot p$, end cap distance is $d_{\text{axial}} = L/2$.
$$\vec{p} = \text{dir}(f) \cdot (d_{\text{axial}} + k_{\text{stack}} \cdot p)$$
$$\mathbf{q} = \mathbf{q}_{\text{face}}(f) \cdot \text{rotY}(\text{roll} \cdot \pi/2)$$

#### Radial Placement ($f \in \{\text{Starboard}, \text{Port}, \text{Dorsal}, \text{Ventral}\}$):
Station $i \in [0, N-1]$ along Y axis:
$$y_i = \left(\frac{N - 1}{2} - i\right) \cdot p$$
$$\vec{p} = (0, y_i, 0)^T + \text{dir}(f) \cdot (w_{\text{half}} - r_{\text{recess}})$$
$$\mathbf{q} = \mathbf{q}_{\text{face}}(f) \cdot \text{rotY}(\text{roll} \cdot \pi/2)$$

#### Thrust Direction Inversion:
A drive bolts to the aft face ($-\hat{Y}$), body extends $-Y$, exhaust nozzle points $-Y$. Thrust pushes $+Y$:
$$\vec{F}_{\text{thrust}} = -(\mathbf{q} \cdot \hat{Y})$$
Valid forward propulsion requires:
$$\vec{F}_{\text{thrust}} \cdot \hat{Y} \ge \cos(15^\circ)$$

### 3.2 Dynamic Collider & Port Concatenation (`src/sim/component.cpp`)
At design instantiation:
1. Initialize empty `world.ship.collider.shapes`.
2. For each placement $p$:
   - Fetch `ModelShape` array from part sidecar.
   - For each 2D shape (box with center $\vec{c}$, half-extents $\vec{h}$, angle $\theta$):
     - Transform center into ship-frame: $\vec{c}' = \mathbf{R}(\mathbf{q}) \cdot \vec{c} + \vec{p}_{\text{mount}}$.
     - Project into XY flight plane.
     - Append transformed shape to ship collider.
3. Compute ship broadphase bounds radius $R_{\text{bounds}} = \max_i (\|\vec{c}'_i\| + \|\vec{h}_i\|)$.
4. Concatenate docking ports: map local port IDs ("A", "B") to global design IDs ("P1", "P2").

### 3.3 Dynamic Multi-Jet RCS Allocation
Current engine has `Real rcsJet[4]` in `src/sim/physics.h`.
With modular ships, RCS quads can be mounted on wingtips, nose sponsons, or engine blocks.
- **RCS Solver Formulation:**
  For $M$ installed RCS nozzles at ship-relative positions $\vec{r}_j$ pointing along unit thrust vectors $\hat{n}_j$ ($j = 1 \dots M$):
  $$\tau_j = (\vec{r}_j - \vec{r}_{\text{COM}}) \times \hat{n}_j \cdot \hat{Z}$$
  $$F_{y, j} = \hat{n}_j \cdot \hat{Y}, \quad F_{x, j} = \hat{n}_j \cdot \hat{X}$$
  Given pilot desired control vector $\mathbf{u} = (\tau_{\text{cmd}}, F_{x, \text{cmd}}, F_{y, \text{cmd}})^T$, solve non-negative linear program or weighted least-squares:
  $$\min \sum_{j=1}^M a_j^2 \quad \text{s.t.} \quad \sum_{j=1}^M a_j \begin{pmatrix} \tau_j \\ F_{x,j} \\ F_{y,j} \end{pmatrix} = \mathbf{u}, \quad 0 \le a_j \le 1$$
- For performance, compute pseudoinverse matrix $\mathbf{P} \in \mathbb{R}^{M \times 3}$ at design compile time:
  $$\mathbf{a}_{\text{raw}} = \mathbf{P} \mathbf{u}, \quad a_j = \text{clamp}(a_{\text{raw}, j}, 0, 1)$$

### 3.4 Runtime Per-Placement Scene Drawing (`src/game/scene.cpp`)
Replace the single-instance ship draw call in `build_scene()`:
```cpp
// Determine single LOD tier for entire ship to prevent visual tearing
const int ship_lod = select(0, world.ship.position.x, world.ship.position.y,
                            static_cast<Real>(world.ship.bounds.halfLength));
if (ship_lod >= 1) {
    const glm::dvec3 ship_origin = relative(origin, world.ship.position.x, world.ship.position.y, 0.0);
    const glm::quat ship_rot = spin_about_z(static_cast<float>(world.ship.angle));

    for (size_t i = 0; i < design.placements.size(); ++i) {
        const auto &p = design.placements[i];
        if (p.destroyed) continue;

        const Mount m = mount_transform(design.spine, p);
        const glm::vec3 part_pos = ship_origin + ship_rot * glm::vec3(m.pos);
        const glm::quat part_rot = ship_rot * glm::quat(m.rot);

        const Model &part_model = lod_model(models, p.part, ship_lod);
        scene.add_model(part_model, part_pos, part_rot, 1.0f,
                        world.ship.thrustLevel > 0.02f,
                        ui::clamp01(thrust / 1.65f),
                        glm::vec3(1.0f),
                        get_placement_jets(world.ship, i));
    }
}
```

---

## 4. Shipyard Rebuild Specification

The Shipyard is rebuilt from a static placeholder into an interactive 3D assembly workbench.

```
┌─────────────────────────┬───────────────────────────────────┬────────────────────────┐
│ PARTS PALETTE           │ 3D TURNTABLE WORKBENCH            │ DERIVED SPECIFICATIONS │
│ ─────────────────────── │ ───────────────────────────────── │ ────────────────────── │
│ [A: Hammerhead]         │       ▲ Nose (Station 0)          │ DRY MASS:       42.5 t │
│ [B: Ingot Hauler]       │      ┌─┐                          │ FUEL CAPACITY:  38.0 t │
│ [C: Waverider]          │      │█│ nose_hammerhead          │ TOTAL MASS:     80.5 t │
│                         │      ├─┤                          │                        │
│ CATEGORIES:             │   ◄──│█│──► combat_a (PDCs)       │ THRUST:        1200 kN │
│ > Command / Nose        │      ├─┤                          │ TWR:             1.52g │
│ > Combat / Weapons      │  (O) │█│ (O) tank_saddle          │ DELTA-V:      4.1 km/s │
│ > Propellant Tanks      │      ├─┤                          │                        │
│ > Utility / Radiators   │ [===]│█│[===] rad_wings (5.8m)    │ HULL INTEGRITY:    480 │
│ > Propulsion            │      ├─┤                          │ RADIATOR COOL:   28 kW │
│                         │      │█│ twin_torch               │ REACTOR HEAT:    22 kW │
│ [x] Mirror Radial       │      ▼▼ Stern (Station 4)         │ THERMAL MARGIN:  +6 kW │
│ [x] Show Snap Flanges   │                                   │                        │
│                         │ [Drag to Orbit | Scroll to Zoom]  │ [✓] Drive Installed    │
│                         │ [Press 1-4 for Camera Presets]    │ [✓] TWR > 1.0          │
│                         │                                   │ [✓] RCS Balanced       │
├─────────────────────────┴───────────────────────────────────┴────────────────────────┤
│ [ < BACK TO CONTRACT ]                     [ LAUNCH VESSEL (READY) > ]                │
└──────────────────────────────────────────────────────────────────────────────────────┘
```

### 4.1 3D Turntable & Station Gizmos
- Turntable orbit around design bounding box center:
  - Yaw: free $360^\circ$ rotation with mouse drag.
  - Pitch: clamped between $-20^\circ$ and $+80^\circ$.
  - Distance: auto-framed to $1.4 \times$ design diagonal.
- Visual Station Rings:
  - Render semi-transparent cyan torus rings at empty stations $(y_0 \dots y_{N-1})$.
  - Hovering a ring highlights it in amber and displays slot connector class.
  - Clicking an occupied module selects it for inspection or removal.

### 4.2 Real-Time Physical Property Calculations
Calculated dynamically whenever a part is added, removed, or swapped:
1. **Total Dry Mass & Propellant:**
   $$M_{\text{dry}} = m_{\text{spine}} + \sum_{p} m_{\text{dry}, p}, \quad M_{\text{fuel}} = \sum_{p} m_{\text{fuel}, p}$$
   $$M_{\text{total}} = M_{\text{dry}} + M_{\text{fuel}}$$
2. **Center of Mass ($\vec{r}_{\text{COM}}$):**
   $$\vec{r}_{\text{COM}} = \frac{m_{\text{spine}} \vec{r}_{\text{spine}} + \sum_p (m_p \cdot \vec{p}_{\text{mount}})}{M_{\text{total}}}$$
3. **Thrust & TWR:**
   $$T = \sum_{p \in \text{drives}} T_p \cdot \max(0, \hat{n}_{\text{thrust}, p} \cdot \hat{Y})$$
   $$\text{TWR} = \frac{T}{M_{\text{total}} \cdot g_0}, \quad g_0 = 9.80665 \text{ m/s}^2$$
4. **Relativistic / Tsiolkovsky $\Delta V$:**
   $$\bar{I}_{sp} = \frac{\sum T_i}{\sum (T_i / I_{sp, i})}, \quad \Delta V = \bar{I}_{sp} \cdot g_0 \cdot \ln\left(\frac{M_{\text{total}}}{M_{\text{dry}}}\right)$$
5. **Thermal Equilibrium Margin:**
   $$Q_{\text{cool}} = q_{\text{spine}} + \sum_p q_{\text{cooling}, p}, \quad Q_{\text{heat}} = \sum_p q_{\text{heat}, p}$$
   $$\Delta Q = Q_{\text{cool}} - Q_{\text{heat}} \quad (\text{must be } \ge 0)$$

---

## 5. UI Vitality & Polish ("A Bit of Life in UI")

The current UI feels flat and inert. We infuse tactile vitality without compromising hard sci-fi legibility:

### 5.1 Subtle Ambient Dynamics
- **CRT Oscilloscope & Micro-Grid Rulings:**
  - Overlay subtle horizontal scanline rulings with $0.04$ alpha.
  - Inject micro-pulse modulation on borders: $\alpha(t) = \alpha_0 \cdot (0.95 + 0.05 \sin(2.4 t))$.
- **Interactive Element Elevation:**
  - On button hover: border shifts from `tokens::RULE` ($0.28$) to `tokens::NAV` ($0.85$), background brightens by $+8\%$ with crisp 1px corner tick marks.
  - On click / activate: 60ms micro-burst flash in `tokens::DRIVE` amber.

### 5.2 Animated Data Visualizations
- **$\Delta V$ & Fuel Gauge:**
  - Segmented fuel bar with animated fluid fill and reserve threshold indicator.
- **TWR Dynamic Arc:**
  - Radial or horizontal meter with safety threshold line at $1.0\text{g}$; turns threat coral below $1.0$.
- **Real-Time Stat Deltas:**
  - When hovering a part in the catalog, stat numbers display instant diffs with color cues: e.g., `MASS: 80.5 -> 84.7 t (+4.2)` in amber, `TWR: 1.52 -> 1.44` in dim etch.

---

## 6. Startup Screen Overhaul ("Fix Glowing Orbit")

### 6.1 Diagnosis of the "Crap Glowing Orbit"
In `src/game/frame.cpp` and `src/render/orrery.cpp`, the startup screen currently calls:
`orrery::build(app.scene, app.orrery_meshes, app.map_frame);`
- **What is wrong:**
  1. Mesh lines are drawn in the additive backdrop pass with unattenuated glow and harsh unit cubes, creating blinding white/cyan concentric rings that drown the screen.
  2. The camera looks straight down at an abstract line drawing with no depth or scale context.
  3. The title panel floats disconnectedly over the top-left corner.

### 6.2 The New Startup Scene Design
1. **Dramatic 3D Docking Bay / Space Diorama Framing:**
   - Instead of looking straight down at raw orbit lines, position camera at an oblique cinematic angle ($\text{pitch} \approx 28^\circ$).
   - Display the player's flagship (Hammerhead Corvette) berthing at Wayfarer Station in the foreground.
   - Soft directional star lighting with rim glow on the hull panels.
2. **Refined Ephemeris Rings (Astronomical Chart Style):**
   - Eliminate additive blooming. Draw orbit rings with muted `VELLUM_RULE` ($\alpha \approx 0.20$).
   - Use dashed styling for planetary orbits and subtle tick marks for true anomalies.
   - The star is a warm, deep amber sphere with atmospheric corona, not an over-bloomed white flash.
3. **Typography & Atmospheric Presentation:**
   - Left-aligned title block with military naval typography:
     ```
     O P R A
     OPERATIONAL PLANETARY RECOVERY AGENCY
     Nereid Sector Division • Almanac Year 2184
     ────────────────────────────────────────────────────────
     [ 1 ]  BEGIN RECOVERY CONTRACT  >
     [ 2 ]  SHIPYARD & OUTFITTING
     [ 3 ]  FLIGHT MANUAL & TELEMETRY
     [ 4 ]  SYSTEM CONFIGURATION
     [ 5 ]  EXIT TO TERMINAL
     ```
   - Subtle background dust motes drifting across the viewport at parallax depth.

---

## 7. Edge Cases & Defensive Rules

1. **Center of Mass Shift:**
   - As modules or radial pods are attached asymmetrically, $\vec{r}_{\text{COM}}$ shifts off the centerline. The RCS solver must rebalance automatically. If $\vec{r}_{\text{COM}}$ shifts $> 0.6 \text{ m}$ from the thrust line, the shipyard displays a visual warning: `"THRUST MISALIGNMENT: YAW BIAS DETECTED"`.
2. **Station Stacking Collisions:**
   - Occupancy grid test (`std::array<bool, 96> occupied`) must execute instantaneously on hover. If a part's station span would collide with an existing module, the ghost mesh turns semi-transparent red and placement is blocked.
3. **Missing Drives / Negative Net Thrust:**
   - A design without an aft-facing drive or with thrust pointing retrograde has launch disabled with explicit HUD message: `"LAUNCH REFUSED: NO FORWARD DRIVE INSTALLED"`.
4. **Wreckage Physics Stability:**
   - When a ship breaks apart, detached modules inherit $\vec{v} + \vec{\omega} \times \vec{r}$. If separation speed $s < 4 \text{ m/s}$, clamp to $4 \text{ m/s}$ to prevent parts colliding with each other on the spawn frame.
5. **Asset Hot-Reloading:**
   - When GLBs are re-exported while the engine is running, `app.reload_models_if_stale()` must re-bind mesh pointers and re-compute colliders without crashing active instances.

---

## 8. Verification & Acceptance Criteria

- **Bench Tests:** `bun run artifacts/yard/gates.mjs` passes all 15 parts and all 3 designs.
- **Export Verification:** 15 `.glb` files and 15 `.json` sidecars created in `assets/`; `assets/models.json` validates.
- **Engine Build:** `cmake --build build --config Release` compiles with 0 errors and 0 warnings.
- **Selftest Suite:** `./build/Release/opra --selftest` runs with 100% pass rate.
- **Visual Inspection:**
  - Launching `./build/Release/opra` displays the redesigned cinematic startup screen (no blinding glowing orbits).
  - Transitioning to Shipyard allows full 360° orbit around Variant A/B/C hulls with real-time derived stats.
  - Launching into flight renders the ship as a cohesive section-module assembly with functional RCS thruster cones.
