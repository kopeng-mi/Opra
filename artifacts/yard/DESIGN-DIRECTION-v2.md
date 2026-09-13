# Opra Shipkit — Design Direction v2
## Section-Module Architecture

> **Supersedes:** TASTE-PROFILE.md (aesthetic rules still apply, structural approach changes)

---

## Core Concept: No Visible Spine

Ships are **chains of section modules** connected flange-to-flange. There is no
visible backbone/truss/keel. Each module is a self-contained chunky block that
IS part of the hull. The "spine" from PLAN-07 becomes an **invisible flange
chain** — the connection standard, not a rendered object.

From the camera at 32° pitch, a ship reads as a **wide flat submarine body**
— a continuous silhouette made of distinct sections, not a stick with things
hanging off it.

---

## Module Types (replacing "spine + parts")

### Fore modules (nose end)
- **nose_command** — tapered wedge, sensor face, bridge windows. ~2.6m wide
- **nose_sensor** — sensor-heavy variant, antenna mast, flat face
- **nose_armored** — blunt, heavy, thick forward plate

### Midsection modules (the body)
- **section_tank** — cylindrical/boxy propellant section, ~2.7m wide
- **section_hab** — crew quarters, slightly wider with window ports
- **section_cargo** — open-frame cargo bay, ~3m wide
- **section_weapons** — weapon hardpoints + PDC turrets integrated, ~3m wide

### Aft modules (engine end)
- **drive_fusion_main** — engine bell + thrust frame, ~3m wide tapering
- **drive_ion** — multiple small thrusters on a plate
- **drive_heavy** — big single bell, widest section

### Side-mount pods (radial, optional)
- **pod_tank** — conformal fuel pod, adds width at a station
- **pod_weapon** — PDC or torpedo cell, small lateral bump
- **pod_radiator** — dark flat panel extending laterally (the silhouette-maker)

---

## How It Maps to PLAN-07

| PLAN-07 concept | New implementation |
|---|---|
| Spine (truss/keel/mono) | **Gone as visible object.** The "family" becomes a hull-width parameter that determines how wide the section modules are |
| Stations at 4m pitch | Each section module is 4m long (one pitch). Modules stack axially |
| Radial facings (port/starboard/dorsal/ventral) | Side-mount pods bolt to section modules, not to spine stations |
| Fore/aft facings | Nose and drive modules cap the chain |
| Flange at origin, body +Y | **Unchanged.** Every module's flange is at -Y, body extends +Y |

The **modular chain** satisfies all PLAN-07 requirements:
- Parts are still individual GLBs with flanges
- `mount_transform` still works (axial stacking)
- Runtime composition still draws one model per part
- Wreckage still detaches individual modules
- Section CV comes from width variation between module types

---

## Plan-View Silhouette Targets

Corvette class (5 modules, 20m long):
```
     ___
    / 2.4\     nose_command (tapered)
   |  2.7 |   section_tank
   |  3.0 |   section_weapons (widest, PDC bumps)
   |  2.7 |   section_tank
    \ 2.8/     drive_fusion_main
     |||
```

Width range: 2.4m → 3.0m → 2.8m
CV = std/mean ≈ 0.07... too low.

Need side-mount pods to create real width variation:
```
     ___
    / 2.4\          nose_command
   |  2.7 |        section_tank
   |  3.0 |--pod   section_weapons + side pods → 5.0m
   |  2.7 |        section_tank  
    \ 2.8/          drive_fusion_main
     |||
```
Widths: 2.4, 2.7, 5.0, 2.7, 2.8 → CV ≈ 0.30 ✓

---

## Proportions

- Section module width: **2.4–3.0m** (body)
- Side pods extend to: **4.5–6.0m** at widest
- Module length: **4.0m** (one pitch, per plan)
- Overall ship length: **12–32m** (3–8 modules)
- Aspect ratio from above: roughly 4:1 to 8:1

---

## Material Palette (function = color)

| Module type | Primary material | Why |
|---|---|---|
| Hull sections | `lightArmor` (warm grey) | Pressure hull, structural |
| Tanks | `metal` (steel grey) | Pressure vessel |
| Weapons | `dark` (charcoal) | Recessed, armored |
| Radiators | `dark` or `black` | Thermal emitter |
| Drive bell | `metal` → `dark` gradient | Exhaust-side thermal |
| Piping/conduit | `copper` | Functional accent |
| Status lights | `glow` | **Pinpoint only** |
| Sensor faces | `black` or `glass` | Optical aperture |

---

## For the Proof Build

The proof design from PLAN-07 §4.8 becomes:

1. `nose_command` — tapered wedge, 4m long, 2.4m wide
2. `section_tank` — boxy propellant module, 4m long, 2.7m wide  
3. `section_weapons` — body with PDC bumps + radiator pods, 4m long, 3.0m body + 5.0m with pods
4. `drive_fusion_main` — engine section, 4m long, 2.8m wide

Total: 16m, 4 modules. All gates should pass.
