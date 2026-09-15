# PLAN-17 — Cold Freight: the playable vertical slice

Depends on PLAN-10 through PLAN-16. This is the plan that proves the others work.

One contract, end to end: take a package out of an asteroid belt, run to the jump line, jump to the
neighbouring star, come back, and put it on a station. It touches docking, belt flying, salvage,
node planning, time warp, the gravity gate, two star systems, crew, heat, propellant and the story
layer — which is the point. If this slice is fun, the game is fun.

---

## 1. The fiction

Real system, real names, invented worlds.

| star | real designation | role |
|---|---|---|
| **Toliman** | Alpha Centauri B, K1V, 0.909 solar masses | the industrial star. The belt, the yards, the work. |
| **Rigil** | Alpha Centauri A, G2V, 1.079 solar masses | the developed star. Old money, a gas giant in the habitable zone and its moons. |
| **Proxima** | Proxima Centauri, M5.5Ve, 8,700 AU out | act three. Not in this slice. Unreachable without the thing the story has not given you yet. |

Toliman and Rigil orbit each other: a = 23.5 AU, e = 0.52, period **79.9 years**. Separation swings
between 11 and 36 AU. That is a real number and it is the freight calendar — which is why a jump
link between them has a cost that changes with the decade.

Rigil's gas giant is the planet JWST imaged as a candidate in 2025. In this fiction it was real, and
its moons are where Rigil's people live.

**The ship** is a Toliman war-surplus escort frigate. The guns were cut off fifteen years ago and
the spine stretched with freight racks where the magazines used to sit. Everyone who sees the hull
knows what it was. The campaign is putting the teeth back in, one mount at a time — which the
shipyard screen already does.

### 1.1 The reskin, and its honest cost

`assets/systems/nereid.json` becomes `toliman.json`. Names are a string change; **the physics is
not**. Nereid is an M-dwarf at `mu = 2.07e19`; Toliman is `mu = 1.206e20`, 5.8x heavier. Every
orbit must be re-derived or the periods are wrong.

| body | Nereid a | Toliman a | note |
|---|---|---|---|
| Cinder | 7.18 Gm | 0.09 AU / 13.5 Gm | scorched inner world |
| Tessera | 17.2 Gm | 0.61 AU / 91 Gm | the habitable one, thin air |
| Vesk | 52 Mm | unchanged | Tessera's moon |
| **the Drift belt** | 24.6-28.9 Gm | **0.37-0.43 AU / 55-64 Gm** | where the slice starts |
| Wayfarer | 26.1 Gm | **0.40 AU / 59.8 Gm** | the station, in the belt |
| Halberd | 43.4 Gm | 0.78 AU / 117 Gm | ice giant |
| **the jump line** | — | **1.038 AU / 155.3 Gm** | computed, not authored |

This regenerates every golden capture. Budget one pass of `tools/shots.ps1` and treat the diff as
expected, not as a regression.

---

## 2. The two systems

### 2.1 Toliman

```json
{
  "name": "Toliman",
  "star": { "name": "Toliman", "mu": 1.206e20, "radius": 5.98e8, "color": "#ffcf8a" },
  "jump_gravity_max": 5.0e-3,
  "zone_seed": 4712,
  "default_anchor": "wayfarer",
  "jump_links": [
    { "to": "rigil", "distance_ly": 0.00017, "arrival_bearing": 2.47 }
  ]
}
```

Bodies as the table above. The belt and Wayfarer keep their existing contents and seed, so the
sector the player already knows is intact.

### 2.2 Rigil

New, and deliberately small — one gas giant, two moons, one station. Enough to be a place, not
enough to be a second game.

| body | a | note |
|---|---|---|
| **Kettle** | 1.15 AU | the gas giant. The JWST candidate, made real. |
| Marn | 0.0042 AU from Kettle | inner moon, tidally heated, the refinery |
| Sill | 0.011 AU from Kettle | outer moon, ice, quiet |
| **Ladder** | at Kettle-Marn L4 | the station. Uses the existing `LagrangeSeat`. |

```
mu_rigil  = 1.432e20      (1.079 solar masses)
jump line = sqrt(1.432e20 / 5.0e-3) = 1.693e11 m = 1.132 AU
```

Arrival is on that circle, on a circular prograde orbit (PLAN-13 J6). Kettle at 1.15 AU sits just
inside it, so the arrival point is a short hop from the only place worth going — which keeps the
second system a scene and not a second commute.

---

## 3. The mission

**Cold Freight.** Issued by the Toliman combine at Wayfarer. Deadline **14 days**. Payout 42,000.

### Beat 1 — Berth

At Wayfarer. Take the contract from the board, look at the crew, look at the ship.

*Exercises:* docking (already works), the contract board, the compartment register, crew roster.

*Story:* `cold_freight.brief`. The quartermaster explains what she is not telling you: the package
was logged as medical, the weight says otherwise, and the combine is paying triple the rate for a
fourteen-day turnaround.

### Beat 2 — The belt

The package is in a cold cache clamped to a ringroid, 40-90 km out in the Drift. It is not marked.
The survey sweep finds it; the scanner narrows it; you match velocity and take it.

*Exercises:* belt flying, the spatial grid, the survey sweep, `Contact` tracking, the collar and
minimap at close range, RCS and manoeuvre propellant, the cutter if the cache is frozen in.

*Design note:* the rock is real and it is moving. **Matching velocity with a tumbling ringroid is
the whole beat** — the existing `KillRelative` flight program is the cue and the player still flies
it, because the director never writes (`sim/program.h`).

*Story:* `cold_freight.found`. The cache is combine-stamped but the seal is Authority. Someone
opened it before you.

### Beat 3 — The run out

From the belt at 0.40 AU to the jump line at 1.038 AU. **95.5 Gm.** Two routes, one gauge:

| route | dv | time | transit propellant (142 t dry) |
|---|---|---|---|
| **Coast** — Hohmann, two nodes | **16.1 km/s** | **116.8 d** | **2.3 t** |
| Burn 0.05 g | 433 km/s | 10.2 d | 76.9 t |
| **Burn 0.10 g** | **612 km/s** | **7.2 d** | **119.9 t** |
| Burn 0.30 g | 1,060 km/s | 4.2 d | 267.9 t |
| Burn 1.00 g | 1,935 km/s | 2.3 d | 841.6 t |

**The deadline is 14 days, so the coast is not an option on this contract.** It is shown anyway, and
it is shown first, because the player has to see what they are giving up: 52 times the propellant to
arrive 110 days sooner. The 0.30 g and 1.00 g rows are drawn dim — a 142 t hull cannot carry 268 t
of propellant, and the table says so without a tutorial.

So the real choice is 0.05 g against 0.10 g: ten days and 77 tonnes, or seven days and 120 tonnes,
against a fourteen-day clock and whatever the return leg is going to cost. **That is the game.**

*Exercises:* the route comparison panel, `plan_transit`, transit resolution, time warp on the conic,
the node planner if the player coasts anyway, crew strain, component wear, heat.

*Story:* one storylet drawn from the transit table. The bent radiator, the engineer's g-tolerance,
the thing in the cache that is not medical.

### Beat 4 — The gate

Local gravity has to fall under 5.0e-3 m/s^2 and stay there for 120 seconds of sim time. Any thrust
resets the charge. So the last act of the run out is to **stop burning and drift**, which is exactly
when the player has least patience and most reason to want it over with.

```
JUMP
  local g   0.0051      threshold 0.0050
  line      0.2 Gm out
  charge    [==        ]   resets on thrust
```

*Exercises:* the gravity gate (the user's rule), the charge state machine, the jump readout.

### Beat 5 — Rigil

Arrive on Rigil's jump line at 1.132 AU. Kettle is close; Ladder station is at its L4. The buyer is
not there, or is, and has changed the terms.

*Exercises:* `SystemStore`, `attach_system`, zone rebuild, a second set of bodies, the Lagrange seat,
a second station dock.

*Story:* the branch. Three ways out, all of which close the contract differently and all of which
move `rep.combine` and `rep.authority` in opposite directions.

### Beat 6 — The run back

Rigil's line is 1.132 AU and Kettle is at 1.15 AU, so the return leg to the gate is short. The
expensive part was getting out of Toliman. **What the player has left in the transit tank decides
how fast they get home** — and if they burned 120 t going out, the answer may be that they coast and
eat the late penalty.

*Exercises:* the same machinery under scarcity, which is when it teaches.

### Beat 7 — Delivery

Dock at Wayfarer. `contract.cold_freight.stage = delivered`. Payout, reputation, and the storylet
that sets up whatever comes next.

*Exercises:* docking under a deadline, contract resolution, the payout, the story hook.

---

## 4. What the slice proves

| plan | proven by |
|---|---|
| PLAN-10 | the whole thing runs from `build/Release` with no overlapping blocks and a camera that behaves |
| PLAN-11 | the radiator you did not fix is why you can only hold 0.05 g |
| PLAN-12 | beat 3's table, and the return leg under scarcity |
| PLAN-13 | beats 4, 5 and 6 |
| PLAN-14 | the route comparison is the beat-3 decision, made visible |
| PLAN-15 | seven storylets, one contract, five crew, two clocks |
| PLAN-16 | two systems, one new station, five portraits, no fatal loads |

---

## 5. Acceptance

The slice is done when a player who has never seen the game can, without being told:

| # | criterion |
|---|---|
| A1 | accept the contract and understand what it asks |
| A2 | find and retrieve the package without a waypoint being placed for them |
| A3 | **explain why they chose 0.05 g over 0.10 g**, or the reverse |
| A4 | reach the jump line and work out on their own why the jump will not fire while they are burning |
| A5 | arrive at Rigil, dock, and resolve the branch |
| A6 | get home and know before they arrive whether they will be late |
| A7 | name one crew member and one thing about them |

A3 and A7 are the two that matter. A3 means the propellant model reads. A7 means the story landed.

### 5.1 Automated gates

| # | gate | how |
|---|---|---|
| G1 | a scripted run completes all seven beats headless | new `--slice` harness mode |
| G2 | the run is deterministic: two runs from one seed produce identical end qualities | `--slice` twice, diff |
| G3 | the beat-3 table reproduces section 3 to 3 significant figures | unit |
| G4 | the jump never fires inside the gate, over 10,000 sampled positions | unit |
| G5 | Rigil arrival is always outside Rigil's gate | unit |
| G6 | the contract expires correctly when the player coasts | scripted |
| G7 | every golden capture regenerates cleanly after the reskin | `tools/shots.ps1` |
| G8 | `--selftest` passes with two systems loaded | existing harness |

---

## 6. Build order

Do not build the slice first. Build it last, out of parts that already pass their own gates.

```
1.  PLAN-10   foundation, cleanup, defects        <- the game becomes assessable
2.  PLAN-16   assets: manifest, no fatal loads    <- nothing else is safe without this
3.  PLAN-11   components: condition, power, heat
4.  PLAN-12   flight: propellant tiers, burn table
5.  PLAN-13   jump: gravity gate, two systems
6.  PLAN-14   ui: registers, data-driven HUD, planner
7.  PLAN-15   story: qualities, storylets, crew, contracts
8.  PLAN-17   this: the reskin, Rigil, seven beats
```

PLAN-16 moves to second because every later plan adds assets, and adding assets to a loader that
calls `fatal()` on a typo is how the tree ended up un-runnable in the first place.
