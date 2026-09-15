# PLAN-14 — UI: three registers, a data-driven HUD, and the node planner

Depends on PLAN-10 (layout bounds, the `scene.cpp` split) and PLAN-12 (the numbers to show).

Two goals. Make the flight view readable. Make the whole HUD **data**, so the storyline can change
the interface without a rebuild.

---

## 0. Where the code is

| fact | where |
|---|---|
| Design tokens: two materials, one darkness, opacity as the second axis | `src/ui/tokens.h` |
| `ui::Context` has **no clock** — no widget can animate | `src/ui/ui.h:44` (a `now` is passed to `begin` but unused by widgets) |
| `build_flight_hud` hardcodes which blocks appear and in what order | `src/hud/hud.cpp:799-855` |
| `BlockLayouter` places blocks down a column | `src/hud/blocks.cpp` |
| Density system: `Density::One/Two/Three`, `density_at_least()` | `src/hud/hud.h` |
| UI primitives: `push_arc`, `push_line`, `push_disc`, `push_text`, `push_rect` | `src/ui/draw.h` |
| `orbit::Node{t, prograde, radial}` and `planned_conic()` exist; no UI places a node | `src/orbit/transfer.h`, `src/sim/world.h` |
| Shipyard turntable, catalogue, ghost preview, stat deltas | `src/ui/screens.cpp`, PLAN-09 §3 |
| `HudFrame` is a pure copy-out of the world | `src/hud/hud.h` |

---

## 1. Decisions

| id | decision | consequence |
|---|---|---|
| **U1** | **Three registers: GLASS, CHART, COMPARTMENT.** The first two exist and are correct. | §2. `tokens.h` gains the third; nothing existing changes. |
| **U2** | **The flight HUD is composed from `assets/hud/flight.json`.** Blocks, rails, order and density thresholds are data. | §5. This is the "UI changes with the storyline" requirement. |
| **U3** | **`ui::Context` exposes a clock and one easing helper.** Widgets may animate; nothing is required to. | §2.3 |
| **U4** | **The BURN bar is the centrepiece of the flight view.** Bottom centre, above the arc. | §3.2. The most important readout in the game, and it does not exist today. |
| **U5** | **Bars change colour at thresholds, never by gradient.** | §2.2. This is already the house rule and it is the right one. |
| **U6** | **The chart gains a node planner** with prograde/radial handles and a live projected conic. | §4. The Spaceflight-Simulator layer. |
| **U7** | **No new screen for crew.** The compartment is a panel over the current screen, not a mode. | §6 |
| **U8** | **Nothing in `ui/` or `hud/` takes a mutable `World&`.** | PLAN-10 F2, enforced by the layer check. |

---

## 2. The three registers (U1)

| register | where | ground | ink | feel |
|---|---|---|---|---|
| **GLASS** | flight | `FIELD` | `ETCH` cool white | you look *through* it. Sparse, etched, mostly bare. |
| **CHART** | map, plan, shipyard | `PLATE` | `VELLUM` warm ivory | you read *at* it. Dense, ruled, a document. |
| **COMPARTMENT** | crew, dialogue, contracts | `BULKHEAD` (new) | `VELLUM` + one warm accent | you are *inside* it. Close, warm, human. |

The player always knows which of the three worlds they are in, and the register carries that without
a label.

### 2.1 The new token

```cpp
// src/ui/tokens.h — the third ground. One step warmer and lighter than PLATE: the inside of a
// hull, lit by working light, not by instruments.
inline const glm::vec4 BULKHEAD{0.094f, 0.086f, 0.078f, 1.0f};  // #181614
/** The one warm accent the compartment may use, and nothing else may: a person is speaking. */
inline const glm::vec4 VOICE{0.898f, 0.678f, 0.443f, 1.0f};     // #e5ad71
```

Nothing else in `tokens.h` changes. `FIELD`, `PLATE`, `ETCH`, `VELLUM`, `NAV`, `DRIVE`, `THREAT` and
the four opacity weights are correct and stay.

### 2.2 Bars (U5)

One helper, used by heat, power, propellant, charge and strain:

```cpp
// track is always ETCH at RULE. The fill changes colour at thresholds, never within a band.
void push_meter(UIBatch &b, const Rect &at, float value,      // 0..1
                float warn, float alarm);                     // thresholds
// value <  warn   -> NAV
// value <  alarm  -> DRIVE
// value >= alarm  -> THREAT
```

No gradients, no glow, no pulse. A reading is a colour and a length.

### 2.3 The clock (U3)

`ui::Context::begin` already receives `now`. Expose it and add one easing helper:

```cpp
double time() const { return now_; }                       // already present, now used
float  ease(uint32_t id, float target, float tau);         // per-widget critical damping
```

`ease` keeps a small map of `id -> current` and steps it with the same frame-rate-clean form
`camera_follow.cpp` uses. That is the whole animation system: a bar that moves, a panel that opens.
Anything more is scope for a later plan.

---

## 3. The flight view (GLASS)

### 3.1 Layout, 1600x900 reference

```
+-- top strip: ship | contract | clock | warp -------------------------+
|                                                                      |
|  +- LEFT RAIL 280 -+                         +- RIGHT RAIL 280 -+    |
|  | VESSEL          |                         | NAV              |    |
|  |  accel   0.30 g |                         |  orbit           |    |
|  |  prop  t 74 %   |       [ the ship ]      |  target          |    |
|  |        m 91 %   |       collar r 110      +------------------+    |
|  |  heat  [====  ] |                         | SCOPE            |    |
|  |  power [===== ] |                         |  (minimap)       |    |
|  +-----------------+                         +------------------+    |
|  | CREW            |                                                 |
|  |  Vhast    eng   |                                                 |
|  |  Imari    nav   |                                                 |
|  +-----------------+                                                 |
|                                                                      |
|              +------------- BURN -------------+                      |
|              | 0.30 g   ETA 5d 04h   dv 1330  |                      |
|              +--------------------------------+                      |
+------------------------- heading arc ---------------------------------+
```

Six changes against what ships today, all of them fixing something observed in a capture:

1. **Collar clamped** to `clamp(shipRadiusPx + 40, 90, 160)`. PLAN-10 D3.
2. **System conics gated** by zoom and clipped outside the collar. PLAN-10 D4.
3. **Blocks bounded** — `place()` returns an empty rect rather than overlapping. PLAN-10 D1.
4. **One top-strip layouter** owning the title and the contract line. PLAN-10 D5.
5. **BURN bar** added, bottom centre (§3.2).
6. **CREW block** added under VESSEL (§6.1).

### 3.2 The BURN bar (U4)

The single most important readout, and the reason the game reads as freight rather than as a flight
sim. Bottom centre, 420x52, above the arc, GLASS register.

```
  0.30 g          ETA 5 d 04 h          dv  1,330 / 1,840 km/s
  [======================------------------]        soft  std  HARD
```

| field | source | note |
|---|---|---|
| accel | `accel_g` (PLAN-12 §3) | the headline. 32 px, `ETCH`, the largest type in the view. |
| ETA | transit duration remaining | blank when not on a transit |
| dv | `dv_spent / dv_planned` | transit tier. `push_meter` with warn 0.75, alarm 0.92. |
| band | which of drift/soft/standard/hard/emergency | the current band is `DRIVE`, the rest `ETCH` at `RULE` |

When no transit is planned it collapses to one line: acceleration and band only. **It is never
absent** — acceleration is always meaningful, because it is always the gravity the crew is standing
in.

### 3.3 The jump readout

Appears only when a jump link exists in the current system (PLAN-13 §3.2). Slots into the right
rail under NAV, three rows and a charge meter.

---

## 4. The chart and the node planner (U6, CHART)

This is the Spaceflight-Simulator layer, and almost all of it already exists in `orbit/`.

### 4.1 What is drawn

| element | source | ink |
|---|---|---|
| current conic | `ship` state -> `Elements` | `NAV` |
| projected conic | `planned_conic()` | `DRIVE`, dashed |
| apoapsis / periapsis marks | `Elements` | `VELLUM`, labelled with altitude and time-to |
| bodies and their orbits | `propagate()` | `VELLUM_RULE` |
| SOI circles | `Body::soi` | `VELLUM_RULE`, dotted |
| jump line | PLAN-13 §6 | `VELLUM_RULE`, dashed |
| node handles | `World::nodes` | `DRIVE` |

### 4.2 Placing and dragging a node

```
click on the conic          -> insert a Node at that true anomaly, t from the conic
drag the prograde handle    -> node.prograde += drag . conic_tangent * scale
drag the radial handle      -> node.radial   += drag . conic_normal  * scale
scroll on a handle          -> fine adjust, 1/10 scale
right-click a node          -> delete
```

`scale` is `metres_per_pixel * NODE_SENSITIVITY`, so a handle drag feels the same at every zoom.
After any edit, `planned_conic()` is recomputed and redrawn — that is the live feedback the whole
interaction depends on, and it is one existing call.

The readout beside the node:

```
NODE  +00:42:15
  prograde   +9,053 m/s
  radial         +0 m/s
  dv total    9,053 m/s      transit tank      1.29 t
```

A node is paid from the **transit** tank (PLAN-12 §2): it is placed and resolved, not flown. The row
shows the propellant mass at the departure mass, not just the delta-v, because tonnes are what the
player is actually spending. A node the tank cannot pay for draws in `THREAT` and says `INSUFFICIENT`.

### 4.3 The route comparison

The one panel that makes PLAN-12 §5 legible. When a target is selected, the chart shows both routes
side by side:

```
        TO  jump line 1.038 AU                    transit tank  184 t

  COAST         16.1 km/s    116 d 19 h      2.3 t
  BURN  0.05 g   433  km/s    10 d 05 h     76.9 t
        0.10 g   612  km/s     7 d 05 h    119.9 t
        0.30 g  1060  km/s     4 d 04 h    267.9 t   <- over tank
        1.00 g  1935  km/s     2 d 07 h    841.6 t   <- over tank
  ................................................ contract due in 14 d
```

The deadline is drawn across the table as a rule. Rows above it miss the contract and draw in
`THREAT`; rows the tank cannot pay for draw dim and say so. **The decision makes itself visible**
and needs no tutorial: here the coast misses by 103 days, the top two burns fit, and the bottom two
are fantasy.

---

## 5. The HUD as data (U2)

The requirement: the interface changes as the storyline changes, without a rebuild. The engine
already states this principle for systems (`nereid.json`: *"not a line of this is hardcoded in
C++"*). Extend it to the HUD.

### 5.1 `assets/hud/flight.json`

```json
{
  "rails": {
    "left":  { "width": 280, "from": "top" },
    "right": { "width": 280, "from": "top" }
  },
  "blocks": [
    { "id": "vessel",  "rail": "left",   "order": 0, "density": 1 },
    { "id": "crew",    "rail": "left",   "order": 1, "density": 1 },
    { "id": "weapons", "rail": "left",   "order": 2, "density": 2, "force_when": "under_fire" },
    { "id": "program", "rail": "left",   "order": 3, "density": 2, "force_when": "gate_failed" },
    { "id": "orbit",   "rail": "right",  "order": 0, "density": 2 },
    { "id": "jump",    "rail": "right",  "order": 1, "density": 1, "require": "jump_link" },
    { "id": "minimap", "rail": "right",  "order": 2, "density": 2 },
    { "id": "burn",    "rail": "centre", "order": 0, "density": 1 }
  ]
}
```

### 5.2 Dispatch

```cpp
using BlockFn    = void (*)(UIBatch &, const HudFrame &, const ui::Rect &);
using HeightFn   = float (*)(const HudFrame &);

struct BlockDef { const char *id; BlockFn draw; HeightFn height; };

// One table. Adding a block to the game is one row here and one line of JSON.
inline constexpr BlockDef BLOCKS[] = {
    {"vessel",  build_vessel_block,  vessel_block_height},
    {"crew",    build_crew_block,    crew_block_height},
    ...
};
```

`build_flight_hud` walks the JSON list, looks each id up in `BLOCKS`, checks `density` /
`require` / `force_when` against the `HudFrame`, and calls `place()` then `draw`. The hardcoded
sequence at `hud.cpp:799-855` is deleted.

**`require` and `force_when` are named predicates over `HudFrame`**, not an expression language:

```cpp
bool hud_predicate(const char *name, const HudFrame &f);   // "under_fire", "jump_link", ...
```

A string that names no predicate is a fatal load error. A config language that can express anything
is a second program to debug; a fixed vocabulary of a dozen predicates is not.

### 5.3 Why this is safe

The layout file cannot introduce a block that does not exist, cannot change what a block reads, and
cannot write to the world. It chooses **which of a fixed set appears, where, and when**. That is the
whole of the dynamism the storyline needs.

---

## 6. The compartment (U7, COMPARTMENT)

### 6.1 The CREW block, in flight

Four to six rows in the left rail. Name, role, one state glyph.

```
CREW
  Vhast     eng    .        nominal
  Imari     nav    .        nominal
  Sote      med    !        strain 0.7
  Oyelaran  gun    x        off duty
```

The glyph is one character in `ETCH` / `DRIVE` / `THREAT` by strain band (PLAN-12 §6). Clicking a
row opens the compartment panel over the current screen.

### 6.2 The panel

Not a screen (U7) — an overlay with its own register, entered and left without changing the
navigation stack. `BULKHEAD` ground, the portrait left, text right.

```
+--------------------------------------------------+
|  [portrait]   VHAST                              |
|   240x300     chief engineer                     |
|               g tolerance   0.45                 |
|               strain        0.12  [==        ]   |
|               trust         3                    |
|                                                  |
|   "The number three radiator is still bent from  |
|    Ceres. She will hold at a third of a g. Ask   |
|    her for more and I am not promising."         |
|                                                  |
|   [ talk ]  [ assign ]  [ stand down ]           |
+--------------------------------------------------+
```

The portrait is a chart-ink line engraving — `VELLUM` on `BULKHEAD`, hatched, the register of a
personnel file the ship actually carries. PLAN-16 owns the art direction and the writing; this plan
owns the frame it sits in.

---

## 7. Gates

| # | gate | how |
|---|---|---|
| G1 | `check_block_layout` reports 0 violations at 1280x720, 1600x900, 1920x1080, all three densities | `selftest.cpp`, 9 cases |
| G2 | the collar radius stays in `[90,160]` across the full zoom range | unit |
| G3 | no system conic is drawn when `zoom > CONIC_FADE_ZOOM` | unit |
| G4 | a `flight.json` naming an unknown block id fails to load, loudly | unit |
| G5 | a `flight.json` naming an unknown predicate fails to load, loudly | unit |
| G6 | removing a block from `flight.json` removes it from the frame and shifts the rest up | golden capture |
| G7 | the burn bar is present in every flight frame at every density | golden capture |
| G8 | editing a node redraws `planned_conic` in the same frame | unit on the call order |
| G9 | a node exceeding the manoeuvre budget renders in `THREAT` and sets `insufficient` | unit |
| G10 | `push_meter` emits exactly one of NAV/DRIVE/THREAT, never a blend | unit |
| G11 | the layer check passes: no `ui/` or `hud/` file takes a mutable `World&` | `tools/check_layers.mjs` |

---

## 8. Files touched

```
new   src/hud/layout.h/.cpp      flight.json load, BlockDef table, predicates
new   src/hud/crew_block.cpp     CREW block + compartment panel
new   src/ui/compartment.h/.cpp  the third register
new   src/ui/planner.h/.cpp      node placement, handles, route comparison
new   assets/hud/flight.json
edit  src/ui/tokens.h            BULKHEAD, VOICE
edit  src/ui/ui.h/.cpp           time() exposed, ease()
edit  src/ui/draw.h              push_meter
edit  src/hud/hud.cpp            delete the hardcoded sequence; collar clamp; conic gate; top strip
edit  src/hud/blocks.cpp         bounded place() (PLAN-10 D1)
edit  src/hud/hud.h              HudFrame gains crew[], burn, jump, transit rows
edit  src/hud/frame.cpp          fill the new fields
edit  src/ui/screens.cpp         chart: planner, route table, jump line
edit  src/selftest.cpp           G1-G10
```
