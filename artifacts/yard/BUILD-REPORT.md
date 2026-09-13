# Shipkit build report — 15 production modules, 3 assembled hulls

Answers the handoff at `handoff-opra-shipkit-models.md`. Nothing has been exported to GLB.

---

## 1. What shipped

15 parts in `parts/`, 3 designs in `designs/`, all authored on `prims.ts` primitives and the
`prims.ts` palette only. Each part exports `build()`, `maps` (a `standard_maps()` with a unique
seed in the 70x range) and `meta`, carries a flange at −Y on the origin, and sits inside the
120–400 triangle band for its own geometry.

| variant | module | kind | own tris | span | note |
|---|---|---|---|---|---|
| A | `nose_hammerhead` | nose_command | 384 | 5.20 m | CIC sponsons, armour deck, radar faces, torpedo bay |
| A | `section_combat_a` | section_weapons | 288 | 3.48 m | hex hull, dorsal armour spine, PDC on opposed quadrants |
| A | `section_tank_saddle` | section_tank | 324 | 3.70 m | 2.6 m waist, conformal saddle tanks, airlock |
| A | `section_radiator_wing` | section_utility | 304 | 6.03 m | radiators on booms, slot open, wingtip RCS |
| A | `drive_twin_torch` | drive_fusion | 376 | 3.36 m | twin bells, two-stage metal→dark, cyan throats |
| B | `nose_pushbow` | nose_armored | 284 | 3.99 m | push-knee on rams, cushions, elevated work cab |
| B | `section_freight` | section_cargo | 272 | 6.35 m | two ISO containers on twist-locks, 0.5 m slot |
| B | `section_machinery` | section_utility | 304 | 6.81 m | octagonal core, canted outrigger banks |
| B | `section_reactor` | section_tank | 272 | 3.54 m | drum half-proud on the deck, crown manifold |
| B | `drive_quad_block` | drive_fusion | 380 | 4.20 m | 2×2 bells, amber confinement glow |
| C | `nose_stealth_needle` | nose_sensor | 192 | 2.63 m | three faceted steps, recessed optics |
| C | `section_delta_fore` | section_weapons | 204 | 5.26 m | swept chines, flush canopy |
| C | `section_stealth_combat` | section_weapons | 272 | 5.64 m | flush cells, conformal PDC bays |
| C | `section_delta_aft` | section_utility | 236 | 7.22 m | wings on pylons, slot open, vent grilles |
| C | `drive_stealth_twin` | drive_fusion | 368 | 5.43 m | recessed hex nozzles, 1.3 m stern notch |

## 2. Gates

Designs, against PLAN-07 §5, with the approved prototypes measured the same way for comparison:

```
                        complexity >= 60   section CV >= 0.20   top material <= 45%
proto A (approved)          69.4 ok            0.19 FAIL        lightArmor 63% FAIL
proto B (approved)          93.4 ok            0.24 ok          metal      44% ok
proto C (approved)          50.0 FAIL          0.36 ok          lightArmor 51% FAIL

variant_a_hammerhead        78.3 ok            0.23 ok          dark       36% ok
variant_b_ingot             94.2 ok            0.22 ok          metal      43% ok
variant_c_waverider         91.6 ok            0.26 ok          dark       41% ok
```

Plan-view width per station (metres), nose band first:

```
A   5.3  5.0  4.0  6.0  3.3  3.3
B   4.0  6.3  6.7  6.7  4.3  4.3
C   2.7  5.3  5.7  7.3  6.0  5.3
```

Pairwise silhouette IoU (PLAN-07 gate 6, max 0.70): **A/B 0.66, A/C 0.68, B/C 0.64**.
Every part passes the triangle band, the flange check, the material share (with `section_tank` and
the radiator-bearing `section_utility` modules declaring `exempt: ['top_material']` per §5), and a
smoke test that draws all three map channels.

Three fixes did most of the work, and all three are structural rather than cosmetic:

- **Open slots.** The prototype's parts all touched. Standing the radiators, the containers and the
  delta wings clear of the hull on booms and twist-locks — which is how each of those actually
  mounts — is what took C from 50.0 to 91.6 and B from 93.4 to 94.2. A panel welded to a flank
  contributes three sides of perimeter; one standing off contributes four and removes area.
- **A real waist.** A's tank station cut to 2.6 m against a 6.0 m radiator station moved its CV
  from 0.19 to 0.23.
- **Dark on top.** Plan view is z-buffered, so the topmost surface is the one that counts. Dorsal
  armour decks, heat-shield decks and machinery trunks put the second material where it is measured
  and took A's `lightArmor` from 63% to `dark` 36%.

## 3. Deviations from the prototype, and why

Everything else preserves the approved proportions and material assignments. Four things changed:

1. **Radiator panels are canted plates, not vertical vanes** (A, and the same reasoning in B).
   The prototype's vanes were 0.08 m thick in X. At the 32° game camera and in the plan raster that
   is a hairline: the station the design is *named* for contributed 0.3 m² and no silhouette. They
   are now 1.1 m plates canted 17° off the play plane, keeping the 5.8–6.0 m span the approved
   build called for.
2. **Hex and octagon hulls are rolled 30°/22.5° about Y.** Three.js puts a cylinder vertex at +Z, so
   an unrolled hex hull is a ridge and the ship reads as a pipe. Rolled, it is a flat dorsal deck
   with a chine at each flank — which is what "wide flat submarine body" means, and it gives the
   dark armour spines something to sit on.
3. **Module bodies are 3.94 m inside a 4.00 m station.** Butted flush, every joint was two coplanar
   end caps fighting for the depth buffer; it striped the hull in the preview and would do the same
   in the engine. A 60 mm seam is sub-pixel at the home framing's 3 px/m and the flange sits in it.
4. **B's amber glow comes from `prims.lamp_material('#f0922e')`.** The palette carries one glow and
   it is cyan; "amber, not cyan" is one of B's five named beats. This is prims' own helper for
   coloured lamps, not a hand-authored material, and it is named `glowAmber` so `meta.untextured`
   can reach it.

Minor, same category: `bevelled()` with a sub-0.5 m radius is used where a box needs a rotation
about Y — it degrades to a plain `BoxGeometry` at prims.ts:915, so it costs the same 12 triangles
as `box()`, which only rotates about Z.

## 4. Two things in the spec that cannot both be true

Neither is a judgement call I should make alone, so both are left as they are and reported.

**The triangle budget and `flange()`.** PLAN-07 §5 sets 120–400 triangles per part, and §3.1 says
every part carries `flange()` and not to re-author it. `flange()` costs **920 triangles**: a
32-segment ring plate (128), a 32×8 torus (512), eight 8-segment bolt bosses (256) and two keys
(24). The budget is unreachable while the rule holds. Measured:

```
part own geometry   192 - 384      inside the band
plus the flange    1112 - 1336     3x over it
a 5-part design    5700 - 6400     against the 1200-4000 the plan expects
```

At the home framing a 2.5 m flange is 7.5 px, so those 920 triangles are invisible. The choices are
to exempt the flange from the per-part budget, or to re-author `flange()` at 8–12 segments (it would
cost about 180 and look identical past 10 m). `partgate.mjs` reports both numbers so the decision
can be made on the arithmetic. **Nothing has been changed in `prims.ts`.**

**The minimum-feature floor and the approved design language.** Plan 05 §3.2's 1.5 m floor is
checked per mesh on its smallest AABB dimension, and the exporter's `enforce_min_feature` *grows*
anything under it by `(1.5/smallest)²` before auditing. Applied to these hulls it would inflate
every radiator plate, every hazard panel, every nav light and — this is the one that matters — the
flange's own 0.10 m ring plate, which is the smallest feature in all fifteen parts. The approved
prototypes have exactly the same property (their smallest features are 0.05–0.08 m). The floor is
right for a 60 m hull at 3 px/m and wrong for a 4 m module measured on its own; it probably wants
to apply to the *assembled design* like complexity and CV do, or to take a per-part opt-out. Until
that is settled the parts are authored to the approved proportions and the bench reports the floor
without enforcing it.

**A third, smaller one: `FLANGE_RADIUS` against a flat hull.** The flange is a 2.5 m disc centred on
the part origin, and C's hulls are 1.45-1.75 m deep, so the ring stands about 0.45 m proud of the
deck and the keel at every joint and reads as a collar in the iso view. A's tank and radiator
sections do the same by a smaller margin. It is visible in the previews and it is why every C module
measures exactly 2.50 m deep - the flange, not the hull, is setting that variant's envelope. It is
defensible as-is (a modular ship showing its interface rings is the point of the architecture), and
the alternatives both cost something: deepen the flat variants to 2.5 m and C stops being the
low-profile one, or drop `FLANGE_RADIUS` for section modules and the kit has two interfaces. Left
alone, flagged.

## 5. Previews

`out/variant_{a,b,c}_*.png`, four views each: `game` (the engine's 32° oblique — `render.mjs` uses
`CAMERA_FOV_Y` and `CAMERA_PITCH_DEFAULT` and `orbit_eye()`'s offset directly, so it is the player's
framing), `plan` (what the gates measure), `iso`, `side`.

They are rendered by `render.mjs`, a z-buffered software rasteriser in the bench, because playwright
cannot launch chromium in this environment — headless shell, `channel: chrome` and headed all time
out at `<launched>`. `shipkit.html` + `shipkit_entry.ts` is the interactive equivalent and is built
and ready (`bun run server.mjs`, then `/shipkit.html?variant=A`) for a machine where the browser
works.

Untextured, like the approved prototype: the `maps` are baked by `tools/export.mjs` at GLB time, so
what is on screen is the geometry and the palette — which is exactly what the gates measure.

## 6. Not done, on purpose

- **No GLB export.** Waiting on the word, per the handoff.
- **No `tools/models/` graduation.** Parts stay in the yard until the two §4 questions are settled;
  the yard README's rule is that a part graduates once it clears the bench, and the flange question
  decides what "clears" means for the triangle gate.
- **No sidecar collider/port/hardpoint authoring** beyond `collider: 'auto'`. PLAN-07 §6.1's schema
  wants `ports` and `hardpoints` on the PDC and docking modules; that is P3 work and depends on the
  runtime composition landing first.
