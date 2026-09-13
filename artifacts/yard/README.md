# The yard

Scratch workbench for authoring the spine/part kit (PLAN-07). Bun + three.js, wired to the real
exporter so a part is measured here by the same code that will gate it later.

Nothing in this folder ships. A part graduates to `tools/models/` once it clears the bench.

## Run

```sh
cd artifacts/yard
bun install                          # once; three 0.183.2, same as tools/
bun run gates.mjs parts/spine_truss_m.ts
bun run gates.mjs ../../tools/models/kestrel.ts     # or measure anything already shipping
```

## What the bench measures

`gates.mjs` runs the shipping audit (`tools/export.mjs: audit_legibility` — triangles, minimum
feature, material regions, IoU) and adds the three gates PLAN-07 introduces:

| gate | rule | why |
|---|---|---|
| complexity | ships ≥ **60** | perimeter²/area. A circle is 12.6, a 3:1 rectangle **21.3**. The old floor of 22 sat four points above a rectangle, which is how a slab passed. |
| section variance | ≥ **0.20** | std/mean of plan-view width per station. A constant-section extrusion has nothing to read along its length. |
| top material | ≤ **45%** of plan area | one material owning the plan view *is* "no colour differentiation", measured. |

Baseline, so the numbers mean something:

```
kestrel    26.0 FAIL   0.11 FAIL   lightArmor 61% FAIL
mule      500.6 ok     0.35 ok     dark       31% ok
needle     86.6 ok     0.51 ok     ceramic    34% ok
station  1795.7 ok     1.00 ok     hullPaint  69% FAIL
```

The Kestrel fails all three. That is the whole of "the models are trash", as arithmetic.

Range-based variance does **not** work here and was tried first: it is dominated by the single
narrowest band, so a slab that tapers at one end scores 0.31 while the mule scores 0.73 — no
usable separation. Coefficient of variation gives 0.11 against 0.35, which does separate them.

## Bench vs. exporter: one difference, on purpose

The exporter runs `enforce_min_feature` (grows undersized parts) and `consolidate_regions` (merges
tiny materials into the dominant one) **before** it audits. The bench does not. So the bench reports
what you actually authored, and the exporter reports what would ship after it has been repaired for
you. Expect the bench to be stricter. That is the point — a part repaired by the exporter is a part
whose author never learned anything.

Note the direction `consolidate_regions` pushes the third gate: merging strays into the dominant
material makes dominance *worse*, never better. `station` reads 69% for exactly that reason.

## Layout

```
artifacts/yard/
  package.json      three 0.183.2
  gates.mjs         the bench
  parts/            work in progress; graduates to tools/models/
  designs/          placement lists under test; graduates to assets/designs/
  out/              rasters and sheets, never committed
```

`prims.ts` is not copied here — import it across (`../../tools/models/prims`) so there is one
library and a part written in the yard needs no edits when it graduates.

---

## The three production hulls (PLAN-07 section-module kit)

`parts/` carries 15 modules — five per approved variant — and `designs/` chains each set on the
4 m station pitch. Every module is authored flange-at-origin with the body toward **+Y**; a section
is placed at the station it starts from, and the drive is placed at the aft cap with `rotZ(pi)`, so
its plume leaves at ship −Y (PLAN-07 §3.4).

```
variant_a_hammerhead   drive_twin_torch  section_radiator_wing  section_tank_saddle  section_combat_a       nose_hammerhead
variant_b_ingot        drive_quad_block  section_reactor        section_machinery    section_freight        nose_pushbow
variant_c_waverider    drive_stealth_twin section_delta_aft     section_stealth_combat section_delta_fore   nose_stealth_needle
```

### What the gates say

Designs, against PLAN-07 §5's three floors — and against the approved prototypes measured the same
way, which is where the work was:

```
                        complexity >= 60   section CV >= 0.20   top material <= 45%
proto A (approved)          69.4 ok            0.19 FAIL        lightArmor 63% FAIL
proto B (approved)          93.4 ok            0.24 ok          metal      44% ok
proto C (approved)          50.0 FAIL          0.36 ok          lightArmor 51% FAIL

variant_a_hammerhead        78.3 ok            0.23 ok          dark       36% ok
variant_b_ingot             94.2 ok            0.22 ok          metal      43% ok
variant_c_waverider         91.6 ok            0.26 ok          dark       41% ok
```

Pairwise plan silhouette IoU (PLAN-07 gate 6, max 0.70): A/B 0.66, A/C 0.68, B/C 0.64.

The prototype numbers are reproduced by copying `buildVariantA/B/C` out of `prototype_entry.ts`
into a scratch module with a `build` and a `meta` export — the file itself cannot be imported
headless, it runs DOM code at module scope.

### Bench commands

```sh
bun run gates.mjs   designs/variant_a_hammerhead.ts     # the three design gates
bun run partgate.mjs parts/nose_hammerhead.ts           # the per-part gates gates.mjs has no home for
bun run iou.mjs     designs/*.ts                        # pairwise silhouette distinctness
bun run render.mjs  designs/variant_a_hammerhead.ts     # rasters into out/, four views
bun run render.mjs --view game --size 1500x940 designs/variant_b_ingot.ts
```

`partgate.mjs` covers what `gates.mjs` does not, because PLAN-07 §5 assigns complexity and section
CV to *designs* and the triangle band, the flange and the material share to *parts*: the 120–400
triangle budget, the flange at −Y on the origin, the top-material share with the sidecar's
`exempt: ['top_material']` honoured, and a smoke test that draws all three `maps` channels so a bad
stencil coordinate fails here rather than at GLB time.

`render.mjs` draws the previews without a browser — playwright cannot launch chromium in this
environment, every channel times out at `<launched>`. It is a z-buffered software rasteriser using
the engine's own camera constants (`render/camera.h`'s `CAMERA_FOV_Y` and `CAMERA_PITCH_DEFAULT`,
`camera.cpp`'s `orbit_eye`), so the `game` view is the framing the player gets and not an
approximation of it. `shipkit.html` + `shipkit_entry.ts` is the interactive viewer for the same
three designs if a browser is available (`bun run server.mjs`, then `/shipkit.html?variant=A`).
