> **Status: complete.** The live plan is [PLAN-05-legibility.md](PLAN-05-legibility.md).

Live plan. [PLAN-01](PLAN.md), [PLAN-02](PLAN-02-system.md) and [PLAN-03](PLAN-03-worlds.md) are
complete; their decisions bind.

Plan 03 built the machinery: PBR materials with tangents and UVs, shadow/bloom/tonemap/planet/star
shaders, orbital mechanics, the director, the layered HUD. **The machinery has no content.** There
are no textures, the planets are shaded spheres with nothing on them, and the ships are the
placeholder hulls ported from a browser game three plans ago.

This plan fills it, using the 29 reference images now in `research/refs/`.

---

## 0. Reference audit

29 images generated against `research/prompts/`. I read every one and measured the textures
numerically (`tools/check_refs.py`).

### What is usable as-is

| id | asset | verdict |
|---|---|---|
| `00` | material board | **Definitive.** All nine materials, clean separation, flat lighting. This is the palette of record. |
| `01` | Kestrel corvette | **Excellent.** Ogival armoured prow, twin drive bells, four RCS quads at the hull corners, dorsal turret blisters, teal + ochre bands. Plan-view silhouette reads instantly. |
| `03` | **Mule tug** (mislabelled — see below) | **Excellent.** Open rail chassis, six cargo pods, forward cab, four-bell square cluster, towing yoke and grapple arms. |
| `10` | PDC turret | Twin barrels on an elevating cradle, ejection port, base ring with exposed copper plumbing. |
| `11` | fusion drive | Regeneratively cooled bell, straw-to-blue heat gradient, gimbal ring, turbopump. Switched off, as asked. |
| `12` | RCS quad | Four nozzles in a **cross in one plane** — which is exactly right for a 2D gameplay plane. |
| `13` | docking collar | Dark seal face, 4 latching lugs at 90°, 4 guide petals on the diagonals, umbilical block with window. |
| `14`, `15`, `30` | radiator, landing leg, relay satellite | Two-state layout worked on all three: deployed and stowed, same scale, same views. |
| `16`, `18`, `31` | sensor mast, torpedo tube, nav buoy | Clean, complete, scale cube present. |
| `17` | hull modules | **The most valuable sheet in the set.** Four modules with *identical bolted flanges at both ends*. That flange is the interface the modular ship system has been waiting for since plan 02 §7. |
| `20`, `21`, `22` | Wayfarer, Lagrange base, surface base | All three good. `22`'s plan view is directly usable as the landing-pad layout. |
| `32` | derelict | Torn open amidships with exposed frames, one drive bell sheared away, hanging radiator. |
| `41`, `42`, `43` | carbonaceous, ore-vein, regolith | Tile cleanly on both axes. |
| `51`, `53`, `54`, `55` | Tessera albedo, night lights, Vesk, Halberd | Wrap correctly, no baked lighting. |

### Defects

| id | defect | measurement | action |
|---|---|---|---|
| **`02`** | **Missing.** The needle cutter was never generated, and the mule was saved as `03`. | — | Rename `03.png` → `02-mule.png`. Re-run `research/prompts/03-needle-cutter.txt`. |
| **`50`** | Cinder: seam at longitude 0/360. | L/R edge difference 14.44 against a 6.09 local-noise baseline — 2.4× worse than the image's own texture. A visible line down the planet. | Seam-repair pass (§3.3), or regenerate. |
| **`52`** | Tessera clouds: worse seam. | 22.31 against 8.31 — 2.7×. | Same. |
| **`56`** | Star photosphere: tiles left-right but **not** top-bottom. | T/B 12.85 against 4.46 — 2.9×. | Wrap-repair on the vertical axis only. |
| `40` | Silicate rock tiles, but marginally. | 20.61 / 13.12 = 1.57×, just inside tolerance. | Accept; offset-blend if a seam shows at grazing angles. |
| all ships | Hull is greyer and less blue-green than the palette asked for. | Dominant hull tone `#9B9D98`; the brief said `#BAC4C3` — a distance of 66 in RGB. `#5C5F5D` where `#667681` was asked (44). | Not a defect to fix in the images. **Take the observed values as the palette of record** and retune `tokens.h` and the material table to match, so ships and UI agree. |

Two things I checked that came back **clean**, against my expectation:

- **No baked lighting anywhere.** My first test flagged all six planet maps as lit. It was wrong —
  it measured column-brightness spread, which cannot tell "continents clustered on one side" from
  "sun on one side". The correct test is that a sun on an unrolled sphere is exactly *one cosine in
  longitude*, so it dominates the first harmonic of the column-brightness spectrum while geography
  spreads across many. Re-measured that way: the highest first-harmonic share is 41% (Cinder) with a
  peak-to-trough of 7.5/255, which is terrain, not a terminator. All eleven pass.
- **Every equirectangular map came back exactly 2:1** (1774 × 887) and every tile exactly square.

`tools/check_refs.py` runs both tests plus a palette census; keep it as the gate for any future
batch. Run it with the Windows interpreter — `py` honours the shebang and dispatches to msys2's
python, which has no numpy.

---

## 1. Decisions

| # | Decision | Consequence |
|---|---|---|
| **A1** | **The reference images are the design of record.** Where an image and a prompt disagree, the image wins. | §2's spec tables are read off the images, not copied from the prompts. |
| **A2** | **The observed palette replaces the specified palette.** | `ui/tokens.h` and the material defaults retune to what `00.png` and the hulls actually contain. UI and hull stop disagreeing. |
| **A3** | **Textures load as standalone PNG**, not only through glTF. Add `stb_image` (already in vcpkg). | Planet maps, rock tiles and the star photosphere are not model assets and must not be forced into a GLB to get on the GPU. |
| **A4** | **Camera: fixed pitch, plane-locked pan.** Ctrl+mouse Y no longer drives pitch *and* pan on the same axis. | §3.1. This is the "doesn't feel natural" fix. |
| **A5** | **Digits are monospaced at the call site, not inside the text engine.** | §3.2. Deletes the quad-reconciliation machinery that is currently dropping glyphs. |
| **A6** | **`17`'s flange is the modular interface.** One flange geometry, one diameter, one bolt pattern, shared by every component that stacks. | Plan 02 §7's `ShipDesign` finally has a physical connector. |

---

## 2. Asset specifications

Read off the reference images. Positions are **fractions of overall length from the nose (0.0) to
the stern (1.0)**, and **fractions of half-width from the centreline**. The modelling agent builds
to these numbers, using the image for surface character.

### 2.1 Kestrel — light corvette

Hull 46 m. Plan-view proportions as drawn: **L : W : H = 3.1 : 1 : 0.62**, so 46 × 14.8 × 9.2 m.
(The prompt asked 4:1; the image came back 3.1:1 and the image wins — A1.)

| Feature | Along | Lateral | Notes |
|---|---|---|---|
| Armoured prow | 0.00–0.18 | ±0.55→±1.0 | Ogival in plan, faceted in section. Blunt, not pointed. |
| Teal band + ochre stripe | 0.13 | full width | The identity marking. Teal aft of ochre. |
| Dorsal turret rings ×2 | 0.20, 0.28 | ±0.25 | `hp.pdc.fwd`, `hp.pdc.aft` |
| Ventral turret ×1 | 0.42 | 0.0 | `hp.pdc.ventral` |
| Open dorsal bay | 0.30–0.62 | ±0.62 | 8 transverse rails. Flat radiator panel recessed in the floor. |
| Machinery spine | 0.62–0.82 | ±0.55 | Exposed plumbing, greebled. |
| Drive bells ×2 | 0.82–1.00 | ±0.22 | Asset `11`, scaled. Splayed 4° outboard. |
| RCS quads ×4 | 0.08, 0.88 | ±0.44 | Asset `12`. Nozzle cross lies **in the gameplay plane**. |
| Docking collar | 0.46 | −1.0 (port) | Asset `13`. `dock.A`, normal −X. |

Collider, from the plan silhouette — four shapes, not one box:

```
box   hull      half (0.50L, 0.34W)  @ (0.0,  +0.04L)
box   prow      half (0.09L, 0.22W)  @ (0.0,  +0.41L)
box   port pod  half (0.09L, 0.14W)  @ (-0.30W, -0.41L)
box   stbd pod  half (0.09L, 0.14W)  @ (+0.30W, -0.41L)
bounds circle   r 0.52L
```

### 2.2 Mule — heavy salvage tug

Hull 58 m. Proportions as drawn: **2.45 : 1 : 0.70**, so 58 × 23.7 × 16.6 m.

| Feature | Along | Lateral | Notes |
|---|---|---|---|
| Grapple arms ×2 | 0.00–0.16 | ±0.75 | Folding, three segments. Open in the reference. |
| Towing yoke | 0.06–0.14 | ±0.30 | Between the arms. |
| Cab | 0.16–0.26 | ±0.45 | Segmented canopy, `glass`. |
| Open rail chassis | 0.26–0.80 | ±0.35 | Two longitudinal rails, structure visible through it. |
| Cargo pods 3 per side | 0.32, 0.48, 0.64 | ±0.80 | Asset `17` cargo frame, clamped. |
| Radiator panels ×2 | 0.62–0.80 | ±0.55 | Asset `14`, deployed. |
| Drive bells ×4 | 0.80–1.00 | ±0.20, ±0.20 | 2×2 square cluster. Asset `11`. |
| Dorsal turret ×1 | 0.30 | 0.0 | `hp.pdc.dorsal` |

The Mule is the asset that proves compound colliders: a single box spans its open chassis and
registers hits in gaps you can fly a Needle through.

```
box   spine     half (0.50L, 0.18W)  @ (0.0, 0.0)
box   cab       half (0.06L, 0.45W)  @ (0.0, +0.38L)
box   port pods half (0.20L, 0.25W)  @ (-0.62W, -0.02L)
box   stbd pods half (0.20L, 0.25W)  @ (+0.62W, -0.02L)
box   drives    half (0.10L, 0.30W)  @ (0.0, -0.40L)
bounds circle   r 0.55L
```

### 2.3 Needle — recon cutter

**No reference image.** Re-run `research/prompts/03-needle-cutter.txt` before this is modelled.
Design intent stands: 31 m, 8:1, one oversized drive filling the rear third, forward sensor needle,
two swept radiator blades, one ventral turret. **Do not model it from imagination** — the whole
point of the reference set is that the three hulls read as distinct, and that can only be judged by
putting the three plan views side by side.

### 2.4 Components

Sizes are the observed object-to-scale-cube ratio in each sheet. Where that disagrees with the
prompt, the image wins (A1).

| Asset | Observed size | Mount | Effects | Tris |
|---|---|---|---|---|
| `pdc_turret` (`10`) | ~3.0 m across base ring | Base ring sits **proud** on the hull, not recessed | muzzle flash ×2 | 1200 |
| `drive_main` (`11`) | 5.5 m tall, 2.6 m bell | Circular flange, top | `flame` cone from bell lip | 1800 |
| `rcs_quad` (`12`) | 1.3 m cube | Flat face | `jet.0`–`jet.3`, one per nozzle, **in-plane** | 500 |
| `docking_collar` (`13`) | 4.0 m ring | Ring face | capture pulse | 900 |
| `radiator_panel` (`14`) | 6.0 × 3.0 m | Hinged root + actuator | — | 700 |
| `landing_leg` (`15`) | 3.5 m extended | Hinged top mount | contact dust | 900 |
| `sensor_mast` (`16`) | 4.2 m tall, 2.2 m dish | Base flange | — | 1100 |
| `hull_module` ×4 (`17`) | 4.0 m long, 2.5 m dia | **Shared flange, both ends** | — | 600 ea |
| `torpedo_tube` (`18`) | 5.0 m long | Flush muzzle block | muzzle flash | 800 |

### 2.5 The flange (A6)

From `17`: a circular bolted flange, **2.5 m diameter**, ring width ~0.18 m, with raised bolt bosses
at 8 equal positions and two copper-trimmed alignment keys at 0° and 180°. Every stackable component
carries it at both ends, identical.

`hull_module` variants: `tank` (quilted insulation blanket, external pipe run), `hab` (two round
ports, one rectangular hatch, teal/ochre band), `cargo` (open frame, clamped container), `truss`
(open lattice, nothing between the flanges).

This is the connector plan 02 §7 specified and could not draw. `ShipDesign` mounts become flange
positions, and a component's collider box comes with it.

### 2.6 Structures

| Asset | Observed | Ports | Notes |
|---|---|---|---|
| `station` (`20`) | Torus on 4 spokes, spine through the axis | 2 per arm, 4 total | **Torus lies in the gameplay plane** so the ring reads from above. Solar farm one side. |
| `station_lagrange` (`21`) | Truss backbone, ring at 1/3, processing block one end | 4 along the truss | Unfinished end with gantry — build it as-drawn, the incompleteness is the character. |
| `surface_base` (`22`) | 2 circular pads, walkway, buried habitat, propellant plant | `dock.pad.A`, `dock.pad.B` | Plan view is the pad layout. Origin at pad deck level. |
| `derelict` (`32`) | Torn amidships, 1 of 2 drives sheared | none | Interior frames must be real geometry — you fly past the opening. |
| `relay_satellite` (`30`), `nav_buoy` (`31`) | 2.0 m bus / 1.5 m tall | — | `30` two-state. |

---

## 3. Engine work

### 3.1 Camera feel (A4) — the reported problem

Two distinct faults, both in the same gesture.

**Fault 1: one axis, two coupled effects.** `camera_follow.cpp:46-49`:

```cpp
pitch  = clamp(pitch + delta_px.y * 0.005, ...);   // mouse Y tilts
pan.y -= delta_px.y * metres_per_px;               // mouse Y ALSO pans
```

Drag down and the camera tilts up while the view slides down. They fight. Nothing about the result
is predictable from the gesture, which is precisely "doesn't feel natural".

It is also a **plan-03 F1 violation**: pitch was to be removed from flight and become a settings
value ("no down, only around the same current plane from above"). Remove the pitch term from
`look_step` entirely. Pitch is read once from settings at camera construction.

**Fault 2: the world does not stick to the cursor.** `frame.cpp:224` converts pixels to metres with
a single flat scale:

```cpp
metres_per_px = camera.half_height * 2.0 / height;
```

Under a **tilted** perspective camera that is only true on one screen row. A pixel near the bottom
of the frame covers materially fewer metres of plane than one near the top, so the drag rate is
wrong almost everywhere, and wrong by a different amount depending on where you grabbed.

Correct form — unproject both pointer positions onto the plane and move by the difference:

```cpp
// on press
anchor_world = unproject(camera, pointer, w, h, 0.0);
// each frame while held
now_world    = unproject(camera, pointer, w, h, 0.0);
pan         += anchor_world - now_world;   // double, before any float cast
```

The plane then tracks the cursor exactly, at any pitch and any zoom, because the projection is
doing the work instead of an approximation of it. `unproject` already exists and already returns
`dvec2` (`camera.cpp:42`).

Also replace `pan_release`'s `1/(1 + dt/tau)` with `exp(-dt/tau)` — the current form is only
approximately frame-rate independent, and the camera is the one place plan 03 spent a whole section
getting that right.

### 3.2 Text: digits are dropping (A5)

Reproducible: the speed readout renders **`31.`** — final digit gone. `artifacts/flight3.png`
renders `31.7` from identical sim state at HUD density 3, so the failure depends on what else is in
the glyph atlas.

Cause, `text.cpp:242-262`: the engine walks the string itself to compute per-digit shifts for
tabular figures, then reconciles that walk against SDL_ttf's quads **by sorting the quads on x**.
But SDL_ttf sorts its draw operations by atlas texture, so they arrive permuted, and it silently
drops any glyph whose ink box is empty. When the x-sort fails to recover string order, shifts land
on the wrong glyphs. The `quads == layout.order.size()` guard does not save it: counts can match
while the mapping is still wrong.

Replace, don't repair. Measure the widest digit advance once per font size; for **readouts only**,
emit one `TextDraw` per character at `i * digit_width`. Labels keep normal kerning. ~10 lines
against ~60, and it never touches SDL_ttf's internals.

### 3.3 Texture pipeline (A3)

Add `stb_image` to `vcpkg.json`. New `render/texture.cpp`:

```cpp
/** Loads a PNG into an sRGB or linear GPU texture with a full mip chain. */
SDL_GPUTexture *load_texture(gpu::Device&, const std::string& path, bool srgb);
```

Colour maps (albedo, night lights) are **sRGB**; data maps (normal, roughness, height, cloud mask)
are **linear**. Getting that backwards is the single most common source of "the PBR looks wrong",
and it is invisible until you compare against a reference.

**Mip generation matters here** — a planet is 2 px wide on the map screen and 2000 px on descent,
and without mips the 2 px version is one arbitrarily-sampled texel that flickers as it rotates.

**Seam repair**, for `50`, `52` and `56` (§0): a build step, not a manual edit, so a regenerated
image gets the same treatment. For an equirectangular map, cross-fade a 32-px column band across
the 0/360 wrap; for a tile, do it on both axes. Extend `tools/check_refs.py` with `--fix` to write
repaired copies into `assets/textures/`, and keep the originals in `research/refs/` untouched.

Resize to power-of-two on the way in: 2048 × 1024 for equirect, 1024² for tiles.

### 3.4 Planet and star content

`planet.hlsl` and `star.hlsl` exist and shade procedurally. Wire the maps in:

| Body | albedo | extra |
|---|---|---|
| Cinder | `50` | — |
| Tessera | `51` | `52` cloud layer (scrolling, own rotation rate), `53` night lights masked to `N·L < 0` |
| Vesk | `54` | — |
| Halberd | `55` | — |
| Nereid | `56` | tiled across the photosphere, under the existing limb darkening |

Clouds rotate **faster than the surface** — a fixed offset rate, not a second body. Night lights are
emissive and must survive tonemapping, so they go into the HDR target before bloom, not after.

Terrain stays procedural (plan 03 §3.2). `55` and `50` are albedo only; they must not be used as
height, or the silhouette you land on stops matching the one you saw from orbit.

### 3.5 Palette retune (A2)

Observed, from `tools/check_refs.py`:

| token | specified | observed | action |
|---|---|---|---|
| hull plate | `#BAC4C3` | `#A0A5A3` board, `#9B9D98` hulls | → `#A3A7A4` |
| light plate | `#E2E3D8` | `#F5F4F2` | → `#EDEDEA` |
| shadowed structure | `#202E38` | `#263C43` | → `#263C43` |
| bare metal | `#667681` | `#626464` | → `#646A6C` |
| copper trim | `#C88755` | `#A56233` | → `#A56233` |
| black recess | `#0C141A` | `#1C2022` | → `#171B1E` |
| teal band | `#326B70` | `#275459` | → `#275459` |
| ochre band | `#BC783C` | `#A46F4C` | → `#AE7040` |

The hull came back greyer and less blue-green than asked. Retune `tokens.h` to match rather than
fighting the art, so the HUD and the ships belong to one world.

---

## 4. Phases

| P | Name | Depends | Deliverable |
|---|---|---|---|
| **H0** | Camera + text | — | §3.1, §3.2. Both are reported defects; nothing else matters until the game feels right to drive and the numbers are readable. |
| **H1** | Texture pipeline | — | `stb_image`, `load_texture`, sRGB/linear split, mips, `check_refs.py --fix`, repaired maps in `assets/textures/`. |
| **H2** | Planets and star | H1 | §3.4. Six maps wired, clouds rotating, night lights emissive. |
| **H3** | Palette retune | — | §3.5. `tokens.h` and material defaults. One screenshot per screen, before and after. |
| **H4** | Needle reference | — | Re-run prompt `03`, rename `03.png` → `02-mule.png`, re-run `check_refs.py`. Compare the three plan views side by side and confirm they read as distinct. |
| **H5** | Component library | H3 | The nine components of §2.4 as three.js modules, with flange, hardpoints, effect children and procedural maps. |
| **H6** | Kestrel + Mule | H5 | §2.1, §2.2 rebuilt to the spec tables, with compound colliders and ports. |
| **H7** | Needle | H4, H5 | §2.3. |
| **H8** | Structures | H5 | §2.6: Wayfarer, Lagrange base, surface base, derelict, satellite, buoy. |
| **H9** | Rock tiles | H1 | `40`–`43` on the asteroid and regolith materials, triplanar. |
| **H10** | Modular interface | H5 | Plan 02 §7's `ShipDesign` over the `17` flange. Stock hulls reproduce today's `SHIPS[3]` within 2%. |

**H0 first, and stop there for a look.** The camera is the thing you touch every second.

### Non-goals

Unchanged from plan 03 §6, plus: no new gameplay systems, no hand-painted textures, no second
system, and no surface scene beyond the pads that already exist.

---

## 5. Review gates

Plan 03 §8 carries, plus:

1. `tools/check_refs.py` runs clean on `assets/textures/`: every tile tiles on both axes, every
   equirect wraps, no first-harmonic share above 45%.
2. sRGB vs linear is correct per map. Sample a mid-grey texel through the shader and assert it
   comes back at the value it went in at.
3. Ctrl+drag: the world point under the cursor at press is still under the cursor at release, to
   within 2 px, at pitch 17°, 45° and 88° and at zoom 0.6 and 3.0. This is the test that the pan is
   a real unprojection and not a scale factor.
4. Pitch does not change during flight. Grep `look_step` for any write to `pitch`.
5. `measure("0000") == measure("1111")` for every readout size, and `31.7` renders as four glyphs
   at every HUD density level.
6. Each ship's compound collider is inside its drawn silhouette at every hardpoint — overlay the
   collider in the model viewer (`B`) and compare against the reference plan view.
7. No asset is modelled without its reference image present in `research/refs/`.
