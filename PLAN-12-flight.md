# PLAN-12 — Flight: two propellant tiers, the burn table, and thrust gravity

Depends on PLAN-10 and PLAN-11. This plan sets the numbers the whole game is played against.

The thesis in one line: **acceleration is gravity, gravity costs propellant, and propellant is the
only thing you really own.**

---

## 0. Where the code is

| fact | where |
|---|---|
| `step_ship` integrates thrust, fuel burn, heat, RCS | `src/sim/physics.cpp` |
| `ShipSpec::thrust` is newtons; the sim is kg and N | `src/sim/physics.cpp:13` |
| `mdot = 14000.0` hardcoded in the HUD delta-v readout | `src/game/scene.cpp:1010` — **wrong, deleted here** |
| Hohmann, Lambert, phase window, synodic period all exist | `src/orbit/transfer.h` |
| `orbit::Node{t, prograde, radial}` is the planner primitive | `src/orbit/transfer.h` |
| `World::nodes`, `run_nodes()`, `planned_conic()`, `warp_to_next_node()` | `src/sim/world.h` |
| `World::warp_step` rides the conic exactly, no integration | `src/sim/world.h` |
| Bottom arc already prints acceleration in g | observed at `1.67 g` in a capture |

---

## 1. Decisions

| id | decision | consequence |
|---|---|---|
| **P1** | **Two propellant tiers.** Manoeuvring propellant is integrated per frame at m/s scale. Transit propellant is spent by a planned burn at km/s scale and never integrated. | §2. Resolves the contradiction between docking precision and interstellar distance. |
| **P2** | **Exhaust velocity `Ve = 1.0e6 m/s`** (Isp ~102,000 s) for the fusion torch. Manoeuvring thrusters run at `Ve = 3.2e3 m/s` (Isp 326 s, a real bipropellant figure). | §2.1. Two drives, two orders of magnitude, both honest. |
| **P3** | **`accel_g` is the headline number**, everywhere. TWR appears only against a real surface. | §3. PLAN-10 D2 already deletes the bad readout. |
| **P4** | **A transit is planned, paid and resolved — not flown.** The player flies the last few km. | §4. Uses `nodes` + `warp_step`, both of which exist. |
| **P5** | **The coast/burn fork is the core decision of every contract.** Both routes stay viable; the deadline picks. | §5. This is the game. |
| **P6** | **Crew have a g-tolerance.** Sustained acceleration above it accrues strain. | §6. The one join between thrust gravity and the crew. |
| **P7** | **No inertial dampers, no juice.** High g hurts, and the only mitigation is burning softer or carrying a better-conditioned crew. | §6 |

---

## 2. Two propellant tiers (P1)

The contradiction this resolves: a docking nudge is 0.1 m/s; crossing 1 AU under thrust is
2.4e6 m/s. One gauge cannot show both and one tank cannot hold both.

| tier | scale | drive | Ve | spent by | integrated? |
|---|---|---|---|---|---|
| **Manoeuvring** | m/s .. km/s | RCS + cold thrusters | 3.2e3 m/s | docking, station-keeping, rock work, combat | yes, per frame |
| **Transit** | 10s .. 1000s of km/s | fusion torch | 1.0e6 m/s | **every burn that crosses the system** — Hohmann nodes *and* brachistochrone transits | no — resolved once |

The dividing line is not distance, it is **whether the burn is flown or planned**. Anything the pilot
holds the stick for comes out of the manoeuvring tank. Anything placed on the chart and resolved —
a node, a transit — comes out of the transit tank.

`ShipState` gains `prop_manoeuvre` and `prop_transit` in kg. `ShipSpec` gains the two capacities.
Tank parts declare which they hold (`"tier": "manoeuvre" | "transit"` in the sidecar).

### 2.1 The rocket equation, used twice

```
dv = Ve * ln(m_wet / m_dry)                 =>    m_prop = m_dry * (exp(dv/Ve) - 1)
```

Manoeuvring budget for the stock Kestrel (82 t dry, 16 t manoeuvre prop):

```
dv = 3200 * ln(98/82) = 3200 * 0.1784 = 571 m/s
```

That is the right order for station-keeping, docking and rock work, and it makes a wasted burn
actually hurt — which the old 20.17 km/s readout did not.

Transit budget is authored per design, in the hundreds of km/s, and §5 spends it.

### 2.2 Instantaneous mass

Both tiers feed one mass:

```
mass_total = spec.dry + prop_manoeuvre + prop_transit + cargo_mass
```

`accel_g` reads this, so the ship gets lighter and livelier as a burn proceeds. That is free drama:
the last hour of a long burn is the fiercest.

---

## 3. Thrust gravity (P3)

```
accel     = spec.thrust * throttle / mass_total          [m/s^2]
accel_g   = accel / 9.80665
```

This is simultaneously:

- the acceleration the ship is under,
- the gravity the crew stands in,
- the number the deck plan is oriented to (thrust is down, nose is up),
- and the input to crew strain (§6).

**One number, four meanings.** That is why it is the headline and why TWR is not.

### 3.1 The bands

| band | accel | what it is |
|---|---|---|
| **drift** | 0 g | freefall. Nothing is down. Loose cargo, floating crew, no strain. |
| **soft** | 0.05 - 0.15 g | the cheap cruise. Crew shuffles, magboots on, everyone comfortable indefinitely. |
| **standard** | 0.3 g | Mars-normal. The default freight burn. Comfortable, expensive. |
| **hard** | 0.6 - 1.0 g | Earth-normal and up. Crew strapped in. Burns the margin. |
| **emergency** | > 1.0 g | strain accrues for everyone. Something has gone wrong. |

Thrust is always along `+Y` in the ship frame, which is already how the sim works, so "down is aft"
needs no new code — only the UI has to say so (PLAN-14).

---

## 4. The burn table

Brachistochrone, flip at the midpoint, over a distance `d` at constant acceleration `a`:

```
t_total = 2 * sqrt(d / a)
v_peak  = sqrt(d * a)
dv      = a * t_total = 2 * sqrt(d * a)
MR      = exp(dv / Ve)
prop    = 1 - 1/MR
```

### 4.1 Per 1 AU (1.496e11 m), Ve = 1.0e6

| burn | crossing | dv | mass ratio | propellant |
|---|---|---|---|---|
| 0.05 g | 12.8 d | 542 km/s | 1.72 | 42 % |
| **0.10 g** | **9.0 d** | **766 km/s** | **2.15** | **53 %** |
| **0.30 g** | **5.2 d** | **1,330 km/s** | **3.78** | **74 %** |
| 1.00 g | 2.9 d | 2,420 km/s | 11.25 | 91 % |

Read the last column. **A ship that can cross 1 AU at 1 g is 91% propellant by mass.** That is why
1 g is an emergency and 0.1-0.3 g is the freight band. The physics writes the gameplay.

### 4.2 Why interstellar needs a jump drive

Alpha Centauri A to B is 11-36 AU depending on where the pair is in its 79.9-year orbit. The
*shortest* crossing at 0.1 g costs about 2,540 km/s and 30 days. Proxima at 8,700 AU is unreachable
at any mass ratio. PLAN-13 owns the jump drive, and this table is the reason it has to exist.

---

## 5. Coast or burn (P5)

The core decision of every contract, and it is not invented — it falls out of two routes across the
same gap.

**Both routes are paid from the transit tank.** The manoeuvring tank is for docking, RCS and rock
work at m/s scale; a Hohmann transfer in a real star system costs tens of km/s and does not fit in
it. The fork is not *which tank* — it is **how much of the same tank**, and that is what makes it
legible on one gauge.

Worked for the slice's first leg, Wayfarer (0.40 AU) out to the jump line (1.038 AU) around Toliman,
`mu = 1.206e20`, gap 95.5 Gm, on a 142 t dry hull:

**Coast** — Hohmann transfer, two impulsive burns, placed as nodes:

```
a_t = (59.8 + 155.3)/2 = 107.6 Gm
dv1 = 9.05 km/s     dv2 = 7.08 km/s     dv_total = 16.14 km/s
t   = pi * sqrt(a_t^3 / mu) = 116.8 days
prop = 142 t * (exp(16140/1e6) - 1) = 2.31 t
```

**Burn** — brachistochrone across the same gap:

| accel | dv | time | propellant |
|---|---|---|---|
| 0.05 g | 433 km/s | 10.2 d | 76.9 t |
| 0.10 g | 612 km/s | 7.2 d | 119.9 t |
| 0.30 g | 1,060 km/s | 4.2 d | 267.9 t |
| 1.00 g | 1,935 km/s | 2.3 d | 841.6 t |

**2.3 tonnes and 117 days, or 120 tonnes and 7 days.** Fifty-two times the propellant to arrive 110
days sooner, out of one tank, shown on one gauge.

The bottom two rows price themselves out: a 142 t hull cannot carry 268 t of propellant, let alone
842 t. They render dim and the table teaches its own ceiling without a tutorial.

The coast is the Spaceflight-Simulator layer — place a node, set prograde, watch the conic open,
warp to apoapsis, circularise. The burn is the Expanse layer. The contract deadline picks between
them. That is the whole loop, and it needed no new mechanic: only for both routes to be priced
honestly and shown side by side (PLAN-14 §4.3).

### 5.1 Planning a transit

```cpp
struct Transit {
    int      target_body  = -1;
    double   distance     = 0.0;   // m, at the planned departure
    double   accel        = 0.0;   // m/s^2, the chosen band
    double   duration     = 0.0;   // s,  2*sqrt(d/a)
    double   dv           = 0.0;   // m/s, a*duration
    double   prop_cost    = 0.0;   // kg, from the rocket equation at departure mass
    double   peak_g       = 0.0;   // accel / 9.80665
    bool     affordable   = false; // prop_cost <= prop_transit
};

Transit plan_transit(const World &w, int target_body, double accel);
```

Pure, `const World&`, no writes — same contract as `sim/program.h`. The chart shows one row per
acceleration band and the player picks one.

### 5.2 Resolving a transit

A transit is not integrated (P4). It is applied the way `warp_to_next_node` already applies time:

1. Deduct `prop_cost` from `prop_transit`.
2. Advance `world.elapsed` by `duration`, propagating every body on its Kepler rail.
3. Place the ship at the target with the arrival velocity matched.
4. Accrue crew strain for `duration` at `peak_g` (§6).
5. Accrue component wear for `duration` at full throttle (PLAN-11 §5.2).
6. Roll transit events against the storylet table (PLAN-15).

Steps 2 and 3 are `propagate()` and a state assignment — both exist. The transit is a **scene
change with a bill**, not a flight.

---

## 6. Crew and g (P6, P7)

Each crew member carries a tolerance, authored per character:

```cpp
float g_tolerance;   // 0.2 (station-born) .. 1.4 (planet-born, trained)
float strain;        // 0..1, accrues above tolerance, recovers at rest
```

```
if (accel_g > g_tolerance)
    strain += STRAIN_RATE * (accel_g - g_tolerance) * dt;
else
    strain -= RECOVER_RATE * dt;
```

`STRAIN_RATE = 0.02 /s per excess g`, `RECOVER_RATE = 0.004 /s`.

At `strain >= 0.6` the crew member's role gate closes: an engineer under strain cannot repair, a
gunner cannot hold a firing solution. At `strain >= 1.0` they are out, and whatever they gated is
gone until they recover.

**This is the one join that makes thrust gravity, crew and story the same mechanic.** A station-born
engineer with `g_tolerance = 0.3` means the 0.3 g burn band is genuinely your ceiling until you
either hire differently or accept losing repairs mid-transit. No dialogue required — the number says
it.

---

## 7. Gates

| # | gate | how |
|---|---|---|
| G1 | `dv = Ve*ln(MR)` round-trips through `prop_for_dv` to 1e-9 relative | unit |
| G2 | the burn table in §4.1 reproduces to 3 significant figures | unit, table-driven |
| G3 | Hohmann 59.8 -> 155.3 Gm about `mu = 1.206e20` gives 16.1 km/s and 116.8 d to 3 sf | unit, against `orbit/transfer.h` |
| G4 | `accel_g` rises monotonically through a burn at fixed throttle | unit, mass falls |
| G5 | `plan_transit` never writes to `World` | signature is `const World&` |
| G6 | resolving a transit conserves: `prop_before - prop_cost == prop_after` | unit |
| G7 | a transit and an equivalent integrated burn agree on arrival position within 0.1% | unit, integration reference |
| G8 | strain accrues only above tolerance and is symmetric in recovery | unit |
| G9 | `mdot = 14000.0` appears nowhere in the tree | grep |

---

## 8. Files touched

```
new   src/sim/transit.h/.cpp     Transit, plan_transit, resolve_transit
edit  src/sim/physics.h          two prop tiers, Ve constants, accel_g, mass_total
edit  src/sim/physics.cpp        integrate manoeuvre prop only; accel_g
edit  src/sim/world.h/.cpp       resolve_transit hook beside warp_to_next_node
edit  src/sim/designs.h          tank tier field
edit  src/hud/frame.cpp          accel_g headline, transit rows, strain
edit  src/game/scene.cpp         delete the mdot readout
edit  src/sim/component_tests.cpp / new transit_tests.cpp   G1-G9
edit  assets/*.json (tank sidecars)  tier field
```
