# PLAN-09 — UX overhaul: flight collar, shipyard interaction, camera controls

## 0. Where the code is

| fact | where |
|---|---|
| Flight HUD: `build_flight_hud()` paints collar, blocks, arc | `src/hud/hud.cpp` |
| HUD frame data: `HudFrame` carries ship screen pos, heading, marks, etc | `src/hud/hud.h` |
| Bottom heading arc: `build_arc()` draws the shallow curve + ticks + marks | `src/hud/hud.cpp` (local fn) |
| Ship screen position: `frame.shipScreen` and `frame.shipRadiusPx` | `src/game/scene.cpp:833` |
| Shipyard UI: `build_shipyard()` draws catalogue, rings, stats | `src/ui/screens.cpp` |
| Shipyard 3D scene: `build_shipyard_scene()` adds design parts | `src/ui/screens.cpp` |
| Shipyard camera: `shipyard_camera()` computes turntable orbit | `src/ui/screens.cpp` |
| Shipyard input: turntable drag, wheel, presets 1-4 | `src/game/frame.cpp:232-278` |
| Flight camera: `camera_for()` → `look_eye()` → cone clamp | `src/game/app.cpp`, `src/game/camera_follow.cpp` |
| Flight input: wheel zoom, Ctrl+drag look, follow spring | `src/game/frame.cpp:308-416` |
| UI primitives: `push_arc`, `push_line`, `push_disc`, `push_text` | `src/ui/draw.h` |
| Design tokens: `ETCH`, `NAV`, `DRIVE`, `THREAT`, 1px hairlines | `src/ui/tokens.h` |
| Density system: `Density::One/Two/Three`, `density_at_least()` | `src/hud/hud.h` |
| Scene builder: `add_design()`, `add_model()` with tint/alpha | `src/game/scene.cpp:84` |
| Model store: `ModelStore::has()`, `model()`, `meta()` | `src/render/gltf.h` |
| Part table: `PartTable`, `PartSpec`, sidecar `ModelPart` | `src/sim/designs.h`, `src/render/gltf.h` |
| Pointer state: `ui::Pointer` has `.at`, `.down`, `.pressed`, `.right_down`, `.right_pressed`, `.wheel` | `src/ui/ui.h` |
| Input state: `Input` has `left`, `right`, `pressed()`, `pointer_delta` | `src/game/input.h` |

---

## 1. Decisions

Taken with the user, this session. Settled.

| id | decision | consequence |
|---|---|---|
| **U1** | **Bearing collar ring around ship.** A thin 1px arc circle centred on `shipScreen`, 360° ticks, cardinal labels, prograde/retrograde markers, target diamond, heading vector, thrust vector. | New `build_collar_ring()` in `hud.cpp`. |
| **U2** | **Ring radius = ship half-length in px + 40px.** Scales with zoom naturally; no clamp. | `radius = frame.shipRadiusPx + 40.0f` |
| **U3** | **Heading + thrust vector shown on the ring.** A short line from centre showing velocity direction (NAV), a second line showing thrust direction (DRIVE amber). The angle between them visualises how thrust is changing the heading. | Computed from `frame.velocityDir` and `frame.noseDir`. |
| **U4** | **Density 1 shows: collar ring + bottom arc + vessel block + speed/heading near ship.** Ring fades to 0.35 alpha at density 1, full at density 2+. | `density_at_least()` gate on alpha. |
| **U5** | **Thin hairline style, no glow.** 1px ring, 1.5px marks, existing palette. No bloom, no gradients, no alpha pulse. | Matches current HUD language exactly. |
| **U6** | **Shipyard: ghost preview + tooltip on hover.** When a part is held and a ring is hovered, the 3D scene draws the candidate part at 35% alpha using actual colours. The DERIVED panel shows stat deltas (e.g. "+7.5t mass", "+16t fuel"). | `build_shipyard_scene()` extended with ghost placement. |
| **U7** | **Shipyard: 3D hover preview of catalogue parts.** When hovering a part in the catalogue (no part held), the turntable shows that part alone, slowly rotating. | `ShipyardState::preview_part` field; scene draws it standalone. |
| **U8** | **Camera: L-drag orbit, R-drag pan, Wheel zoom, Middle-click reset.** Flight view: L-drag replaces Ctrl+drag for the look cone. Shipyard: L-drag orbits (already works), R-drag does nothing (no pan needed for turntable). | `src/game/frame.cpp` input rewiring. |

---

## 2. Flight Bearing Collar Ring

### 2.1 Geometry

The collar is a circle in screen space centred on `frame.shipScreen`.

```
radius = frame.shipRadiusPx + 40.0f
```

Drawn as a segmented arc (48 segments, full TAU), 1px `ETCH` at 0.45 alpha (density 1) or 0.65 alpha (density 2+).

### 2.2 Tick marks

Every 10° around the circle, a tick extending **outward** from the ring by 4px (minor) or 8px (major at 0°, 90°, 180°, 270°). Cardinal labels (N/E/S/W) at the four major ticks, 12px `ETCH_DIM`, 14px outward from the ring.

The tick at world-north (heading 0°) maps to the **top** of the ring. The ring rotates with the ship's heading so the ticks show absolute bearing: the ship's nose always points in `frame.headingDeg`, and the ring's zero sits at world north.

```cpp
// For a tick at world bearing `b` degrees:
float angle_on_ring = (90.0f - b) * DEG;  // 0=right, CCW positive
// The ring's internal convention: 0°=right, +90°=up, but screen y is down
// so the arc angle = -(90 - b) in screen-y-down coords
float screen_angle = -(90.0f - b) * DEG;
```

### 2.3 Marks on the ring

Each mark sits **on** the ring circumference at its bearing angle.

| mark | symbol | colour | condition |
|---|---|---|---|
| **Prograde** | small circle (r=3px) + dot | `NAV` | `frame.speed >= 0.05` |
| **Retrograde** | circle + cross | `NAV` at 0.6 alpha | `frame.speed >= 0.05` |
| **Target** | diamond (4 lines) | `NAV` | `frame.targetValid` |
| **Hostile** | chevron pointing inward | `THREAT` | any mark with `MarkKind::Hostile` |
| **Heading index** | filled triangle pointing outward at ship nose | `ETCH` at 0.8 | always |

### 2.4 Heading and thrust vectors from ship centre

Two lines emanate from `shipScreen` toward the ring edge:

1. **Velocity vector** (heading vector): a line from ship centre toward the prograde bearing, length = `radius * 0.6`, colour `NAV` at 0.5 alpha, 1.5px. Tip has a small arrowhead (two 4px lines at ±25°).

2. **Thrust vector**: a line from ship centre toward `noseDir` (thrust direction), length = `radius * 0.45 * frame.throttle`, colour `DRIVE` at 0.55 alpha, 1.5px. Only drawn when `frame.throttle > 0.01`.

The **angle between** these two lines is what the pilot reads: when they diverge, the ship is thrusting off its velocity vector. When they converge, the burn is prograde.

### 2.5 Speed/heading floating readouts

At density 1+, two small readouts near the ship:

- **Speed**: centred below the ring, `y = shipScreen.y + radius + 20`, 20px `ETCH` readout face, e.g. `124.3 m/s`.
- **Heading**: centred above the ring, `y = shipScreen.y - radius - 8`, 18px `ETCH` readout face, e.g. `047°`.

These duplicate what the bottom arc shows, but they're near the ship where the eye already is. At density 1 they're the only speed/heading readout visible without looking at the arc.

### 2.6 Integration with existing HUD

The collar ring is drawn in `build_flight_hud()` after the range rings and before the bottom arc. It respects:
- `frame.hideCollar`: skip entirely (chart, manual, cinematic).
- `frame.wrecked`: skip entirely.
- Zoom level: when `frame.zoom > 2000.0` (system scale), skip — the ship is a pixel; a ring is noise.

### 2.7 Math summary

```
vel_bearing_deg = atan2(velocityDir.x, -velocityDir.y) * 180/π  (0..360)
nose_bearing_deg = headingDeg  (already 0..360)

For mark at world bearing `b`:
  ring_angle = (90 - b) * DEG  // math convention: 0=east, CCW
  screen_x = centre.x + radius * cos(ring_angle)
  screen_y = centre.y - radius * sin(ring_angle)  // y-down
```

---

## 3. Shipyard interaction overhaul

### 3.1 Catalogue hover → 3D preview

**State change**: add `ShipyardState::preview_part` (string, empty = no preview).

When no part is held (`state.held == -1`) and the pointer hovers a catalogue row, set `preview_part` to that row's part id. `build_shipyard_scene()` checks: if `preview_part` is non-empty and the model exists, draw **only** that part at the origin, slowly auto-rotating (yaw += dt * 0.4).

The preview is a single instance at the origin, scale = `design.scale`, no jets. The turntable camera frames it using a smaller extent (the part's own AABB half-diagonal instead of the full chain's).

When the pointer leaves the catalogue panel or a part is held, clear `preview_part`.

### 3.2 Ghost preview on ring hover

When a part is held (`state.held >= 0`) and the pointer is near a mount ring (`best_slot >= 0`):

1. Compute the candidate `Placement` and its `mount_transform`.
2. In `build_shipyard_scene()`, draw the candidate part at mount position with `tint_alpha = 0.35`. The existing `add_model()` already takes a tint vec3 and an alpha vec4 — pass `glm::vec3(1.0f)` tint and `glm::vec4(0.35f)` for the ghost.
3. If mirror is on, also draw the mirrored ghost at 0.35 alpha.

**Wire**: `ShipyardState` gets `ghost_placement` (optional Placement) and `ghost_mirror` (optional Placement), set during `build_shipyard()`, read during `build_shipyard_scene()`.

### 3.3 Stat deltas on hover

When a ghost is showing, the DERIVED panel shows **delta** values in parentheses:

```
DRY MASS    82.0 t  (+7.5)
PROPELLANT  16.0 t  (+8.0)
THRUST      1600 kN (+0)
```

Implementation: when `ghost_placement` is set, compute `derive_spec()` for the design + the ghost placement(s), diff against `cached_spec`, and format the delta. The delta is drawn in `DRIVE` colour after the main value.

### 3.4 Ring visual improvements

Current rings are 24-segment circles with varying alpha. Improve:

- Green ring (`NAV` at 0.5) for valid mounts.
- Red ring (`THREAT` at 0.5) for invalid mounts (blocked by occupancy or mirror conflict).
- Hovered ring: thicker (2px) and brighter (0.9 alpha) with a label showing slot number.
- Non-hovered rings: 1px, 0.3 alpha.
- When no part is held: no rings shown (less clutter).

### 3.5 Part removal with right-click

Already works. Add: when hovering a placed part (no part held), show a tooltip with the part name and "right-click to remove" hint. Draw a thin `THREAT` ring around the hovered part.

---

## 4. Camera controls

### 4.1 Flight view — L-drag orbit (with 3px deadzone)

**Current**: Ctrl + left-drag swings the look cone. Bare left-drag does nothing for the camera (it goes to UI widgets).

**New**: Left-drag (when the pointer is not over any UI widget) directly swings the look cone. No Ctrl required. The drag sensitivity stays the same (`LOOK_RADIANS_PER_PIXEL`). Release eases back to home.

**3px deadzone**: On `left_pressed()`, record the anchor position. While held, if `|pointer - anchor| < 3px`, do nothing (it's a click, not a drag). Once the 3px threshold is crossed, activate look-cone drag. This prevents accidental camera swings when clicking HUD elements or empty space.

```cpp
// State in App:
std::optional<glm::vec2> drag_anchor;  // set on left_pressed, cleared on release
bool drag_active = false;               // true once 3px threshold crossed

// Per frame:
if (input.left_pressed() && in_flight_area) {
    drag_anchor = input.pointer;
    drag_active = false;
}
if (!input.left) { drag_anchor.reset(); drag_active = false; }
if (drag_anchor && !drag_active) {
    if (glm::length(input.pointer - *drag_anchor) >= 3.0f)
        drag_active = true;
}
const bool looking = drag_active || ctrl_held;  // Ctrl still works as alias
```

Keep Ctrl+drag as an alias so existing muscle memory works.

### 4.2 Flight view — R-drag pan

Right-drag pans the follow point in the world plane. The delta in pixels maps to a delta in world metres at the current zoom level:

```cpp
if (input.right && input.pointer_valid && !over_ui) {
    const double metres_per_px = app.camera.half_height * 2.0 / height;
    app.follow.target.x -= input.pointer_delta.x * metres_per_px;
    app.follow.target.y += input.pointer_delta.y * metres_per_px;  // screen y is down
}
```

The pan is temporary: releasing right-click lets the follow spring ease back to the ship. This is already how the follow works — the spring's goal tracks the ship, and a nudge to `follow.target` damps out.

### 4.3 Flight view — middle-click reset

Middle-click resets `camera_look` to zero and `half_height` to `HOME_HALF`, same as pressing `0`.

```cpp
if (input.middle_pressed()) {
    app.camera_look = glm::dvec2(0.0);
    app.half_height = HOME_HALF;
    app.follow_body = -1;
}
```

Requires adding `middle` button tracking to `Input` (SDL_BUTTON_MIDDLE).

### 4.4 Shipyard — L-drag orbit (already works)

L-drag in the middle area orbits the turntable. No change needed.

### 4.5 Shipyard — wheel zoom (already works)

Wheel zooms `distance_target`. No change needed.

### 4.6 Shipyard — middle-click reset to preset 1

Middle-click in the shipyard resets yaw/pitch/distance to preset 1 (0.6, 0.35, 1.0).

---

## 5. Input additions

### 5.1 Middle button

Add to `Input`:
```cpp
bool middle = false, previous_middle = false;
bool middle_pressed() const { return middle && !previous_middle; }
```

In `apply_event()`, handle `SDL_BUTTON_MIDDLE` like left/right.

### 5.2 Right-drag state

Add to `App` or track in frame.cpp:
```cpp
bool right_dragging = false;
glm::vec2 right_drag_anchor{0.0f};
```

Set `right_dragging = true` on `right_pressed()`, clear on `!right`. While dragging, compute delta from `input.pointer_delta`.

### 5.3 Pointer in flight area

A function `pointer_in_flight_area()` that returns false when the pointer is inside:
- The bottom arc scrim rect (already computed: `{W/2 - A - 40, y_apex - 12, (A+40)*2, 108}`).
- Any active block rect (the `BlockLayouter` tracks these, but they're not currently exposed to the input layer).

Simplest approach: the HUD blocks are in the four corners inside the safe inset. Check: pointer is not in `{0, 0, column_w + inset, height}` (left column) and not in `{width - column_w - inset, 0, column_w + inset, height}` (right column) and not in the bottom 120px strip.

---

## 6. Implementation phases

### P1: Input plumbing (middle button, right-drag, flight-area detection)
- Add middle button to `Input` and `apply_event()`.
- Add right-drag tracking in frame.cpp.
- Add `pointer_in_flight_area()` helper.
- Wire L-drag → look cone (replacing Ctrl requirement, keeping Ctrl as alias).
- Wire R-drag → pan follow point.
- Wire middle-click → reset.
- **Gate**: Ctrl+drag still works. L-drag orbits camera. R-drag pans. Middle resets. Selftest passes.

### P2: Bearing collar ring
- Implement `build_collar_ring()` in `hud.cpp`.
- Draw ring, ticks, cardinal labels, heading index triangle.
- Draw prograde/retrograde marks on ring.
- Draw target diamond on ring.
- Draw velocity vector line from centre.
- Draw thrust vector line from centre.
- Draw speed/heading readouts near ring.
- Integrate into `build_flight_hud()` with density and hideCollar gates.
- **Gate**: Ring visible in flight, ticks rotate with ship bearing, marks track correctly, fades at density 1. Selftest passes.

### P3: Shipyard ghost & catalogue preview
- Add `preview_part`, `ghost_placement`, `ghost_mirror` to `ShipyardState`.
- Implement catalogue hover → set `preview_part`.
- Implement ghost placement → set `ghost_placement` on ring hover.
- Modify `build_shipyard_scene()`: draw preview part standalone or ghost part at 35% alpha.
- Modify `shipyard_camera()`: use part AABB when previewing.
- **Gate**: Hovering catalogue shows part in 3D. Hovering ring shows ghost. Ghost disappears on click-to-mount. Selftest passes.

### P4: Shipyard stat deltas & ring polish
- Compute stat deltas when ghost is active.
- Draw delta values in DERIVED panel.
- Improve ring visuals: green/red colour, thickness on hover, slot labels.
- Add part-name tooltip on placed-part hover.
- Add "right-click to remove" hint.
- **Gate**: Deltas show correctly. Rings are clear green/red. Removal hint visible. Selftest passes.

### P5: Shipyard camera — middle-click reset, auto-rotate preview
- Middle-click resets turntable to preset 1.
- Preview part auto-rotates slowly.
- **Gate**: Middle-click works. Preview rotates. Selftest passes.

---

## 7. Files touched

| file | change |
|---|---|
| `src/game/input.h` | middle button fields |
| `src/game/input.cpp` | `apply_event()` middle button handling |
| `src/game/frame.cpp` | L-drag look, R-drag pan, middle reset, shipyard middle reset |
| `src/game/camera_follow.h` | (none expected) |
| `src/game/camera_follow.cpp` | (none expected) |
| `src/hud/hud.h` | (none expected — ring uses existing HudFrame fields) |
| `src/hud/hud.cpp` | `build_collar_ring()`, integrate into `build_flight_hud()` |
| `src/ui/screens.h` | `ShipyardState` additions: `preview_part`, `ghost_placement`, `preview_yaw` |
| `src/ui/screens.cpp` | catalogue hover, ghost drawing, stat deltas, ring polish |
| `src/game/scene.cpp` | (none expected — ghost drawn via existing `add_model`) |

---

## 8. What this plan does NOT do

- No new shaders or GPU state. The ghost is drawn with the existing model pipeline's alpha.
- No drag-and-drop in the shipyard. Click-to-hold, click-ring-to-place stays.
- No bloom, glow, or post-processing effects on the HUD.
- No 2D silhouettes in the catalogue list (the 3D preview replaces this need).
- No changes to the bottom arc heading tape — it stays as is, the collar ring is additive.
- No changes to the design format, part specs, or assembly logic.
