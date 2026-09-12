# Opra — reference image prompt pack

For ChatGPT web image generation. Output goes to `research/refs/` and becomes the modelling
reference for the three.js authoring agent.

Method follows `3d-sprite-prompting-collection.md`: frozen style block on every prompt (§3, §6),
one asset per image (§9), white background for clean LLM analysis (§9.1), reference-image
anchoring after the first good render (§6.1).

---

## How to run this

1. Generate **P00** first. It is the anchor for everything else.
2. Generate **P01**. Iterate until the hull language is right — this sets the whole fleet.
3. From P02 onward, **attach P01's image** and prepend:
   > Image 1 is the style reference. Match its material treatment, lighting, palette, weathering
   > and level of detail exactly. Different subject, same shipyard.
4. One change per re-roll. Re-running beats asking for a repair (§4).
5. Save as `research/refs/<id>-<name>.png`, e.g. `research/refs/01-kestrel.png`.

Each prompt = **S + V + N + Body**. Paste the blocks verbatim; they must not drift.

---

## Block S — style (identical in every prompt)

```
Industrial hard science-fiction spacecraft in the visual logic of The Expanse: no aerodynamic
streamlining, the hull is structure rather than skin. Exposed frames, bolted plate seams, weld
lines, machined fittings, cable runs and conduit. Utilitarian weathering: scorch aft of thrusters,
micrometeorite pitting, paint worn through at handholds and hatches, mismatched replacement panels.
Matte painted metal and ceramic. No chrome, no neon, no glowing seams, no decorative lighting.
Palette: hull plate #BAC4C3, light plate #E2E3D8, shadowed structure #202E38, bare metal #667681,
copper trim #C88755, black recess #0C141A, glass #376C7C, teal marking #326B70, ochre marking
#BC783C. 3D render, hard-surface modelling, soft-box studio lighting, ambient occlusion, sharp
edges, crisp focus.
```

## Block V — views, vehicles

```
Orthographic technical turnaround: one image, 2x2 grid of four aligned views — top-down plan,
port-side profile, nose-on front, and rear three-quarter at 30 degrees elevation. All four at
identical scale, identical lighting, centred in their cells. Pure white background #FFFFFF.
```

## Block V2 — views, components

```
Orthographic turnaround: one image, three aligned views in a row — front, side, three-quarter at
30 degrees. Identical scale and lighting. Place a plain 1-metre reference cube beside the object.
Pure white background #FFFFFF.
```

## Block N — negatives (every prompt)

```
No perspective distortion, no cast shadows on the background, no text, no labels, no dimension
lines, no arrows, no callouts, no watermark, no logos, no insignia, no human figures, no planets,
no starfield, no lens flare, no motion blur, no background clutter.
```

---

## The one constraint that is not negotiable

The game is played on a plane seen from **32 degrees above**. The **top-down plan view is the
gameplay silhouette** — it is what the player actually reads, every frame. Every vehicle prompt
ends with:

```
The top-down plan view must be an instantly recognisable silhouette, distinct from the other ships
in the fleet, with the nose direction unmistakable at a glance.
```

Call this **Block T**. It goes on P01–P03 and P20–P22.

---

## P00 — material and palette board

*No Block V. Run this first; it anchors the whole set.*

```
[Block S]
A flat lay of nine rectangular spacecraft hull sample plates arranged in a 3x3 grid, seen straight
on. Each plate is roughly 30cm square and shows one material at close range: painted hull plate,
light ceramic plate, shadowed structural alloy, bare machined metal, copper heat trim, black
recessed grating, tinted armoured glass, teal painted marking band, ochre painted marking band.
Each plate shows real surface history — bolt heads, a weld seam, a scuff, edge wear, a thermal
stain. Even frontal studio lighting.
[Block N]
```

---

## P01 — Kestrel, light corvette · 46 m

```
[Block S]
A 46-metre independent patrol corvette. Nose-forward hull with a blunt armoured prow carrying two
forward torpedo tubes; a narrow structural spine; twin fusion drive bells at the stern. Crew decks
stack perpendicular to the thrust axis, read as transverse ring frames and small rectangular ports
along the flank. Two point-defence cannon turrets in recessed dorsal blisters, one ventral. Two
folding radiator panels along the spine. A docking collar amidships on the port flank. Four RCS
thruster quad blocks at the shoulders and hips. Sized for a crew of four.
[Block V]
[Block T]
[Block N]
```

## P02 — Mule, heavy salvage tug · 58 m

```
[Block S]
A 58-metre heavy salvage tug. Open load-bearing chassis with exposed longitudinal rails rather
than a closed hull. Three modular cargo pods clamped along each flank. A forward-set pressurised
cab with a wide segmented canopy. Four drive bells in a protected square cluster at the stern.
A heavy towing yoke and two folding grapple arms at the bow. Oversized radiator panels. One
dorsal point-defence turret. The most worn ship in the fleet: mismatched replacement panels,
patched plate, decades of service.
[Block V]
[Block T]
[Block N]
```

## P03 — Needle, reconnaissance cutter · 31 m

```
[Block S]
A 31-metre reconnaissance cutter. Long slender fuselage, one oversized fusion drive bell at the
stern that dominates the rear third. A forward sensor needle on the centreline and two swept
radiator blades angled off the flanks. Minimal armour with framing left exposed. A single ventral
point-defence turret. A small two-person cabin set well forward. Built for delta-v, not for
fights: everything is subordinate to the drive.
[Block V]
[Block T]
[Block N]
```

---

## Components

These become the modular ship parts. Each is a separate mesh with its own mount.

## P10 — point-defence cannon turret

```
[Block S]
A recessed point-defence cannon turret, 1.8 metres across the blister. A rotating armoured dome
set flush into a hull recess, twin rapid-fire barrels on an elevating cradle, ammunition feed
housing below the deck line, ejection port to one side. Shown deployed with the barrels traversed
30 degrees and elevated 20. Scorching at the muzzles, bare metal worn at the traverse ring.
[Block V2]
[Block N]
```

## P11 — main fusion drive

```
[Block S]
A fusion drive assembly, 5 metres long. Expansion bell with regenerative cooling channels running
its length, a gimbal ring at the throat, turbopump and plumbing wrapped around the upper housing,
a mounting flange at the top. Heat discoloration graduating from straw to deep blue toward the
bell lip. Shown cold, not firing.
[Block V2]
[Block N]
```

## P12 — RCS thruster quad

```
[Block S]
A reaction control thruster quad block, 1.2 metres across. Four small nozzles on a shared armoured
mounting box, two facing outboard and two fore-and-aft, propellant lines entering from the rear,
a service hatch on the face. Soot staining radiating from each nozzle.
[Block V2]
[Block N]
```

## P13 — docking collar

```
[Block S]
A spacecraft docking collar, 2.4 metres across the ring. An armoured outward-facing ring with a
compliant seal face, four latching lugs at 90 degrees, guide petals, an umbilical connector block
to one side, and a small inspection window. Wear polished into the seal face and the lugs.
[Block V2]
[Block N]
```

## P14 — radiator panel

```
[Block S]
A deployable spacecraft radiator panel, 6 metres long. Thin flat panel with fine parallel coolant
channels, a hinged root with an actuator, and a manifold at the base. Shown twice in the same
image: fully deployed, and folded flat against its root. Faint thermal discoloration across the
panel face.
[Block V2]
[Block N]
```

## P15 — landing leg

```
[Block S]
A spacecraft landing leg, 3.5 metres extended. Three segments with a telescoping shock strut, a
hinged upper mount, and a broad articulated footpad with a compliant sole. Shown twice: extended,
and retracted against the hull line. Dust and scoring on the footpad.
[Block V2]
[Block N]
```

## P16 — sensor mast

```
[Block S]
A spacecraft sensor mast, 4 metres tall. A parabolic dish on a two-axis gimbal, a feed horn at the
focus, a phased-array flat panel mounted below the dish, cable runs down a lattice mast to a base
flange. Clean and unweathered relative to the hull — recently replaced.
[Block V2]
[Block N]
```

## P17 — modular hull section family

```
[Block S]
Four modular spacecraft hull components shown side by side at the same scale, each 4 metres long
with identical end flanges so they can be bolted into a stack: a cylindrical propellant tank with
insulation blanket, a pressurised habitat section with two small ports and a hatch, an open cargo
pod frame with a clamped container inside, and a plain structural truss spacer.
[Block V2]
[Block N]
```

## P18 — torpedo tube

```
[Block S]
A spacecraft torpedo launch tube assembly, 4 metres long. A square armoured muzzle block flush to
the hull, a hinged blast door, the tube body behind it with a loading mechanism and a rack of two
reload rounds alongside. Shown with the blast door open. Scorching at the muzzle.
[Block V2]
[Block N]
```

---

## Stations and structures

## P20 — Wayfarer, orbital station

```
[Block S]
An orbital station, 180 metres across. A slowly rotating habitat torus on four spokes around a
non-rotating central spine, with docking arms extending from each end of the spine carrying four
docking collars. Solar wings and a radiator farm on the anti-sun side. Propellant tanks clustered
at the spine's base. Approach lighting strips along the docking arms. Built in stages over
decades: the newest module is visibly newer than the oldest.
[Block V]
[Block T]
[Block N]
```

## P21 — Lagrange base

```
[Block S]
A large deep-space station, 400 metres long, the biggest structure in the setting. A long open
truss backbone, a rotating habitat ring a third of the way along, an industrial processing block
with ore hoppers at one end, four docking arms, enormous radiator wings, and a solar array farm.
Unfinished construction at one end with exposed framing and a construction gantry still attached.
[Block V]
[Block T]
[Block N]
```

## P22 — surface base and landing pad

```
[Block S]
A small planetary surface base on bare regolith, 60 metres across. Two flat circular landing pads
with hold-down clamps and approach lights, a low pressurised habitat of linked cylindrical modules
half-buried under a regolith berm, a propellant plant with two vertical tanks, a communications
mast, and a short covered walkway connecting pad to habitat. Dust weathering up the lower metre of
every surface.
[Block V]
[Block T]
[Block N]
```

---

## Props

## P30 — relay satellite

```
[Block S]
A small deployable relay satellite, 2 metres across the bus. A compact hexagonal bus, two folding
solar wings, a high-gain dish on a gimbal, a whip antenna, and a small station-keeping thruster
cluster. Shown twice: stowed folded for carriage, and fully deployed.
[Block V2]
[Block N]
```

## P31 — navigation buoy

```
[Block S]
A navigation buoy, 1.5 metres tall. A weathered cylindrical body on a small triangular frame, a
strobe housing at the top, a radar reflector, a solar collar around the body, and a grab handle
for retrieval. Heavily faded ochre marking band.
[Block V2]
[Block N]
```

## P32 — derelict wreck

```
[Block S]
The wreck of a 40-metre freighter, dead in space. The hull is torn open amidships exposing deck
frames and severed conduit; one drive bell is missing entirely and its mount is sheared; plating
is peeled back around a blast scar; a radiator panel hangs from one hinge. Scorched, cold, long
abandoned. Structurally readable — it is obvious what the ship was.
[Block V]
[Block N]
```

---

## What to send back

Per asset: the four-view turnaround at the highest resolution available, named
`research/refs/<id>-<name>.png`.

Flag any where the **top-down plan view** is weak — that view is the gameplay silhouette and it is
the one that matters most. A beautiful three-quarter with a mushy plan view is a failed render.

Priority order, if you are not generating all of them: **P00, P01, P02, P03** (the fleet sets the
language), then **P10, P11, P12** (the parts that appear on every ship), then **P20**, then the
rest.
