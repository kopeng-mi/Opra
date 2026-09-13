# Plan 06 — the instrument: flow, HUD, screens, sky

> Live plan, with [PLAN-07-shipkit.md](PLAN-07-shipkit.md). Plan 05's engine work landed; this one
> is about what the player looks at and touches. Plan 07 is what they look *at*.

Two rules run through everything below and are worth stating before the detail:

1. **Every screen is a place you do something.** Not a page you read and dismiss. A menu that only
   lists nouns is a menu that should have been a keypress.
2. **The glass stays clear.** The HUD is transparent except for one solid arc along the bottom.
   Corner blocks darken only the pixels under their own text, and nothing floats over the ship.

---

## 0. What the shots show

Captured from `build/Release/Opra.exe` at 1600×900 into `artifacts/audit/`, every screen, current
build. These are measurements, not impressions.

### 0.1 The flow is wired wrong

`ui/flow.cpp:46` sends Startup → Manual. The only edges out of Manual (`flow.cpp:25-26`) both go
to **Flight**. So reading the manual from the title screen starts the game. The `Transition` struct
has carried a `pushes` field since it was written and **every row in both tables sets it `false`** —
the mechanism that would fix this exists and has never been used.

`Settings::save()` (`game/settings.cpp:60`) writes settings. There is no other serialisation in the
build. So `continue` and `new contract` are the same edge to the same freshly-constructed world, and
the session line `T+ 00:00:00, under way` is printed off a world created two frames earlier. The
title screen offers a save that does not exist.

### 0.2 Overlay screens do not suppress the HUD

`artifacts/audit/help.png`: the flight HUD draws underneath the manual. `VESSEL`, `hull`, `prop`,
`heat`, `mass`, `TWR`, `Δv`, `WEAPONS`, `PDC A`, `PDC B`, `cutter`, `CIRCULARIZE`, `throttle` each
collide with a manual row. The centre speed readout sits mid-table. The manual's last row lands at
y=886 of 900 and the rows past it are off-screen.

`artifacts/audit/chart.png`: same, plus `Sector chart`'s subtitle overlapping the word `prop`.

### 0.3 One quantity, three places, three values

`flight.png` prints **Δv** three times — `20.3 m/s` (VESSEL), `75.5 m/s` (node), `0.0 m/s` (ORBIT) —
with the same label on all three. **throttle** is printed twice (`1.00 / 1.00` in the left column,
`1.00` on the bottom strip). A reader cannot tell which number answers a question.

### 0.4 Text shares pixels with the world

HUD blocks draw bare marks on the field (`hud/hud.h:4`: *"The HUD paints marks, never surfaces"*).
In `flight.png` the ORBIT block's labels are behind an asteroid; the `VESSEL` rule has a body sitting
on it. On `pause.png` and `settings.png` the scrim is α0.78–0.82, so the lit hull reads clearly
through the menu rows it covers.

### 0.5 The sky is 2,420 additive quads and reads as dirty glass

`render/backdrop.cpp`: 1,200 stars + 4 × 80 nebula motes + 600 dust + 300 motes.

Camera `CAMERA_FOV_Y = 0.41887903` rad = **24.0°**, so at 900 px height the scale is **37.5 px per
degree**. Nebula motes are 55–170 m at a 4,600 m shell:

```
55 / 4600  = 0.01196 rad = 0.685 deg  ->  51 px across
170 / 4600 = 0.03696 rad = 2.118 deg  -> 159 px across
```

The comment at `backdrop.cpp:29` claims *"Small enough that no mote reads as a circle of its own"*.
Every mote is a 51–159 px disc. The claim is wrong by roughly fifty times, and 320 of them stacked
additively is the bokeh field in `mapview.png`.

Stars: `size = 9 + rng² · 26` at a ~10,000 m shell → 1.9–7.5 px, brightness up to **1.0**. Bloom
thresholds at `1.15` with a `0.5` knee (`render/renderer.cpp:1052`), so the soft knee opens at
**0.65** — every star brighter than 0.65 blooms. That is the glare.

### 0.6 Defect ledger

| id | where | defect |
|---|---|---|
| T-1 | `ui/flow.cpp:46` | Startup→Manual has no return edge; the manual starts the game |
| T-2 | `ui/flow.h:31` | `pushes` is false on all 20 rows; the push/pop mechanism is dead code |
| T-3 | `ui/title.cpp:10` | `continue` offers a save that does not exist; session line is fabricated |
| T-4 | `game/frame.cpp` | overlay screens do not suppress the flight HUD |
| T-5 | `hud/hud.cpp:691` | 46 px speed readout centred on the play area, over the ship |
| T-6 | `hud/hud.cpp:309,346` + orbit block | three different quantities all labelled `Δv` |
| T-7 | `hud/hud.cpp` | throttle printed twice with different formatting |
| T-8 | `ui/menus.cpp:12,34` | pause/settings scrim α0.78–0.82; the hull reads through the rows |
| T-9 | `ui/menus.cpp:71-81` | settings rows are a 720 px rule with a word at each end, 630 px apart |
| T-10 | `ui/menus.cpp:74-81` | the MSAA radio row draws its marks detached from its options |
| T-11 | `backdrop.cpp:31` | nebula motes are 51–159 px; the comment claims otherwise |
| T-12 | `backdrop.cpp:92` | star brightness to 1.0 against a 0.65 bloom knee |
| T-13 | `hud/hud.cpp:460` | the collar scales to 260 px and covers the play area at every zoom |
| T-14 | `mapview.png` | `warp 1x ?100000x` — a missing glyph renders as a box |
| T-15 | `game/viewer.cpp` | the model viewer's grid renders as diagonals, not a ground plane (plan 05 S-4, still open) |
| T-16 | `hud/hud.cpp` | range rings and the collar draw unchanged at system zoom, where they mean nothing |

---

## 1. Decisions

| id | decision |
|---|---|
| **L1** | Flow is **Title → Contract → Shipyard → Flight**. No save system: a run is a session. `continue` is removed. |
| **L2** | Screens become a **stack**. `pushes` finally does something; Manual and Settings return to whoever opened them. |
| **L3** | The HUD stays transparent. Corner blocks darken only the pixels under their own text. One solid element: the **bottom arc**. |
| **L4** | The ship-centred collar is **removed**. Orientation lives on the arc. Nothing draws over the hull. |
| **L5** | An overlay screen **owns the frame**: it suppresses the flight HUD and draws its own surface. |
| **L6** | Every screen is interactive. The orrery behind the title is live and manipulable; the contract is a chart you explore; the shipyard is a build space. |
| **L7** | Sky is **stars only**, small and under the bloom knee. The nebula is deleted. |
| **L8** | Each quantity appears **exactly once**, from one shared formatter. |

---

## 2. Flow

### 2.1 The stack

`Screen` gains `Contract` and `Shipyard`. `App::screen` becomes `std::vector<Screen> stack` with
`Startup` at the bottom; `current()` is `stack.back()`.

```cpp
enum class Mode { Replace, Push, Pop };
struct Transition { Screen from; Action on; Screen to; Mode mode; };
```

`Mode::Pop` ignores `to`. `apply(stack, edge)`:

- `Replace` — `stack.back() = edge.to`
- `Push` — `stack.push_back(edge.to)`
- `Pop` — `if (stack.size() > 1) stack.pop_back()`

The guard on `Pop` is the whole of the "cannot escape past the root" rule; `Startup` has no Pop edge
anyway, and both facts are asserted rather than assumed.

### 2.2 The tables

```
FLOW
  Startup   Manual        Manual     Push       <- T-1: returns to Startup
  Startup   Settings      Settings   Push
  Contract  Pause         Startup    Pop
  Shipyard  Pause         Contract   Pop
  Flight    Chart         Chart      Push
  Flight    Manual        Manual     Push
  Flight    Pause         Pause      Push
  Flight    ModelViewer   Viewer     Push
  Chart     Chart/Pause   -          Pop
  Manual    Manual/Pause  -          Pop
  Viewer    ModelViewer/Pause  -     Pop
  Pause     Pause         -          Pop
  Settings  Settings/Pause     -     Pop

MENU_FLOW
  Startup   Begin         Contract   Replace
  Startup   Settings      Settings   Push
  Startup   Manual        Manual     Push
  Startup   Quit          -          (the loop reads the row and quits)
  Contract  Accept        Shipyard   Replace
  Contract  Back          Startup    Replace
  Shipyard  Launch        Flight     Replace     <- gated on design_has_drive
  Shipyard  Back          Contract   Replace
  Pause     Resume        -          Pop
  Pause     Settings      Settings   Push
  Pause     Abandon       Startup    (clears the stack to {Startup})
  Settings  Back          -          Pop
```

Note the asymmetry, and that it is deliberate: the four **forward** steps of the run are `Replace`,
because going back through them is a decision (`Back`), not a dismissal. The seven **instruments**
are `Push`, because dismissing one should put you back exactly where you were. T-1 is precisely the
case where a Push edge was written as a hard-coded Replace to Flight.

`Quit to desktop` leaves the pause menu and appears only on the title screen. From flight, `Abandon
run` returns to the title, which is where quitting lives. Two ways to leave the process from two
different screens was the thing that felt wrong.

### 2.3 Tests

Extending `ui/flow_tests.cpp`:

- no `(from, on)` pair appears twice — already tested, keep
- every screen is reachable from `Startup` — already tested, keep
- **new:** for every screen `S` and every Push edge `S → I`, applying the edge then `I`'s Pop edge
  returns a stack whose back is `S`. This is the property T-1 violated, stated directly.
- **new:** no Pop edge exists on `Startup`
- **new:** every screen reachable by Push has at least one Pop edge
- **new:** `Abandon` from any stack depth yields exactly `{Startup}`

---

## 3. The HUD

### 3.1 Layout

```
 +-----------------------------------------------------------------------+
 | SR-084                                                 T+ 00:04:12    |
 | Resolve and recover                                       warp 1x     |
 |                                                                       |
 | VESSEL                                            ORBIT  Nereid       |
 |  hull   ####------  62                             alt      25.66 Gm  |
 |  prop   #######---  81                             peri     25.65 Gm  |
 |  heat   ##--------  17                             apo      25.93 Gm  |
 |  mass       97.9 t                                 ecc        0.0054  |
 |  TWR          1.24                                 period    67 d 12 h|
 |  dv budget 20.3 m/s                                                   |
 |                                                          (  minimap ) |
 | WEAPONS                                                   7 contacts  |
 |  PDC A       ready                                                    |
 |  PDC B       ready                                NODE                |
 |                                                    node dv  75.5 m/s  |
 |                                                    burn        4.6 s  |
 |                                                    T-      31 d 22 h  |
 |                                                                       |
 |                             v                                         |
 |        150   160   170   180   190   200   210                        |
 |  |__ ---'''   '   '   '   '   '   '   '   '''--- __|                  |
 |  thr      31.7          180         1.67 g       warp                 |
 +-----------------------------------------------------------------------+
```

The arc is the only chrome. Everything else is ink on the world with a local scrim.

### 3.2 The arc — geometry

A compass tape bent into a shallow arc across the bottom centre. It is the honest instrument for a
game played on a plane: there is no pitch or roll to show, so a navball would be a sphere displaying
one number. A tape shows that number *and* where everything else lies relative to it.

```
A        = clamp(W * 0.30, 300, 520)        half-width, px
s        = 52                               sagitta (bulge height), px
R        = (A*A + s*s) / (2*s)              radius of curvature
phi_max  = asin(A / R)                      half-angle subtended
y_apex   = H - I - 96                       arc apex; 96 px reserved below for the readout row
C        = (W/2, y_apex + R)                centre of curvature, below the screen
```

At W=1600: `A = 480`, `R = (230400 + 2704) / 104 = 2241 px`, `phi_max = asin(0.2142) = 0.2159 rad
= 12.37 deg`. Check: at `phi_max` the point is `(W/2 + 480, y_apex + 52)` — half-width and sagitta
both recovered, so the parameterisation is self-consistent.

A point and its outward normal at tape angle `phi`:

```
n(phi) = ( sin phi, -cos phi )
p(phi) = C + R * n(phi)
```

`n` points away from `C`, which is upward into the play area. At `phi = 0`, `p = (W/2, y_apex)`.

**Heading to tape angle.** The tape shows a ±45° window:

```
delta = wrap180(bearing_deg - nose_deg)          in (-180, 180]
phi   = phi_max * delta / 45                     valid for |delta| <= 45
```

Scale at the apex: 45° of heading maps to 480 px, so **10.67 px per degree**. Major ticks every 10°
(107 px apart, nine across the tape), minor every 5°, unlabelled.

`wrap180` must be total and deterministic at the boundary: `delta == 180.0` exactly maps to `+180`,
never alternating, or a retrograde marker at the edge flickers between the two ends of the tape.

**Ticks.** Major: from `p(phi)` to `p(phi) + n*12`, label centred at `p(phi) + n*23`, drawn
**upright** — a numeral rotated to the arc's local tangent is unreadable at 13 px and the arc only
leans 12°, so upright is also closer to correct than it sounds.

**Marks** (prograde, retrograde, target, anti-target, node, director cue) sit in the bowl at
`p(phi) - n*10`, 9 px glyphs, the same glyph vocabulary the collar used.

**The index.** A fixed downward caret at `(W/2, y_apex - 16)`. The heading numeral sits under the
apex at `(W/2, y_apex + 34)`, `%03.0f`, READOUT_LARGE.

**Readout row**, baseline `y_apex + 62`:

| position | value |
|---|---|
| `W/2 - A*0.55` | speed, READOUT, with `m/s` at LABEL_SMALL beneath |
| `W/2` | heading numeral (above) |
| `W/2 + A*0.55` | acceleration in g, READOUT |
| `W/2 - A - 28` | throttle, a 72 px vertical bar filled bottom-up |
| `W/2 + A + 28` | warp rate |

This kills T-5 (the centre readout moves here) and T-7 (this is the only throttle).

### 3.3 Arc edge cases

| case | handling |
|---|---|
| mark at `|delta| > 45` | clamp `phi` to `±phi_max`, draw a half-arrow pointing off-tape at 0.7 alpha. **Never dropped** — "your target is behind you" is information |
| `speed < 0.05 m/s` | prograde and retrograde are not live. Not drawn, not drawn at 000 |
| velocity exactly zero | guard before `atan2(0,0)`; the mark is absent, not zero |
| prograde and retrograde | 180° apart, so at most one is ever on tape. Correct and expected |
| two marks within 6 px | stack: the second draws 10 px lower. Never overdrawn |
| `nose_deg` is NaN | ticks still draw, the caret numeral is a dash. Never prints `nan` |
| W < 900 px | `A` is at its 300 floor; narrow the window 90° → 60°, then drop minor ticks, then fall back to a straight strip (guard `R` against `s → 0`) |
| corner blocks | reserve `[W/2 - A - 56, W/2 + A + 56]`; bottom blocks lay out outside it or move up a row |
| system zoom | the arc is scale-free — a bearing is a bearing at any zoom. It stays. The **range rings** and the old collar do not (T-16) |

### 3.4 The scrim, and why α = 0.72

Each block computes the union of its own text rects, inflates by `SPACE[2]` = 8 px, and fills with
`FIELD` at α 0.72 plus a 12 px ring at α 0.36 so the edge feathers instead of drawing a border. No
outline, no radius: the block reads as the world getting darker, not as a panel arriving.

The alpha is derived, not chosen. Worst case is `ETCH` over a lit hull, sampled at `#B2B4B2`:

```
composite = 0.72 * (7,13,21) + 0.28 * (178,180,178) = (55, 60, 65)
L_bg  = 0.0440      L_ETCH = 0.7764
contrast = (0.7764 + 0.05) / (0.0440 + 0.05) = 8.79 : 1
```

Against the brightest thing that can ever be behind a block — a drive flame at white:

```
composite = 0.72 * (7,13,21) + 0.28 * (255,255,255) = (76, 81, 87)
contrast = 0.8264 / 0.1309 = 6.31 : 1
```

Both clear 4.5:1 with room. 0.72 is the smallest round alpha that does, which is why it is the one
that keeps the most of the world visible. Fixes T-4's legibility half and T-8.

### 3.5 One quantity, one place (T-6, L8)

| shown as | source | block | live when |
|---|---|---|---|
| `dv budget` | `frame.deltaV` | VESSEL | tanks non-empty |
| `node dv` | `frame.nodeDeltaV` | NODE | a node exists |
| `plan dv` | `frame.orbit.dvPlanned` | ORBIT | **and** it differs from `node dv` by > 0.1 m/s |

The third row disappears on a single-node plan, which is the `live()` predicate doing exactly the
job it exists for. All three go through one formatter, so they cannot diverge in units or digits.

`dv budget` guards the log: `m_dry <= 0 || m_wet <= m_dry` yields `--`, never `-inf` (plan 05 §4.6).

### 3.6 Block contract

```cpp
struct Block {
    const char *title;
    float measure(const HudFrame &) const;                  // height it will use
    void  build(UIBatch &, const HudFrame &, Rect at) const;
    bool  live(const HudFrame &) const;                     // false = absent, never empty
};
```

`live()` returning false means the block is **not placed** — no title, no rule, no zeros. A block
with nothing to say occupies nothing. `BlockLayouter` already guarantees non-overlap by construction
and `check_block_layout` proves it per frame under `--debug`; both stay, with a new rule added: no
block's rect may intersect the arc's reserved span.

---

## 4. The screens

Each one answers: *what do you do here?* If the answer is "read it", it is not a screen.

### 4.1 Title

The orrery behind the plate becomes **live and yours**: wheel zooms, drag rotates, bodies run on an
accelerated clock. Hovering `begin` flies the orrery to the contract's zone over 0.8 s, so the menu
is a preview of where you are going. The plate is a narrow left column; it never covers the star.

Rows: `begin` · `settings` · `manual` · `quit`. `continue` and `new contract` both go (T-3), and so
does the fabricated session line.

### 4.2 Contract

A chart of the zone you can pan and zoom, markers placed on it, hovering one prints its line.
A `hazards` toggle overlays belt density. Payout, deadline and the vessel licence sit in a column
beside it. `accept` advances; `back` returns to the title.

Not a paragraph with a button under it.

### 4.3 Shipyard

The interactive screen, and the one plan 07 exists to fill.

```
 +--------------+-----------------------------------+-------------------+
 | PARTS        |                                   | DERIVED           |
 |  spines    > |        (live 3D, turntable)       |  mass     97.9 t  |
 |  drives    > |                                   |  TWR        1.24  |
 |  tanks     > |          ghost stations appear    |  dv      1.42 km/s|
 |  hab       > |          while a part is held     |  torque   12.4 MNm|
 |  weapons   > |                                   |  hull        420  |
 |  utility   > |                                   |  cooling    18 kW |
 |  cargo     > |                                   |                   |
 |              |                                   |  + drive_main     |
 |  [mirror]    |                                   |    TWR 1.24 -> 1.71|
 +--------------+-----------------------------------+-------------------+
                               [ back ]   [ launch ]
```

- drag the turntable; wheel zooms; the ship is the real composed model, not a preview render
- pick a part from the palette and the spine's free stations light as ghost rings; click one to place
- `mirror` mirrors the placement to the opposite facing (§3 of plan 07). Greyed for fore/aft
  facings, which have no mirror partner — greyed, not silently ignored
- the DERIVED column updates live, and while a part is held it shows the **delta** that placing it
  would cause. That is what makes the screen a tool rather than a form
- right-click removes; a mirrored pair removes together
- `launch` is disabled with a reason line when `design_has_drive()` is false

### 4.4 Flight

Arc, corner blocks, minimap. Collar gone (T-13). Range rings live only under a 2 km half-height
(T-16).

### 4.5 Chart

Full-bleed surface; **suppresses the flight HUD** (T-4). Pan, zoom, click a marker to target, `N`
plans a transfer to it. Closes on `M` or `Esc` back to wherever it was opened from.

### 4.6 Manual

Suppresses the flight HUD. Paginated into columns that fit the viewport with an explicit page
indicator — the current version runs 30 rows off the bottom of an 900 px screen.

Interactive: **the key you press highlights its row**. Hold `W` with the manual open and `main
drive` lights. Typing filters. That is the difference between a keymap you read and one you learn.

### 4.7 Pause and settings

Scrim to α 0.92 (T-8). Settings rows become bounded: label left, control **adjacent** to it, gap
capped at 180 px, rules only between sections and not under every row (T-9). The MSAA control becomes
a two-button segmented pair rather than detached radio marks (T-10). Camera pitch previews live
behind the scrim while dragging.

---

## 5. The sky

| | now | after | why |
|---|---|---|---|
| nebula | 4 × 80 motes, 51–159 px | **deleted** | T-11; every mote reads as a disc |
| stars | 1,200 | 700 | fewer, so each can be a point |
| star size | `9 + rng²·26` → 1.9–7.5 px | `6 + rng²·9` → **1.3–3.2 px** | a star is a point source |
| star value | `0.35 + rng·0.65` → max 1.00 | `0.18 + rng²·0.42` → max **0.60** | the bloom knee opens at 0.65 (§0.5). Capping below it is what stops the glare, and the squared term keeps most stars faint |
| dust / motes | 600 / 300 | unchanged | the near-field speed cue, already dim |

2,420 → 1,300 backdrop quads. Space is black; the star, the drive flames and the hull are the only
bright things in frame, which is the contrast structure `backdrop.cpp:102` claims and does not have.

Delete `Backdrop::nebula`, `NEBULA_*`, `facing()` and the nebula loop in `add_backdrop` — `facing()`
has no other caller, and leaving a billboard helper behind for a layer that no longer exists is how
it comes back.

---

## 6. Phases

| phase | work | depends |
|---|---|---|
| **N0** | Flow: stack, `Mode`, both tables, the five new tests. Fixes T-1, T-2 | — |
| **N1** | Title trimmed; `continue` and the session line removed. Fixes T-3 | N0 |
| **N2** | Overlay screens suppress the flight HUD and own their surface. Fixes T-4 | N0 |
| **N3** | Sky: nebula deleted, stars retuned. Fixes T-11, T-12 | — |
| **N4** | The arc. Collar removed, centre readout moved. Fixes T-5, T-13, T-16 | — |
| **N5** | Scrim + block contract + one-quantity-one-place. Fixes T-6, T-7, T-8 | N4 |
| **N6** | Contract screen | N0 |
| **N7** | Shipyard screen — palette and turntable against stock designs | N0, plan 07 P3 |
| **N8** | Manual paginated and live-highlighting; settings rows rebuilt. Fixes T-9, T-10 | N2 |
| **N9** | Title orrery live; chart interaction | N1, N2 |
| **N10** | Stragglers: T-14 missing glyph, T-15 viewer grid | — |

N7 is the one with a cross-plan dependency: the shipyard cannot be built against a kit that does not
exist yet. N0–N6 and N8–N10 are free of plan 07 entirely.

---

## 7. Gates

1. `flow_tests` pass, including push/pop round-trip on every Push edge, and Startup has no Pop.
2. Opening the manual from the title and closing it returns to the **title**. Screenshot diff proves
   the frame is identical to the one before it opened.
3. `help.png` and `chart.png` contain no flight-HUD text. Asserted by the layout pass, not by eye.
4. No quantity appears twice in one frame: a debug pass hashes every drawn label string and fails on
   a duplicate outside a whitelist.
5. Contrast: sample `ETCH` glyph pixels against their scrimmed background across all screens; the
   minimum ratio is ≥ 4.5:1. Both worst cases in §3.4 are in the sample set.
6. Arc: sweep the nose through 0–360° in 1° steps; every mark is either on tape at the `phi` its
   heading implies, or clamped with an off-tape arrow. No mark is ever absent while its target lives.
7. No HUD block rect intersects the arc's reserved span, at 1280×720, 1600×900 and 2560×1440.
8. Backdrop: no drawn backdrop instance exceeds 3.5 px, and no star's colour exceeds 0.65 in any
   channel. Measured off the instance buffer, so it cannot drift.
9. `--debug` layout pass reports zero violations on every screen at all three resolutions.
10. Launch is refused with a visible reason for a design with no drive.
