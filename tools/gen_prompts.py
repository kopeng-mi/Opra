#!/usr/bin/env python3
"""Writes one self-contained, paste-ready image prompt per file.

Each file is run in its own fresh chat, so every block is expanded inline and
nothing may refer to anything outside that one prompt: no other asset, no other
chat, no attached image, no part of the object that is not in the picture.

Every instruction must be literal and depictable. Abstract direction ("clearly a
warship", "more than drama") is either ignored or rendered as mood; concrete
direction ("roughly four times as long as it is wide") is obeyed.

    python tools/gen_prompts.py            # default
    python tools/gen_prompts.py --anchor   # add an obey-the-attached-image line
"""
import pathlib
import sys

OUT = pathlib.Path("research/prompts")

# Non-depictable framing. Names the failure modes explicitly so they cannot be
# chosen: a printed sheet, a scene, a hero shot.
LEAD = """Produce exactly one image. It is a flat, evenly lit, orthographic technical render of a single object, made so that its shape and proportions can be read precisely. It is not a poster, not a scene, not a cinematic shot, and not a photograph of a printed sheet or a drawing."""

ANCHOR = """An image is attached. Copy its material treatment, lighting, colours, weathering and level of detail exactly. Change only the object itself."""

STYLE = """Style. Industrial hard science-fiction in the visual logic of The Expanse: no aerodynamic streamlining, no smooth curved shell. The hull is visible structure - exposed frames, bolted plate seams, weld lines, machined fittings, cable runs and conduit. Weathering: soot aft of every thruster, small impact pitting across forward surfaces, paint worn through to bare metal around hatches and handholds, and a few replacement panels in a slightly different shade. Matte painted metal and ceramic only. No chrome, no mirror finish, no neon, no glowing seams, no light strips.

Palette, use these exact colours: hull plate #BAC4C3, light plate #E2E3D8, shadowed structure #202E38, bare metal #667681, copper trim #C88755, black recess #0C141A, tinted glass #376C7C, teal marking band #326B70, ochre marking band #BC783C.

Render. 3D hard-surface render. Soft even lighting from the upper front left with a weak fill from the right, so no surface is in darkness. Ambient occlusion in the crevices. Sharp edges, everything in focus, no depth of field."""

VIEWS_VEHICLE = """Layout. One image, divided into a 2x2 grid of four separate views of the same object. Top left: seen from directly above. Top right: seen from directly to its left side. Bottom left: seen from directly in front, nose toward the viewer. Bottom right: seen from behind and 30 degrees above. All four are orthographic with no perspective, at the same scale, with the same lighting and the same colours, each centred in its own cell. The background is pure white #FFFFFF in every cell."""

VIEWS_PART = """Layout. One image, divided into three separate views of the same object in a single row. Left: seen from directly in front. Centre: seen from directly to its side. Right: seen from 30 degrees above and to one side. All three are orthographic with no perspective, at the same scale, with the same lighting and the same colours. Stand a plain grey untextured cube exactly 1 metre on each side on the ground next to the object in every view, for scale. The background is pure white #FFFFFF."""

VIEWS_PART_TWOSTATE = """Layout. One image, divided into two rows. The top row shows the object in its deployed, extended state in three separate views: seen from directly in front, seen from directly to its side, and seen from 30 degrees above and to one side. The bottom row shows the same object in its stowed, folded state, in those same three views. All six views are orthographic with no perspective, at the same scale, with the same lighting and the same colours. Stand a plain grey untextured cube exactly 1 metre on each side next to the object in the side view of each row, for scale. The background is pure white #FFFFFF."""

# Self-contained: says nothing about assets this chat cannot see.
PLAN = """Priority. Of the four views, the one seen from directly above matters most - it is the only view of this object that will ever be used. Give it the most care. Its outline must be bold and distinctive, and the nose end must be a clearly different shape from the stern end, so that the direction the object points is obvious from the outline alone, with no colour or shading."""

NEG = """Do not include: perspective distortion, cast shadows falling on the background, text, letters, numbers, labels, dimension lines, arrows, callouts, watermarks, logos, insignia, human figures, planets, stars, starfields, nebulae, lens flare, glow, motion blur, ground planes, or any background object other than the stated scale cube."""

# id, filename stem, kind (vehicle | part | part2 | board), subject body
ASSETS = [
("00", "palette-board", "board",
 "Subject. Nine flat rectangular metal plates arranged in a 3x3 grid on a pure white surface, seen "
 "from directly above, with a clear white gap between every plate. Each plate is about 30cm square "
 "and shows one material in close detail: painted hull plate, light ceramic plate, shadowed "
 "structural alloy, bare machined metal, copper heat trim, black recessed grating, tinted armoured "
 "glass, a teal painted marking band, an ochre painted marking band. Every plate carries real "
 "surface history: bolt heads, a weld seam, a scuff, wear along one edge, a heat stain.\n\n"
 "Layout. One image, seen straight down, even frontal lighting, no perspective, pure white "
 "background #FFFFFF. Each plate must be large and clearly separate from its neighbours."),

("01", "kestrel-corvette", "vehicle",
 "Subject. A 46-metre armed patrol corvette, roughly four times as long as it is wide. The hull "
 "points nose-forward, with a blunt armoured prow at the front, a narrow structural spine along "
 "the middle, and two fusion drive bells at the stern. Two square torpedo tube openings sit in the "
 "face of the prow. Along each flank, evenly spaced transverse ring frames and small rectangular "
 "ports mark the internal decks, which lie across the hull rather than along it. Two point-defence "
 "cannon turrets sit in shallow recessed blisters on the upper surface and one on the underside. "
 "Two flat radiator panels fold along the spine. A circular docking collar is set into the left "
 "flank at the midpoint. Four small thruster blocks sit at the four corners of the hull."),

("02", "mule-tug", "vehicle",
 "Subject. A 58-metre heavy salvage tug, roughly three times as long as it is wide. Instead of a "
 "closed hull it has an open chassis of exposed longitudinal rails, so the structure is visible "
 "through it. Three rectangular cargo pods are clamped along each flank. A pressurised cab with a "
 "wide segmented canopy sits well forward. Four drive bells sit at the stern in a protected square "
 "cluster. A heavy towing yoke and two folding grapple arms extend from the bow. Two radiator "
 "panels, oversized relative to the ship. One point-defence cannon turret on the upper surface. "
 "Heavily worn: patched plate, and several replacement panels in visibly different shades."),

("03", "needle-cutter", "vehicle",
 "Subject. A 31-metre reconnaissance cutter, roughly eight times as long as it is wide. A long "
 "slender fuselage with one fusion drive bell at the stern that is the single largest component on "
 "the ship and fills the rear third of its length. A thin sensor needle projects forward from the "
 "centre of the nose. Two flat radiator blades sweep back from the flanks at an angle. The armour "
 "is minimal and the structural framing is left exposed along the sides. One point-defence cannon "
 "turret on the underside. A small two-person cabin with one window, set well forward."),

("10", "pdc-turret", "part",
 "Subject. A point-defence cannon turret, 1.8 metres across. A rotating armoured dome set into a "
 "shallow circular hull recess, with two rapid-fire barrels side by side on a cradle that can tilt. "
 "An ammunition feed housing sits below the dome and a rectangular ejection port on one side. The "
 "barrels are turned 30 degrees to one side and raised 20 degrees. Heavy soot at the muzzles, bare "
 "metal worn bright in a ring where the dome rotates."),

("11", "fusion-drive", "part",
 "Subject. A fusion rocket engine, 5 metres long. A flared expansion bell with fine cooling "
 "channels running down its length, a gimbal ring at the narrow throat, a turbopump and plumbing "
 "wrapped around the upper housing, and a flat circular mounting flange at the top. The metal is "
 "heat-discoloured, straw yellow near the throat darkening to deep blue at the bell rim. The engine "
 "is switched off: no flame, no exhaust, no glow."),

("12", "rcs-quad", "part",
 "Subject. A reaction control thruster block, 1.2 metres across. Four small nozzles mounted on one "
 "shared armoured box, two pointing sideways and two pointing fore and aft. Propellant lines enter "
 "from the rear. A small rectangular service hatch on the front face. Soot staining spreads outward "
 "from each nozzle mouth. Switched off: no flame, no glow."),

("13", "docking-collar", "part",
 "Subject. A spacecraft docking collar, 2.4 metres across the ring. An armoured ring facing "
 "outward, with a flat rubber sealing face, four latching lugs spaced evenly around it, four guide "
 "petals angled outward behind them, a connector block on one side, and a small round inspection "
 "window. The sealing face and the lugs are worn smooth and shiny from repeated use."),

("14", "radiator-panel", "part2",
 "Subject. A deployable spacecraft radiator panel, 6 metres long when extended. A thin flat "
 "rectangular panel with fine parallel coolant channels across its face, a hinged root with a "
 "visible actuator, and a manifold box at the base. The panel face is faintly discoloured by heat, "
 "strongest near the manifold. Deployed means the panel is swung fully out and flat; stowed means "
 "it is folded back against its root."),

("15", "landing-leg", "part2",
 "Subject. A spacecraft landing leg, 3.5 metres long when extended. Three segments with a "
 "telescoping shock strut, a hinged mount at the top, and a wide flat articulated footpad at the "
 "bottom. Dust and scoring on the underside of the footpad. Deployed means the leg is fully "
 "extended and angled outward; stowed means the segments are collapsed and folded flat."),

("16", "sensor-mast", "part",
 "Subject. A spacecraft sensor mast, 4 metres tall. A parabolic dish on a two-axis gimbal at the "
 "top, a small feed horn held at the centre of the dish on three thin struts, a flat rectangular "
 "phased-array panel mounted below the dish, cable runs following an open lattice mast down to a "
 "flat mounting flange at the base. Only lightly weathered."),

("17", "hull-modules", "part",
 "Subject. Four modular spacecraft hull sections standing side by side in a row, all the same "
 "length of 4 metres and the same diameter, each ending in an identical circular bolted flange at "
 "both ends so that any of them could be bolted to any other. From left to right: a cylindrical "
 "propellant tank wrapped in a quilted insulation blanket; a pressurised habitat section with two "
 "small round ports and one rectangular hatch; an open cargo frame with a rectangular container "
 "clamped inside it; a plain open structural truss with nothing between its ends."),

("18", "torpedo-tube", "part",
 "Subject. A spacecraft torpedo launch tube, 4 metres long. A square armoured muzzle block at the "
 "front with a hinged blast door standing open, the cylindrical tube body behind it, a loading "
 "mechanism at the rear, and a rack holding two spare cylindrical rounds alongside the tube. Heavy "
 "soot around the muzzle opening."),

("20", "wayfarer-station", "vehicle",
 "Subject. An orbital station, 180 metres across. A wheel-shaped habitat torus is held on four "
 "straight spokes around a central cylindrical spine that runs through its axis. A docking arm "
 "extends from each end of the spine, each carrying two circular docking collars. Flat solar wings "
 "and a group of flat radiator panels extend from one side of the spine. Four cylindrical "
 "propellant tanks are clustered around the base of the spine. Rows of small approach lights run "
 "along the docking arms. It was built in stages: one section of the torus is visibly newer and "
 "cleaner than the rest."),

("21", "lagrange-base", "vehicle",
 "Subject. A large deep-space station, 400 metres long. A long open lattice truss forms the "
 "backbone. A wheel-shaped rotating habitat ring encircles the truss one third of the way along "
 "its length. At one end sits an industrial processing block with ore hoppers and chutes. Four "
 "docking arms project from the truss at intervals. Two enormous flat radiator wings and a field of "
 "flat solar panels extend from the sides. The far end is unfinished: bare exposed framing with no "
 "plating, and a construction gantry still clamped to it."),

("22", "surface-base", "vehicle",
 "Subject. A small base standing on bare grey regolith, 60 metres across. Two flat circular landing "
 "pads with hold-down clamps around their rims and small lights around their edges. A low habitat "
 "made of linked horizontal cylindrical modules, half buried under a heaped bank of regolith. A "
 "propellant plant with two vertical cylindrical tanks. One tall communications mast. A short "
 "covered walkway running from the nearest pad to the habitat. Grey dust staining coats the lowest "
 "metre of every surface."),

("30", "relay-satellite", "part2",
 "Subject. A small relay satellite, 2 metres across the body. A compact six-sided body, two "
 "folding solar wings, a dish antenna on a gimbal, a thin whip antenna, and a cluster of four tiny "
 "station-keeping thrusters. Deployed means both solar wings are swung fully out and the dish is "
 "raised; stowed means the wings are folded flat against the body and the dish is folded down."),

("31", "nav-buoy", "part",
 "Subject. A navigation buoy, 1.5 metres tall. A weathered cylindrical body standing on a small "
 "three-legged frame, a strobe light housing on top, a corner-cube radar reflector on one side, a "
 "ring of solar cells around the middle of the body, and a curved grab handle for retrieval. A "
 "painted ochre band around the body, heavily faded and chipped."),

("32", "derelict-wreck", "vehicle",
 "Subject. The wreck of a 40-metre freighter, dead and powerless. The hull is torn open across its "
 "middle, exposing internal deck frames and severed cables. One of its two drive bells is missing "
 "entirely and the mount it was attached to is sheared off. Hull plating is peeled back around a "
 "blast scar on one flank. A radiator panel hangs loose from a single hinge. Everything is scorched "
 "and cold. Enough of the original hull survives that its original shape is still readable as a "
 "freighter."),
]


# --------------------------------------------------------------------------- textures
#
# A different family entirely. These are flat maps, not objects, so none of the
# hull style, turnaround layout or plan-view rule applies. The two failure modes
# that matter: the model draws a sphere instead of an unrolled map, and it bakes
# a sun into the image. Both are named explicitly and banned.

LEAD_TEX = """Produce exactly one image: a flat rectangular texture map, to be wrapped onto a 3D surface in a game engine. It is a flat picture of a surface and nothing else. It is not an object, not a sphere, not a scene, not a photograph of anything with edges or depth."""

LIGHT_TEX = """Lighting. Completely flat and even, as though lit from every direction at once. There must be no sun, no directional light, no cast shadows, no terminator, no day and night division, no specular highlights, and no darkening toward any edge. This image records surface colour only. Lighting is added later by the engine, so any lighting baked in here would be wrong twice over."""

TILE = """Layout. One square image of a flat surface seen from directly overhead, filling the frame edge to edge. It must tile seamlessly: the left edge must continue into the right edge and the top edge into the bottom edge with no visible seam. Keep the detail evenly spread, with no single feature large or distinctive enough to be noticed repeating when the image is tiled across a large area."""

EQUIRECT = """Layout. One image, exactly twice as wide as it is tall, in equirectangular projection: the surface of a whole sphere unrolled flat into a rectangle. The left and right edges must match exactly so they join without a seam when wrapped around a sphere. Features stretch and smear horizontally as they approach the top and bottom edges, which are the north and south poles. Fill the entire rectangle edge to edge."""

NEG_TEX = """Do not include: a sphere, a globe, a planet seen from space, a curved horizon, any three-dimensional object, black space, stars, a starfield, a nebula, atmosphere, glow, a border, a frame, a vignette, a drop shadow, text, letters, numbers, labels, watermarks, or a colour chart."""

# id, stem, kind (tile | equirect), body
TEXTURES = [
("40", "rock-silicate", "tile",
 "Subject. The bare surface of a grey silicate asteroid, seen very close, covering about 4 metres "
 "across. Fractured angular rock in mid grey, crossed by fine hairline cracks, scattered small "
 "impact craters of varying sizes, patches of finer grey dust settled in the low spots, and a few "
 "lighter freshly-broken faces where rock has spalled away. Dry, dusty, and completely airless."),

("41", "rock-carbonaceous", "tile",
 "Subject. The bare surface of a very dark carbonaceous asteroid, seen very close, covering about "
 "4 metres across. Near-black sooty rock, low contrast, slightly crumbly, with shallow rounded "
 "pits rather than sharp fractures, a dusting of paler grey regolith caught in the hollows, and "
 "occasional dull brown mineral staining. Matte and light-absorbing throughout."),

("42", "rock-ore-vein", "tile",
 "Subject. The bare surface of a metal-rich asteroid, seen very close, covering about 4 metres "
 "across. Grey-brown rock split by a branching network of exposed metallic veins in dull nickel "
 "grey and warm copper, the veins slightly raised and smoother than the rock around them. "
 "Scattered small craters, and a scatter of fine bright metallic flecks through the matrix."),

("43", "regolith-dust", "tile",
 "Subject. Fine loose grey regolith dust on an airless surface, seen from directly above, covering "
 "about 3 metres across. Soft powder with faint ripples, a scatter of small angular rock fragments "
 "of varying sizes partly buried in it, a few tiny fresh craters with slightly brighter rims, and "
 "very subtle variation between lighter and darker grey. No footprints, no tracks."),

("50", "cinder-scorched", "equirect",
 "Subject. The whole surface of a small scorched rocky planet that orbits very close to its star. "
 "Dark basalt plains in charcoal and deep brown, vast overlapping impact basins, long radial crack "
 "systems, and broad fields of solidified lava flow in near-black. Scattered bright ejecta rays "
 "streaking out from the youngest craters in pale grey. Sun-baked, cracked and utterly dry. No "
 "water, no ice, no vegetation, no clouds."),

("51", "tessera-temperate", "equirect",
 "Subject. The whole surface of a temperate inhabited world, drawn as land and sea only. Several "
 "irregular continents in ochre, olive green and dry tan, separated by deep blue-green oceans with "
 "paler continental shelves fringing every coast. Mountain chains as ridged brown spines, river "
 "systems branching toward the coasts, inland lakes and salt flats, and white ice caps covering "
 "both polar edges. Roughly one third land, two thirds ocean. No clouds at all."),

("52", "tessera-clouds", "equirect",
 "Subject. A cloud layer alone, with no ground beneath it. Pure white clouds on a solid pure black "
 "background, drawn as a greyscale mask where white is dense cloud and black is clear sky. Banded "
 "by latitude: broad swirling mid-latitude storm systems, a ragged clumpy equatorial belt, thin "
 "streaky cloud near the poles, and two large spiral cyclones. Roughly half the area covered. "
 "White and black only, no colour, no grey ground, no land, no ocean."),

("53", "tessera-nightlights", "equirect",
 "Subject. The lights of inhabited settlements at night, with no ground beneath them. Small warm "
 "yellow-white points and clusters of light on a solid pure black background. Dense clusters where "
 "coastlines and river valleys would be, thin threads of light linking them, and large areas of "
 "complete blackness across oceans, deserts and poles. Sparse overall, as a lightly settled world "
 "rather than a crowded one. Only light on black, no landmasses, no colour other than the lights."),

("54", "vesk-moon", "equirect",
 "Subject. The whole surface of a small airless grey moon. Heavily cratered highlands in pale grey "
 "with craters overlapping craters at every size from vast to tiny, and several large smooth darker "
 "grey basins flooded with ancient lava. Bright ray systems streak outward from a few young sharp "
 "craters. Faint rilles and scarps cross the plains. No water, no ice, no clouds, no atmosphere."),

("55", "halberd-ice", "equirect",
 "Subject. The whole surface of a cold ice-covered world. A fractured crust of pale blue-white ice "
 "crossed by a dense network of long dark linear cracks and ridges that run for great distances and "
 "intersect one another. Broad regions of chaotic broken ice blocks, patches of tan and rust "
 "mineral staining along the larger cracks, and a few shallow impact scars softened by flow. "
 "Smooth, cold and bright. No open water, no land, no clouds."),

("56", "star-photosphere", "tile",
 "Subject. The surface of a cool red dwarf star, seen very close. A dense mosaic of convection "
 "granules, each a rounded cell of bright orange-red separated from its neighbours by narrow darker "
 "lanes, in a range of sizes. Several large irregular starspots in deep dull red, and a few "
 "brighter hotter patches. Turbulent and organic. Deep orange-red throughout, no yellow, no white, "
 "no blue."),
]

KINDNAME = {"vehicle": "4-view turnaround", "part": "3-view + scale cube",
            "part2": "3-view, deployed + stowed", "board": "single board",
            "tile": "seamless square tile", "equirect": "equirectangular 2:1 map"}


def build(kind, body, anchor):
    parts = [LEAD, "", body, "", STYLE, ""]
    if kind == "vehicle":
        parts += [VIEWS_VEHICLE, "", PLAN, ""]
    elif kind == "part":
        parts += [VIEWS_PART, ""]
    elif kind == "part2":
        parts += [VIEWS_PART_TWOSTATE, ""]
    parts += [NEG]
    # Never on the palette board: it is generated first, so no reference can exist yet.
    if anchor and kind != "board":
        parts += ["", ANCHOR]
    return "\n".join(parts).rstrip() + "\n"


def build_texture(kind, body):
    layout = TILE if kind == "tile" else EQUIRECT
    # No ANCHOR ever: a texture copied from another texture's look is a texture of
    # the wrong surface. Each one stands alone by design.
    return "\n".join([LEAD_TEX, "", body, "", layout, "", LIGHT_TEX, "", NEG_TEX]) + "\n"


def main():
    anchor = "--anchor" in sys.argv
    OUT.mkdir(parents=True, exist_ok=True)
    index = [
        "# Prompt pack - one file, one chat",
        "",
        "Each `.txt` is complete and self-contained: paste the whole file into a fresh chat.",
        "No prompt refers to another asset, another chat, an attached image, or any part of the",
        "object that is not in the picture. Every instruction is literal and depictable.",
        "",
        "Save every result to `research/refs/<id>-<name>.png`.",
        "",
        "Run order: **00** first (it is the colour and material anchor), then **01-03** (the",
        "ships), then **10-12** (the parts that appear on every ship), then **20**, then the rest.",
        "",
        "Across isolated chats the style block and the nine palette hexes are the only thing",
        "holding the set together, so expect drift in surface detail. Silhouette, proportion and",
        "component placement are what matter, and those are stated as numbers and positions.",
        "Making the three ships look distinct from each other is a review job, not something any",
        "single chat can do - it cannot see the others.",
        "",
        "If you later decide to attach the 00 board to each chat, regenerate with",
        "`python tools/gen_prompts.py --anchor` to add one line telling the model to obey it.",
        "The board itself never gets that line.",
        "",
        "| id | file | asset | views |",
        "|---|---|---|---|",
    ]
    for pid, stem, kind, body in ASSETS:
        path = OUT / "{}-{}.txt".format(pid, stem)
        path.write_text(build(kind, body, anchor), encoding="utf-8")
        index.append("| {} | `{}` | {} | {} |".format(pid, path.name, stem.replace("-", " "),
                                                      KINDNAME[kind]))
    index += [
        "",
        "## Textures (40-56)",
        "",
        "A separate family: flat maps, not objects. None of the hull style, turnaround layout or",
        "plan-view rule applies, and none of them takes a reference image - a texture copied from",
        "another texture's look is a texture of the wrong surface.",
        "",
        "Two failure modes to watch for when you review a result. First, the model draws a sphere",
        "instead of an unrolled rectangle - reject it, the prompt bans it explicitly. Second, it",
        "bakes in a sun: a bright side, a terminator, shadows in the craters. That is subtler and",
        "worse, because the engine lights the surface again on top of it.",
        "",
        "Equirectangular maps must come back exactly 2:1. If the model returns a square, ask only",
        "for a 2:1 aspect ratio and change nothing else.",
        "",
        "| id | file | asset | form |",
        "|---|---|---|---|",
    ]
    for pid, stem, kind, body in TEXTURES:
        path = OUT / "{}-{}.txt".format(pid, stem)
        path.write_text(build_texture(kind, body), encoding="utf-8")
        index.append("| {} | `{}` | {} | {} |".format(pid, path.name, stem.replace("-", " "),
                                                      KINDNAME[kind]))
    (OUT / "README.md").write_text("\n".join(index) + "\n", encoding="utf-8")
    print("wrote {} object + {} texture prompts + README to {}".format(
        len(ASSETS), len(TEXTURES), OUT))


if __name__ == "__main__":
    main()
