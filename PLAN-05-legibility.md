> **Status: complete.** The live plans are [PLAN-06-instrument.md](PLAN-06-instrument.md) (flow, HUD, screens, sky) and [PLAN-07-shipkit.md](PLAN-07-shipkit.md) (spines, parts, assembly, wreckage).

Live plan. Plans [01](PLAN.md), [02](PLAN-02-system.md), [03](PLAN-03-worlds.md) and
[04](PLAN-04-assets.md) are complete; their decisions bind except where reversed below.

Plan 04 rebuilt every model against measured spec tables. The models are faithful to those tables
and still read as grey lumps, for a reason that turns out to be arithmetic. This plan fixes that at
the authoring rule, replaces the flight-view/map-screen split with one continuous zoom from hull to
system, gives the camera the freedom the sketch asks for, rebuilds the HUD at KSP information
density, and puts close-quarters manoeuvring and combat behind the hardpoints that already exist.

---

## 0. Review of plan 04

**Landed and verified.** 1451 selftest checks, clean Release build.

| Was | Now |
|---|---|
| H0 camera: mouse Y drove pitch *and* pan | `camera_follow.cpp` `look_step` is three lines — a pure unprojection difference. Pitch gone from it. |
| H0 text: digits dropping | `31.7` renders correctly at every density. |
| A2 palette retune | In the GLBs: `armor #A2A6A3`, `teal #2B5559`, `ochre #AD6F42`, `frame #5D6A71`. |
| A3 texture pipeline | `stb_image`, planet maps, rock tiles wired. |
| §2 spec tables | `kestrel.ts` follows them line by line, reasoning in comments. |

Nothing in plan 04 was done badly. The problem is that none of it can be seen.

### The legibility arithmetic

```
view half-height   340 / 1.35 = 252 m     ->  1.79 px per metre at 900 px tall

kestrel   46 m  ->   82 px long,  26 px wide,  5776 triangles
mule      58 m  ->  104 px
needle    31 m  ->   55 px

ochre identity band   0.85 m  ->  1.52 px
teal identity band    1.15 m  ->  2.06 px
bay coaming           0.55 m  ->  0.98 px
```

Every feature carrying the design is **one to two pixels wide**. They cannot read as features; they
average into the hull and the hull averages into grey. The detail frequency is about ten times
finer than the display resolves.

Not lighting, which I checked before blaming it: rendered hull mean `#B2B4B2` against a base colour
of `#A2A6A3` is ordinary shading.

### Other defects

| ID | Site | Fault |
|---|---|---|
| **S-1** | `render/backdrop.cpp` | Nebula back to large soft ellipses with locatable edges — reads as watercolour stain over space. Plan 03 R-3 regressed. |
| **S-2** | `hud.cpp` collar | Range ring labels drawn at ring radius regardless of ship screen size; they land on the hull. Open since plan 03. |
| **S-3** | `config.h:18` | Comment claims "187 m half-height at the default zoom". `FLIGHT_HALF` is 340 at zoom 1.35 = 252 m. The stale number was used to justify the follow tuning above it. |
| **S-4** | `game/viewer.cpp` | Grid overlay draws long diagonals across the right third instead of a ground grid. |
| **S-5** | flight view | Ship renders with no colour differentiation — the symptom a player reports for the arithmetic above. |

---

## 1. Decisions

| # | Decision | Consequence |
|---|---|---|
| **J1** | **Camera: 30° cone of freedom.** Home is fixed elevation, fixed azimuth (north up). Ctrl+drag moves the eye anywhere within a **30° cone** about the home axis, any direction, easing back on release. | From the sketch, confirmed: 30° is the *degree of freedom*, not the elevation. The cone never inverts the frame, so the collar keeps world north. |
| **J2** | **One continuous zoom, no separate map.** Hull scale to system scale on one wheel. The orrery stops being a screen and becomes what you see when you keep zooming out. | §2. Replaces plan 03's map screen. Biggest architectural change in this plan. |
| **J3** | **Three LODs plus an icon**, selected by on-screen pixel size with hysteresis. | §2.5. |
| **J4** | **Legibility fixed with both levers**: default framing pulled in *and* models re-authored bold. | §3. Neither alone works. |
| **J5** | **Minimum feature size is a build rule the exporter enforces.** | §3.2–3.4. What stops this recurring. |
| **J6** | **Fine detail moves from geometry to material maps.** | Mip filtering turns sub-pixel texture into material; sub-pixel geometry turns into aliasing. |
| **J7** | **HUD at KSP density**: information-dense, grouped into bounded blocks, context-gated so density never becomes clutter. | §4. |
| **J8** | **Close quarters: manoeuvring first, combat on top of it.** PDCs, torpedoes, damage. | §5. **This reverses plan 03's no-combat non-goal.** |
| **J9** | **Survey and modular refit** also in scope. | §6. |

Carried and binding: `sim` is `double`; world positions never live in a `float` outside the scene
builder; `orbit/` depends only on `<cmath>` and glm; one-way module dependencies; every phase ends
with a clean `/W4` Release build and `--selftest` exit 0.

---

## 2. Continuous scale (J2)

One camera, one world, no mode switch. The wheel runs from a 5 m half-height to 5 × 10¹⁰ m — ten
orders of magnitude.

### 2.1 Zoom control

Zoom is exponential in half-height, so a notch feels the same at every scale:

```
half_height *= exp(-wheel · k)          k = 0.25, or 1.00 with Shift held
half_height  = clamp(half_height, 5.0, 5e10)
```

`ln(1e10) = 23`, so the full range is 92 notches, or 23 with Shift. Add:

- **Frame a body**: double-click sets `half_height = 2.5 × body.radius` and the follow target to it.
- **Home**: `0` returns to the flight framing and the ship.
- **Scale presets**: `1`…`5` jump to hull / flight / local / orbital / system.

### 2.2 Near and far scale with the camera

A fixed near/far cannot span ten orders. Both are derived from the eye distance `R`, so the ratio
is constant and reversed-Z has the same precision at every zoom:

```
R     = half_height / tan(fov_y / 2)
near  = R · 1e-3
far   = R · 1e3
```

Ratio 10⁶ at every scale. With reversed-Z on `D32_FLOAT` that is comfortable.

### 2.3 Deep space: what lies beyond `far`

At flight zoom `far` is ~600 km, and the planets are at 10¹⁰ m. They must still be drawn, at the
correct bearing and the correct angular size, or the sky is empty and the continuity is a lie.

Anything farther than `far` is drawn in a **deep pass** before the near pass, onto a shell just
inside the far plane. The re-projection preserves direction and angular size **exactly**:

```
dir     = normalize(p_world - eye)                 // computed in double
d       = |p_world - eye|                          // double
d'      = far · 0.9
p'      = eye + dir · d'                           // cast to float here, never before
r'      = r · d' / d
```

Because `r'/d' = r/d`, the angular radius is unchanged, and the direction is unchanged. An object
crossing the `far` boundary therefore does not move or change size at all — the transition is
continuous by construction, not by tuning.

Frame order:

```
1  deep pass    bodies beyond far, re-projected. depth cleared after.
2  near pass    everything within [near, far], real camera-relative positions.
3  overlay      orbit lines, icons, markers (§2.4, §2.6). depth test off.
4  post         bloom, tonemap.
5  UI           HUD. after tonemap, always.
```

### 2.4 Orbit lines and other unbounded geometry

A conic can span from 100 m to 10¹⁰ m in one curve, so it belongs in neither pass. Orbit lines,
transfer arcs and the predicted trajectory are drawn in the **overlay** as projected screen-space
polylines:

```
for each sample on the conic:
    v      = p_world - eye                          // double
    clip   = view_proj · vec4(float(v), 1)
    if clip.w <= near_eps: mark the sample as behind
    screen = (clip.xy / clip.w · 0.5 + 0.5) · viewport
```

Samples marked behind are handled by clipping the *segment* against the near plane in eye space
before projecting, not by dropping the sample — dropping it leaves a visible gap in an orbit that
passes behind the camera.

Adaptive sampling: step in eccentric anomaly, subdividing wherever the angle between successive
screen-space segments exceeds 4°. Caps at 512 samples per conic. A circular orbit then costs ~90
samples and a high-eccentricity one spends them where the curvature is.

### 2.5 LOD by pixel size (J3)

Selection is on **on-screen size**, never distance, because at one distance a rock and a planet are
not the same problem:

```
px = 2 · r / d · (height / 2) / tan(fov_y / 2)
```

| px diameter | representation | budget |
|---|---|---|
| > 250 | detailed model | ~6000 tris |
| 60 – 250 | bold model (§3) | ~1500 tris |
| 8 – 60 | blocky model | ~200 tris |
| < 8 | vector icon + label, overlay pass | — |

**Hysteresis, not crossfade.** Each threshold has a ±10% deadband: a model already at the bold LOD
stays there until it exceeds 275 px or drops below 54 px. This costs nothing and removes pop-flicker
for an object sitting exactly on a boundary. Crossfading would need a blend pipeline and a second
draw per object, for an effect nobody sees at a 10% deadband.

The icon is not a fallback — below 8 px it is the *only* honest representation. A 3 px grey smudge
is not a ship, and a player at orbital scale is reading positions, not hulls.

### 2.6 Icons

Drawn in the overlay pass as screen-space vector marks at the projected world position:

| Kind | Mark |
|---|---|
| Own ship | filled chevron, oriented to heading |
| Ship | hollow chevron |
| Station | ring |
| Planet / moon | filled disc, radius `max(3, px/2)` |
| Star | filled disc with a four-point flare |
| Rock | small dot, only if selected or targeted |
| Node | crossed circle |

Labels appear above ~12 px separation from their neighbour. **Declutter**: bucket icons into a 48 px
screen grid; where more than three land in one bucket, draw one cluster mark with a count and drop
the individuals. Without this, system scale is a wall of overlapping labels.

### 2.7 Warp coupling

Warp and zoom stay independent controls, with one coupling in each direction:

```
warp_suggest = clamp(10^floor(log10(half_height / 50)), 1, 100000)
```

Shown as a suggestion on the time block, never applied silently. And the hard rule:

- Zooming below a 200 m half-height forces warp to 1×.
- Plan 03's auto-drop rules still hold (SOI change, node T-10 s, proximity, contact).
- **New:** any round or torpedo in flight forces warp to 1× (§5).

### 2.8 Edge cases

| Case | Handling |
|---|---|
| Zoom out while docked | Follow target stays the ship, not the station. Docked ship inherits station motion; the camera must not jump on undock. |
| Orbit line crosses an SOI boundary | The conic is about one parent. Convert each sample to barycentric before projecting, and split the polyline at the crossing so the two arcs are drawn about their own parents. |
| Ship inside the near plane at high zoom-out | It became an icon long before, but assert it: if `d < near` and the object is not an icon, force the icon. |
| Object exactly behind the eye | `clip.w ≤ 0`. Clip the segment in eye space; never project a point with non-positive `w`. |
| Two bodies on the same pixel | §2.6 declutter. |
| `d` = 0 (target is the camera origin) | Guard the divide; the follow target is never drawn. |
| Zoom changing during a burn | Nothing about the burn changes. Zoom is a view control and must never touch `FlightInput`. |
| Deep-pass object with `d` just above `far` | Re-projection is exact at the boundary, so no special case. Verified by gate 4. |
| Hyperbolic orbit at system scale | Sample in hyperbolic anomaly over a bounded window (±3 SOI radii), not to infinity. |
| Warp raised with zoom at hull scale | Blocked by §2.7. |

---

## 3. Legibility (J4, J5, J6)

### 3.1 New default framing

```
FLIGHT_HALF   340 -> 200                 default zoom 1.35 -> 150 m half-height -> 3.00 px/m

kestrel   46 m  ->  138 px   (was 82)
mule      58 m  ->  174 px   (was 104)
needle    31 m  ->   93 px   (was 55)
```

This is now just the *home* framing for the `0` key and the startup state; the wheel goes anywhere.
Retune `belt.cpp` density down: each rock is twice the size on screen, so the same count reads as
rubble. Fix S-3's stale comment while there.

### 3.2 Authoring rules

At 3.00 px/m home framing:

| Rule | Value | Reason |
|---|---|---|
| Minimum feature dimension | **1.5 m** | 4.5 px at home. Below this a feature is noise. |
| Minimum identity feature | **3.0 m** | 9 px at home, still 4 px at the widest flight zoom. |
| Identity band width | **3.5 m** | 10.5 px. Was 0.85–1.15 m. |
| Silhouette notch depth | **≥ 3.0 m** | The plan outline is the design; anything shallower closes up. |
| Triangle budget, bold LOD | **~1500** | Was 5776 with no LOD at all. |
| Minimum material region, plan view | **6 m²** | ≈54 px². Smaller and colour never reads as colour. |
| Panel lines, rivets, seams | **no geometry** | Normal map only (J6). |

### 3.3 The exporter enforces it

`tools/export.mjs --audit` gains a legibility report and **fails the export** on violation:

```
kestrel   46.0 m   138 px at home framing
  triangles        1489                      ok   (budget 1500)
  plan bbox        138 x 44 px
  smallest feature 1.8 m = 5.4 px            ok   (min 1.5 m)
  material regions, plan-view area:
    armor          2140 px²  61%             ok
    lightArmor      420 px²  12%             ok
    teal            118 px²   3%             ok
    ochre            96 px²   3%             ok
    black            41 px²   1%   FAIL      below 54 px² minimum
```

Computed by rasterising the merged geometry orthographically down +Z at the home pixels-per-metre
and counting pixels per material. A few dozen lines, and it answers the only question that matters:
*will a player see this?*

`prims.ts` gains `min_feature(size, name)` which throws at authoring time, so the audit is the
backstop rather than the first line of defence.

### 3.4 Silhouette gates

"Does it read" is otherwise a matter of taste, so two automated checks on the plan raster:

- **Distinctness.** Normalise each ship's plan silhouette to a common bounding box and compute
  pairwise intersection-over-union. Any pair above **0.70** is too similar. This is the check no
  single reference-image chat could perform, because it can only see one ship.
- **Complexity.** Perimeter² / area. A featureless blob sits near 4π ≈ 12.6. Require **≥ 22**.

### 3.5 What changes per model

A pass over `tools/models/*.ts`, not a rewrite:

- **Delete** every `panel_relief`, bolt row, rail and greeble under 1.5 m; they become normal-map
  content in the module's `maps` export.
- **Widen** identity bands to 3.5 m and carry them as one continuous block across deck and flanks
  rather than three thin strips.
- **Deepen** the plan silhouette: the Kestrel's dorsal bay becomes a real notch in the *outline*,
  not a recess in the deck; the drive bells move outboard so the stern reads forked; the prow keeps
  its ogive and gains a shoulder step.
- **Consolidate** materials — under 6 m² in plan view, a region grows or merges.
- **Keep** colliders and hardpoints exactly as plan 04 specified. Gameplay does not move.
- **Add** the blocky LOD (~200 tris): the silhouette extruded, drive bells as cylinders, bands kept.
  It is the bold model with everything but the outline and the colour blocking removed.

---

## 4. HUD at KSP density (J7)

KSP's lesson is not "more numbers". It is that dense information reads when it is **grouped into
bounded blocks**, each answering one question, with nothing in a block that does not belong to it —
and that blocks appear only when their question is live.

### 4.1 Layout

```
┌──────────────────────────────────────────────────────────────────────────────┐
│ SR-084  Resolve and recover          alt 412.6 km          T+ 04:12:09  10x  │
│ ▓▓▓▓▓▓▓░░░ 68%                       ▲ 1.4 m/s             warp ⏵ 100x ok    │
│                                                                              │
│ ┌ VESSEL ─────────┐                                    ┌ ORBIT ───────────┐ │
│ │ Kestrel         │                                    │ Tessera          │ │
│ │ hull  ▓▓▓▓▓▓▓▓░ │                                    │ Ap   1 204.8 km  │ │
│ │ prop  ▓▓▓▓▓░░░░ │              ╭──────────╮          │      T- 31:06    │ │
│ │ heat  ▓░░░░░░░░ │            ╱   collar    ╲         │ Pe     318.1 km  │ │
│ │ mass   98.2 t   │           │      ◈        │        │      T-  4:12    │ │
│ │ TWR     1.67    │            ╲  31.7 m/s   ╱         │ ecc    0.0412    │ │
│ │ Δv    2.41 km/s │              ╰──────────╯          │ per   1 h 48 m   │ │
│ └─────────────────┘                                    │ SOI   Tessera    │ │
│                                                        └──────────────────┘ │
│ ┌ WEAPONS ────────┐                                    ┌ NODE ────────────┐ │
│ │ PDC A  ready    │                                    │ circularise      │ │
│ │ PDC B  ready    │                                    │ Δv     75.5 m/s  │ │
│ │ tube 1 loaded   │                                    │ burn    4.6 s    │ │
│ │ tube 2 loaded   │              ┌ minimap ┐           │ T-  31 d 22:14   │ │
│ └─────────────────┘              └─────────┘           └──────────────────┘ │
│                                                                              │
│ throttle ▓▓▓░░░░░ 0.24    1.67 g   006°        assist F   kill X   warp . ,  │
└──────────────────────────────────────────────────────────────────────────────┘
```

Every block has a **named header rule** and a bounded extent. That is the device that lets density
stay readable: the eye finds the block by its label, then reads inside it. Unbounded floating
numbers at the same weight — what the HUD does today — is what actually causes clutter, not count.

### 4.2 Block contract

```cpp
struct Block {
    const char* title;
    glm::vec2   measure(const HudFrame&) const;   // self-measuring, plan 03 G2
    void        build(UIBatch&, Rect, const HudFrame&) const;
    bool        live(const HudFrame&) const;      // false -> not drawn at all
};
```

**`live()` is the anti-clutter mechanism.** A block with nothing to say does not render an empty
frame or zeros — it is absent, and the blocks below it move up. No target, no TARGET block. Not
under thrust, no throttle row. In orbit about nothing, no ORBIT block.

### 4.3 Readout maths

| Readout | Expression |
|---|---|
| Altitude | `\|r\| - R_body` |
| Vertical speed | `(r · v) / \|r\|` |
| Horizontal speed | `sqrt(\|v\|² - v_vert²)` |
| Apoapsis | `a(1+e) - R_body` |
| Periapsis | `a(1-e) - R_body`, shown as **"impact"** when `a(1-e) < R_body` |
| Time to apoapsis | `(π - M) / n`, wrapped positive |
| Time to periapsis | `(2π - M) / n`, wrapped positive |
| Period | `2π sqrt(a³/μ)`, and **"escape"** when `e ≥ 1` |
| TWR | `thrust / (mass · μ/\|r\|²)`; **"—"** when no dominant body |
| Δv budget | `I_sp · g₀ · ln(m_wet / m_dry)` |
| Burn time | `Δv · m / thrust`, ignoring mass loss below 5% of total |
| Closing rate | `-(r_rel · v_rel) / \|r_rel\|` |

**Number formatting is a shared function, not per-site.** Engineering prefixes with a fixed field
width so a value never changes width as it counts — `  9.81 m/s`, ` 412.6 km`, `1 204.8 km`,
`26.14 Gm`. Thin-space thousands separators. Times as `31 d 22:14`, `4:12`, `0.8 s`.

### 4.4 The collar earns its space

It is the navball analogue and currently shows bearing ticks and little else. It gains the vector
marks that make a navball useful:

| Mark | Placement |
|---|---|
| prograde / retrograde | bearing of `±v` |
| target / anti-target | bearing of `±r_rel` |
| target prograde / retrograde | bearing of `±v_rel` |
| manoeuvre node | bearing of the node's Δv direction |
| flight director cross | from `Cue.heading` (plan 03 F9) |

And S-2: range labels move to the ring's 45° diagonals, and are dropped entirely when the ring is
under 160 px.

### 4.5 Density and context

Density 2 is the baseline (J7). Level 1 keeps a deliberate clean-glass option; level 3 adds
per-thruster, mass flow, thermal, power. Context still forces a block on regardless of level:
docking forces the corridor block, descent forces altitude/vertical speed/burn, an SOI change
forces ORBIT for 10 s, an out-of-tolerance gate forces its own block, **incoming fire forces
WEAPONS and the threat block** (§5).

### 4.6 Edge cases

| Case | Handling |
|---|---|
| No orbit (landed, docked) | ORBIT block not live. |
| Hyperbolic | Period "escape", Ap "—", Pe still valid. |
| Sub-surface periapsis | "impact in mm:ss", in `threat`. |
| Node in the past | Node is consumed and the block goes dark; it never shows negative T-. |
| Warp > 1 | T- counters tick at warp rate; burn times do not (they are ship time). |
| Window under 1000 px wide | Right column collapses under the left; blocks stack rather than overlap. Assert via plan 03's layout pass. |
| Value unavailable this frame | Render the previous value at dormant opacity, never `0` or `nan`. |
| Δv with empty tanks | `0.00 m/s`, not `-inf`. Guard the `ln`. |

---

## 5. Close quarters (J8)

Manoeuvring is the foundation; weapons are what happens while you do it.

### 5.1 Manoeuvring

The flight model is already Newtonian with an RCS torque solve. What close quarters needs:

- **Translation authority** at low speed: RCS strafe is currently 22% of main thrust. Make it a
  per-component derived value (§6.2) so a refit with more RCS blocks manoeuvres better.
- **Collision response** must not launch you: plan 02's single-MTV rule stands, and the restitution
  at low relative speed drops to near zero so a nudge against a rock is a nudge.
- **Proximity cues**: closing rate and range to the nearest three contacts, drawn on the collar
  inside 500 m.

### 5.2 Point-defence cannons

Turrets are hardpoints that already exist on every hull. Each has a traverse range, a rate, a round
speed, a magazine and a heat budget.

**Lead solve.** Constant-velocity target, constant-speed round, relative position `r` and velocity
`v`, round speed `s`:

```
|r + v t| = s t
(v·v - s²) t² + 2 (r·v) t + (r·r) = 0
```

Take the **smallest positive root**. If the discriminant is negative or both roots are negative,
there is no solution — the target cannot be caught, and the turret holds fire rather than firing
uselessly. Aim direction is `normalize(r + v t)`.

Degenerate cases the quadratic hides:
- `v·v = s²` — the quadratic collapses to linear: `t = -(r·r) / (2 r·v)`, valid only if `r·v < 0`.
- `|v| ≈ 0` — direct aim, `t = |r| / s`.
- Solution outside turret traverse — no shot.
- Solution beyond effective range (`s · t > range`) — no shot.

**Hit detection must be swept.** A round at 1100 m/s travels 9.2 m per 120 Hz step; a 2 m torpedo
between steps would never be tested. Use the existing `segment_circle_hit` against the swept
segment, for every round, every step. This is not an optimisation detail — a per-step point test
simply does not work at these speeds.

**Own-hull occlusion.** A stern turret can fire through its own ship. Before firing, test the aim
segment against the firing ship's own compound collider out to 2× the hull radius; if it hits, no
shot. Without this a PDC destroys the vessel carrying it.

### 5.3 Torpedoes

Proportional navigation, which is what real interceptors use and what makes them dodgeable:

```
λ̇   = (r × v) / (r · r)                 // 2D scalar cross, LOS rate
V_c  = -(r · v) / |r|                    // closing speed
a_cmd = N · λ̇ · V_c                      // N = 4, perpendicular to LOS
a_cmd = clamp(a_cmd, -a_max, a_max)
```

A torpedo with finite lateral acceleration cannot correct for a target that turns hard late — that
is the whole counterplay, and it falls out of the guidance law rather than being scripted.

Torpedo state: `armed` after 0.5 s and 50 m, `live` while fuel remains, `ballistic` after burnout.
A ballistic torpedo is still lethal and still tracked; it just cannot turn.

### 5.4 Damage

Kinetic, not hitpoint-per-shot:

```
E = ½ m v_rel²
```

A 20 g PDC round at 1100 m/s relative carries 12.1 kJ. Hull damage is `E · k`, `k` tuned so a
sustained PDC burst is a threat but a single round is not. A torpedo carries a fixed warhead plus
its kinetic term.

Damage is applied to the **compound collider shape that was hit**, not to a single hull pool, so a
hit on a drive pod degrades thrust and a hit on a tank vents propellant. This is what makes the
component model (§6.2) pay for itself.

### 5.5 Edge cases

| Case | Handling |
|---|---|
| Firing while docked | Blocked. |
| Rounds in flight during warp | Warp forced to 1× (§2.7). |
| Round despawn | Lifetime 12 s **and** range cap. Both, so neither leaks. |
| Target destroyed mid-flight | Torpedo goes ballistic on its last LOS heading; it does not vanish. |
| Two turrets, one target | Assign by traverse cost; do not let both saturate one target while another is unengaged. |
| PDC vs rock | Same damage path as the cutter. One `apply_damage`, two callers. |
| Zero relative velocity | Lead solve degenerates → direct aim (§5.2). |
| Torpedo at `r → 0` | `λ̇` blows up. Below 5 m, stop guiding and coast — detonation is imminent anyway. |
| Round spawned inside a hull | Spawn at the muzzle plus the hull radius along the barrel, never at the turret origin. |
| Friendly in the line of fire | Same occlusion test as §5.2, extended to any friendly collider. |
| Magazine empty | Turret reads `dry` in the WEAPONS block; it does not silently stop. |

---

## 6. Survey and refit (J9)

### 6.1 Survey

Specified in plan 03 §3.7 and never built. Scan arcs recorded when altitude is within `[h_min,
h_max]` and ground-track speed under a limit. Satellite deployment against an `(a, e)` tolerance
box, checked at release **and** one period later so a marginal orbit fails honestly. Discoveries —
derelicts, anomalous masses, cached depots — found by scanning and recorded permanently on the map.

### 6.2 Modular refit

The flange (plan 04 A6) and the component library exist.

```cpp
struct Component { std::string id, model; Vec2 mount; Real angle; };
struct ShipDesign { std::string name; std::vector<Component> components; };
```

Derived, never authored:

```
mass     = Σ dry_mass
com      = Σ (dry_mass · position) / mass
thrust   = Σ thrust of drives whose axis is within 15° of +Y
torque   = Σ (thrust × (position - com)) for RCS blocks
fuel     = Σ propellant
cooling  = Σ cooling
collider = concat of every component's shapes, in the ship frame
ports    = concat of every component's ports
```

**Acceptance, and it is the point:** the three stock hulls expressed as component sets reproduce
today's `SHIPS[3]` within **2%**. That proves the derivation before any editor exists.

Edge cases: a design with no drive reports `thrust 0` and is flagged, not silently allowed; an
offset centre of mass produces asymmetric torque that the RCS solve must handle (it already works
about `com`, not the origin); overlapping components are rejected at mount time; TWR below 1 at the
intended body is a warning, not an error, because orbital work does not need it.

---

## 7. Phases

| P | Name | Depends | Deliverable |
|---|---|---|---|
| **K0** | Defects | — | S-1 … S-5. |
| **K1** | Audit tooling | — | `export.mjs --audit`, plan raster, material areas, `min_feature`, both silhouette gates. **Before any model is touched.** |
| **K2** | Framing + camera | — | `FLIGHT_HALF` 200, belt retune, 30° cone with ease-back, collar offset index. |
| **K3** | Continuous scale | K2 | §2.1–2.4: exponential zoom, scaled near/far, deep pass, overlay orbit lines. The map screen is deleted. |
| **K4** | LOD + icons | K3 | §2.5, §2.6: pixel-size selection with hysteresis, blocky LOD, icon set, declutter. |
| **K5** | Kestrel | K1 | One model through every gate. The pattern for the rest. |
| **K6** | Mule + Needle | K5 | Pairwise IoU below 0.70. |
| **K7** | Components + structures | K5 | The rest of the library under the same rules. |
| **K8** | HUD | K0, K3 | §4: blocks, `live()`, formatter, collar vectors, density 2 default. |
| **K9** | Close quarters | K2 | §5.1 manoeuvring. |
| **K10** | Combat | K9, K8 | §5.2–5.5: PDC, torpedoes, damage by component. |
| **K11** | Survey | K3 | §6.1. |
| **K12** | Refit | K7 | §6.2. Stock hulls within 2%. |

**K1 before K5.** Re-authoring models without the audit is how this recurs. **K3 before K8**,
because the HUD's blocks depend on what scale means.

---

## 8. Review gates

Plans 03 §8 and 04 §5 carry, plus:

1. `export.mjs --audit` passes for every model: no material region under 6 m² in plan view, no
   feature under 1.5 m, triangle budget met.
2. Pairwise silhouette IoU across kestrel / mule / needle all below 0.70; perimeter²/area ≥ 22 each.
3. A home-framing screenshot in which the teal and ochre bands are visible **without zooming the
   image**. If they cannot be seen, the phase is not done.
4. Zoom continuity: sweep the wheel from 5 m to 5e10 m and capture every 10 notches. No object may
   jump in position or size at any frame, and specifically not at the `far` boundary — §2.3 makes
   that transition exact, so any jump is a bug in the implementation, not a tuning problem.
5. Camera: Ctrl+drag reaches exactly 30° from home and no further, in every direction, returns home
   within 0.5 s. Elevation stays in `[8°, 88°]`. World north stays within 30° of screen-up.
6. No HUD block renders zeros or `nan`; `live()` governs every one. No block overlaps the hull at
   any zoom. Layout pass clean at 1280×720, 1600×900, 1920×1080, 2560×1440.
7. A PDC round fired at a 2 m target crossing at 1 km/s registers a hit — the swept-segment test,
   verified in `selftest`, not by eye.
8. No turret can damage the vessel carrying it, at any traverse angle. Selftest case.
9. `ShipDesign`-derived stats for the three stock hulls within 2% of `SHIPS[3]`.
10. No economy or contract-market code appears; that is still out of scope.
