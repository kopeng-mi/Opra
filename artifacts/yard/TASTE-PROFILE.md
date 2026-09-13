# Opra Shipkit — Visual Taste Profile
## *Expanse-Class Hard Sci-Fi Design Language*

> **Purpose:** Reference document for all `artifacts/yard/parts/*.ts` authors.
> Every geometry decision should trace back to a rule in this file.

---

## 1. Core Form Principle: "Office Tower in Space"

Expanse ships are **vertical skyscrapers oriented engine-down**. Thrust = gravity.
Decks stack perpendicular to the thrust axis. This is the single most important
structural constraint and the reason Expanse ships look nothing like Star Wars
or Star Trek.

**For our kit:** The Y-axis is the thrust axis. Engine at +Y (aft/bottom under
thrust), nose at −Y (fore/top). All parts stack along Y via flanges. The ship
reads as a tall, narrow column — not a wide airplane.

---

## 2. Hull Language: Faceted Panels, Not Smooth Curves

### What The Expanse does:
- **Faceted, angular hull plating** — flat panels meeting at shallow angles,
  like submarine pressure hull sections or stealth aircraft facets.
- **No organic curves.** The Rocinante, Donnager, and Amun-Ra are all built
  from flat or single-curvature panels joined at seams.
- **Panel lines and seams are the detail.** The visual interest comes from
  where panels meet, not from surface greebling.
- Hull panels are **large relative to the ship** — a 60m corvette might have
  panels 3-5m across. Not hundreds of tiny tiles.

### What to avoid:
- ❌ Smooth lathe-turned shapes (rockets, missiles, capsules)
- ❌ Organic compound curves
- ❌ Heavy surface greebling or random pipe/box detailing
- ❌ Aerodynamic lifting bodies, swept wings, waverider noses

### Translation to our primitives:
- Use `box()` as the primary building block — flat panel faces
- Use `tube()` with **low segment counts (4-6)** to get faceted cylinders,
  not smooth rounds
- Use `lathe()` with very few control points and low segments for
  **faceted cones**, not smooth ogives
- Chamfer corners by adding angled transition panels, not by increasing
  segment count

---

## 3. Color Palette: Military Industrial, Not Aerospace Showroom

### The Expanse palette:
| Role | Color | Hex approx | Notes |
|------|-------|-----------|-------|
| Primary hull | Medium warm grey | `#6B6B6B` | Unpainted composite/metal |
| Armor plate | Dark charcoal | `#3A3A3A` | Thick structural sections |
| Accent stripe | Faction color | — | MCRN: orange/red. UNN: blue. OPA: variable |
| Warning/status | Amber or red | `#CC6600` | Small, functional |
| Thermal radiator | Dark matte | `#2A2A2A` | Non-reflective panels |
| Interior light bleed | Warm white/amber | `#FFE4B5` | From windows, ports |
| Engine glow | Blue-white | `#88CCFF` | Epstein drive exhaust |

### What to avoid:
- ❌ Pristine white aerospace composite (SpaceX/NASA aesthetic)
- ❌ Bright cyan LED accent strips everywhere
- ❌ Pearlescent or iridescent surfaces
- ❌ "Solar cell" navy blue panels as large decorative surfaces
- ❌ High-saturation accent colors covering large areas

### Translation to our material system:
| prims.ts material | Expanse role | Usage |
|-------------------|-------------|-------|
| `lightArmor` | Primary hull panels | Largest area, reads as warm grey |
| `dark` | Recessed/structural/radiator | Second largest, shadow + depth |
| `metal` | Exposed structural frame, flanges | Unpainted metal at joints |
| `frame` | Internal lattice visible through gaps | Darker structural members |
| `copper` | Piping, conduit, small functional bits | Small accent, implies function |
| `glow` | Status lights, engine throat | **Tiny.** Pinpoint, not strips |
| `solarCell` | ~~decorative~~ → radiator panels | Reads as dark functional surface |
| `black` | Sensor windows, thermal coating | Small, purposeful |

**Key rule:** `glow` should be used like an LED indicator light — a few
square centimeters, not meters-long strips. Real ships have tiny status
lights, not Tron lines.

---

## 4. Structural Logic: Every Shape Justifies Its Existence

### The Expanse design philosophy:
Everything on the hull has a **function you can name:**
- PDC turret housings (bump-outs with flat armored faces)
- RCS thruster quads (small nozzle clusters at the corners)
- Radiator panels (flat, dark, often folded against hull)
- Antenna arrays (small masts or dish recesses)
- Airlock hatches (rectangular, flush with hull)
- Docking ports / flanges (standardized ring connectors)
- Torpedo tube openings (recessed circular or hex ports)
- Hull panel seam lines (where armor plates bolt together)

### What to avoid:
- ❌ Decorative fins, strakes, or "chines" with no function
- ❌ Random boxes glued to surfaces
- ❌ Symmetrical "X-wing" nacelle arrangements for aesthetics
- ❌ Swept-back anything (no aerodynamics in vacuum)

### For each component:
Before adding geometry, answer: **"What is this and why is it here?"**
Valid answers: structural frame, pressure vessel, thruster mount,
sensor housing, thermal management, docking interface.
Invalid: "looks cool," "adds visual interest," "breaks up the silhouette."

---

## 5. Component Design Rules

### Spine (structural backbone):
- **Square or hexagonal cross-section** truss, not round
- Visible internal lattice framing — diagonal braces, longerons
- Attachment hardpoints at regular stations (flange rings)
- Conduit/cable runs visible along the length
- **Not a smooth tube.** It's an exposed structural frame, like scaffolding
  or a bridge truss.

### Drive (engine):
- **Nozzle bell** — faceted cone, wide at exit, narrow throat
- Engine mount frame visible around the bell
- **No decorative fins or vectoring vanes** — Epstein drives don't need them
- RCS thruster quads near the engine for attitude control
- Radiator panels flanking the engine (thermal management)
- Glow only at the throat/exit plane — tiny, intense, not running up the sides

### Tank (propellant):
- **Cylindrical pressure vessel** with domed or flat endcaps
- Structural bands/rings at regular intervals (like real rocket tanks)
- Fill/drain valve cluster on one side (small, functional)
- Level sensor strip (thin, vertical, on one side)
- Insulation blanket texture implied by material (not geometry)
- **No decorative ribs or chines.** Tanks are tanks.

### Nose (command/sensor):
- **Blunt, faceted wedge or truncated pyramid** — not a pointed ogive
- Sensor windows: flat dark panels recessed into the hull face
- Antenna mast or small dish
- Navigation lights (tiny glow dots at extremities)
- Bridge windows: very small, recessed, with armored shutters implied
- **Not aerodynamic.** There's no atmosphere. A flat face is fine.

---

## 6. Proportions & Silhouette

- **Aspect ratio:** Ships are tall and narrow (3:1 to 8:1 length:width)
- **Width variation along length:** Engines are wider (thrust frame),
  midsection is narrow (spine/truss), nose is blunt but smaller.
  The profile should be **a slight taper**, not uniform.
- **Asymmetry:** Real ships aren't perfectly symmetric. One side has the
  airlock, another has antenna masts, another has radiator panels.
  Minor asymmetry reads as authentic.

---

## 7. Detail Density

- **Far fewer details, but each one is deliberate.**
- At game zoom (32m ship = ~100px), the ship should read as:
  1. A clear silhouette shape
  2. 3-4 distinct sections (nose, spine, tank, engine)
  3. A few high-contrast detail elements (thruster clusters, sensor window)
- **Panel line seams** do the heavy lifting for visual texture.
  These come from the `standard_maps` texture pass, not geometry.
- Geometric detail budget: **functional equipment only.**

---

## 8. Anti-Patterns (Things That Make It Look Like AI Slop)

1. **Tron lines:** Long glowing strips running the length of the hull
2. **Symmetrical fin arrays:** 4 identical fins at 90° intervals
3. **Smooth ogive nose:** Missile/rocket aesthetic
4. **"Aerospace composite" white:** SpaceX showroom look
5. **Random greeble boxes:** Boxes stuck on the hull with no function
6. **Uniform cross-section:** Same width from engine to nose
7. **Decorative sweep:** Anything that looks swept-back or aerodynamic
8. **Oversized accent color areas:** Large panels of a bright color
9. **Too many materials visible:** More than 3-4 distinct colors
10. **Perfectly clean/new:** No ships look factory-fresh. But also not
    "scrapy/weary" — just *used,* like a working truck, not a prop.

---

## References

- [Expanse Wiki: Donnager](https://expanse.fandom.com/wiki/Donnager) —
  "sleek and angular, covered in thick armor plating, shaped like a long
  broadhead arrow tip"
- [Expanse Wiki: Rocinante](https://expanse.fandom.com/wiki/Rocinante_(TV)) —
  Corvette-class, compact, functional
- [Joseph Shoer: Spaceships of the Expanse](http://josephshoer.com/blog/2015/06/spaceships-of-the-expanse/) —
  Engineering analysis of the design logic
- [Ferrovial: Fiction Engineering](https://www.ferrovial.com/blog/en/2022/08/fiction-engineering-the-expanse/) —
  "decks perpendicular to direction of motion"
- [Ryan Dening concept art (ArtStation)](https://www.artstation.com/artwork/zQwR6) —
  Original Rocinante design sheets
- [Spacedock: Original Designs for the Rocinante](https://www.youtube.com/watch?v=E6z4vF6F0WY) —
  Design evolution discussion with Chris Danelon
