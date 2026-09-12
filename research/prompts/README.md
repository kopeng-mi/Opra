# Prompt pack - one file, one chat

Each `.txt` is complete and self-contained: paste the whole file into a fresh chat.
No prompt refers to another asset, another chat, an attached image, or any part of the
object that is not in the picture. Every instruction is literal and depictable.

Save every result to `research/refs/<id>-<name>.png`.

Run order: **00** first (it is the colour and material anchor), then **01-03** (the
ships), then **10-12** (the parts that appear on every ship), then **20**, then the rest.

Across isolated chats the style block and the nine palette hexes are the only thing
holding the set together, so expect drift in surface detail. Silhouette, proportion and
component placement are what matter, and those are stated as numbers and positions.
Making the three ships look distinct from each other is a review job, not something any
single chat can do - it cannot see the others.

If you later decide to attach the 00 board to each chat, regenerate with
`python tools/gen_prompts.py --anchor` to add one line telling the model to obey it.
The board itself never gets that line.

| id | file | asset | views |
|---|---|---|---|
| 00 | `00-palette-board.txt` | palette board | single board |
| 01 | `01-kestrel-corvette.txt` | kestrel corvette | 4-view turnaround |
| 02 | `02-mule-tug.txt` | mule tug | 4-view turnaround |
| 03 | `03-needle-cutter.txt` | needle cutter | 4-view turnaround |
| 10 | `10-pdc-turret.txt` | pdc turret | 3-view + scale cube |
| 11 | `11-fusion-drive.txt` | fusion drive | 3-view + scale cube |
| 12 | `12-rcs-quad.txt` | rcs quad | 3-view + scale cube |
| 13 | `13-docking-collar.txt` | docking collar | 3-view + scale cube |
| 14 | `14-radiator-panel.txt` | radiator panel | 3-view, deployed + stowed |
| 15 | `15-landing-leg.txt` | landing leg | 3-view, deployed + stowed |
| 16 | `16-sensor-mast.txt` | sensor mast | 3-view + scale cube |
| 17 | `17-hull-modules.txt` | hull modules | 3-view + scale cube |
| 18 | `18-torpedo-tube.txt` | torpedo tube | 3-view + scale cube |
| 20 | `20-wayfarer-station.txt` | wayfarer station | 4-view turnaround |
| 21 | `21-lagrange-base.txt` | lagrange base | 4-view turnaround |
| 22 | `22-surface-base.txt` | surface base | 4-view turnaround |
| 30 | `30-relay-satellite.txt` | relay satellite | 3-view, deployed + stowed |
| 31 | `31-nav-buoy.txt` | nav buoy | 3-view + scale cube |
| 32 | `32-derelict-wreck.txt` | derelict wreck | 4-view turnaround |

## Textures (40-56)

A separate family: flat maps, not objects. None of the hull style, turnaround layout or
plan-view rule applies, and none of them takes a reference image - a texture copied from
another texture's look is a texture of the wrong surface.

Two failure modes to watch for when you review a result. First, the model draws a sphere
instead of an unrolled rectangle - reject it, the prompt bans it explicitly. Second, it
bakes in a sun: a bright side, a terminator, shadows in the craters. That is subtler and
worse, because the engine lights the surface again on top of it.

Equirectangular maps must come back exactly 2:1. If the model returns a square, ask only
for a 2:1 aspect ratio and change nothing else.

| id | file | asset | form |
|---|---|---|---|
| 40 | `40-rock-silicate.txt` | rock silicate | seamless square tile |
| 41 | `41-rock-carbonaceous.txt` | rock carbonaceous | seamless square tile |
| 42 | `42-rock-ore-vein.txt` | rock ore vein | seamless square tile |
| 43 | `43-regolith-dust.txt` | regolith dust | seamless square tile |
| 50 | `50-cinder-scorched.txt` | cinder scorched | equirectangular 2:1 map |
| 51 | `51-tessera-temperate.txt` | tessera temperate | equirectangular 2:1 map |
| 52 | `52-tessera-clouds.txt` | tessera clouds | equirectangular 2:1 map |
| 53 | `53-tessera-nightlights.txt` | tessera nightlights | equirectangular 2:1 map |
| 54 | `54-vesk-moon.txt` | vesk moon | equirectangular 2:1 map |
| 55 | `55-halberd-ice.txt` | halberd ice | equirectangular 2:1 map |
| 56 | `56-star-photosphere.txt` | star photosphere | seamless square tile |
