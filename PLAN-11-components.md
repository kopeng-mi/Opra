# PLAN-11 — Components: condition, power, heat

Depends on PLAN-10. The ship stops being a bag of static numbers and becomes a set of subsystems
that can be damaged, cut, overloaded and cooked.

One idea carries this whole plan: **`derive_spec()` already sums part stats. Make it sum
`stat x integrity`.** Everything else here is the consequence of that multiply.

---

## 0. Where the code is

| fact | where |
|---|---|
| `Placement` is a part name, a slot, a face and a `destroyed` flag | `src/sim/component.h` |
| `derive_spec()` sums part stats into a `ShipSpec` | `src/sim/component.cpp` |
| `ShipSpec` carries `dry`, `fuel`, `thrust`, `hull`, `cooling`, `torque`, `cargo` | `src/sim/physics.h` |
| `PartSpec` is the authored per-part row loaded from the sidecar | `src/sim/designs.h` |
| `World::rebuild_from_design()` re-derives after any design edit | `src/sim/world.h` |
| `ShipState::heat` exists and is integrated | `src/sim/physics.cpp` |
| Part stat table (mass, prop, thrust, hull, cooling, rcs) | `PLAN-08 §2.2` |
| Hull damage is applied per shape by the collider | `src/sim/combat.cpp` |

---

## 1. Decisions

| id | decision | consequence |
|---|---|---|
| **C1** | **Condition is per component, in `[0,1]`, and multiplies every stat it contributes.** | §2. `derive_spec` gains one multiply per term. |
| **C2** | **Three condition axes: `integrity`, `wear`, `heat`.** Integrity is damage (combat, collision). Wear accumulates with use and is cleared at a berth. Heat is per component and feeds the ship total. | §2.1 |
| **C3** | **Power is a budget, not a resource pool.** Supply and draw are both instantaneous kW. Draw over supply browns out the lowest-priority components. | §3. No battery, no charge state. |
| **C4** | **Heat is a single ship-level scalar fed by component rates.** Per-component heat exists for display and failure, not for a conduction network. | §4. A thermal graph is not worth the code. |
| **C5** | **Radiators occlude.** A radiator behind another module, or retracted, contributes nothing. | §4.2. This is what makes radiator placement a real shipyard decision. |
| **C6** | **Damage lands on the component the collider shape belongs to**, not on a global hull pool. | §5. The collider is already composed per placement, so the mapping exists. |
| **C7** | **No repair in flight without an engineer aboard.** The crew gate is the one in PLAN-15; this plan exposes the hook and nothing more. | §5.3 |

---

## 2. Component state

```cpp
// src/sim/component.h — runtime state, one entry per Placement, same index
struct Component {
    float integrity = 1.0f;   // 0..1  battle damage and collisions
    float wear      = 0.0f;   // 0..1  accumulates with use, cleared at a berth
    float heat      = 0.0f;   // K above ambient, display and failure only
    float power     = 0.0f;   // kW drawn when online (authored, not state)
    bool  online    = true;   // the crew can cut any component
};
```

`ShipDesign` gains `std::vector<Component> condition;` kept the same length as `placements`.
`rebuild_from_design()` resizes it and preserves existing entries by slot.

### 2.1 The condition factor

One function, used everywhere. Nothing else is allowed to interpret condition.

```cpp
inline float condition_of(const Component &c) {
    if (!c.online) return 0.0f;
    return c.integrity * (1.0f - 0.35f * c.wear);
}
```

Wear costs at most 35% of a component's output at `wear = 1`. Integrity is linear and can reach
zero. A component at `integrity == 0` is dead but still present: it keeps its mass and its collider.
**A dead tank still weighs what its structure weighs, and still holds whatever propellant has not
leaked.** That asymmetry is the point — damage makes the ship heavier per unit of capability.

### 2.2 derive_spec becomes condition-aware

The existing sum, with one multiply added per term:

```
k_i = condition_of(condition[i])

dry     = SUM  part.mass                      (unchanged — mass does not degrade)
fuel    = SUM  part.propellant * integrity_i  (a holed tank loses its contents)
thrust  = SUM  part.thrust   * k_i
cooling = SUM  part.cooling  * k_i * (1 - occlusion_i)
hull    = SUM  part.heat_capacity * integrity_i
torque  = SUM  part.rcs_authority * |arm_i| * k_i
cargo   = SUM  part.cargo    * integrity_i
```

`dry` deliberately does not take `k_i`: mass is mass. This is what makes a damaged ship fly badly
rather than fly the same and merely report worse.

**The calibration test in `src/sim/component_tests.cpp` stays valid** because every `k_i` is 1.0 on
an undamaged stock design. That is the gate: PLAN-08 §2.3's three ships must still land on `SHIPS[]`
exactly.

---

## 3. Power

Instantaneous, no storage (C3).

```
supply = SUM over reactor parts:  part.power_output * k_i
draw   = SUM over online parts:   part.power        * (online ? 1 : 0)
margin = supply - draw
```

When `margin < 0`, components are browned out in ascending priority until `margin >= 0`. Priority is
authored in the part sidecar as `power_priority` (int, lower sheds first):

| priority | class |
|---|---|
| 0 | cargo heating, external lighting, comms |
| 1 | sensors, survey, scanner |
| 2 | weapons, PDC, cutter |
| 3 | RCS, reaction control |
| 4 | radiators, cooling pumps |
| 5 | life support |
| 6 | drive |

A browned-out component has `online = false` for the frame and contributes nothing. The HUD shows
which ones went dark and why (PLAN-14 §3.2).

**Shedding order is deterministic** — sort by `(priority, slot index)` — so the same damage produces
the same brownout every run. That is what makes it testable.

---

## 4. Heat

### 4.1 The loop

Four lines, once per step, in `step_ship`:

```
heat_in  = SUM online part.heat_rate
         + thrust_fraction * spec.drive_heat            // the torch dominates
heat_out = spec.cooling * radiator_efficiency
ship.heat += (heat_in - heat_out) * dt / spec.hull
ship.heat  = max(ship.heat, 0)
```

`ship.heat` is normalised by `spec.hull` so it reads as a fraction: 0 is cold, 1.0 is the alarm
threshold, above 1.0 components start taking integrity damage.

```
if (ship.heat > 1.0) {
    for each online component:
        integrity -= HEAT_DAMAGE_RATE * (ship.heat - 1.0) * dt;
}
```

`HEAT_DAMAGE_RATE = 0.02 / s`. At `heat = 1.5` a component loses 1% integrity per second — slow
enough to be a warning, fast enough to matter over a long burn.

### 4.2 Radiator occlusion (C5)

A radiator contributes `cooling * k_i * (1 - occlusion_i)`. Occlusion is computed once per design
rebuild, not per frame:

```
For each radiator placement r:
    occlusion = 0
    for each other placement p, p != r:
        if p.slot is within OCCLUDE_SPAN slots of r.slot
           and p.face == r.face:
            occlusion += 0.5
    occlusion = min(occlusion, 1.0)
```

`OCCLUDE_SPAN = 1`. A radiator with a module directly fore or aft on the same face loses half its
capacity; two neighbours kill it entirely. **Radiators want the ends of the chain and clear faces**,
which is exactly the silhouette a torch ship should have, and it falls out of the rule rather than
being drawn by hand.

Retracted radiators (a combat action) set `online = false` and contribute nothing. That is the
tactical trade: run cool and fragile, or armoured and cooking.

### 4.3 Why the torch dominates

A fusion torch at 1.6 MN with an exhaust velocity of 1.0e6 m/s carries a jet power of

```
P_jet = 0.5 * F * Ve = 0.5 * 1.6e6 * 1.0e6 = 8.0e11 W
```

Even 1% waste is 8 GW. Radiating 8 GW at 1000 K needs

```
A = P / (eps * sigma * T^4) = 8e9 / (0.9 * 5.67e-8 * 1e12) = 1.57e5 m^2
```

roughly 400 m x 400 m of panel. The game does not simulate that area — `drive_heat` is a tuned
coefficient — but it keeps the *shape* of the relationship: **heat load scales with thrust, and a
ship that burns hard must be mostly radiator.** That is why radiators are a component class and why
burning at 1 g is a decision and not a button.

---

## 5. Damage

### 5.1 Mapping a hit to a component (C6)

`App::sync_ship_collider` already composes the collider from per-placement sidecar shapes. Record
the owning placement index while composing:

```cpp
struct Shape {
    ...
    int placement = -1;      // which Placement contributed this shape
};
```

A hit that resolves against shape `s` applies to `condition[s.placement]`:

```
integrity -= damage / part.heat_capacity
```

So a heavy module absorbs more. No new collision code — the index is filled in the loop that already
exists.

### 5.2 Wear

```
wear += WEAR_RATE * thrust_fraction * dt     // drives only
wear += WEAR_RATE_RCS * |rcs_input| * dt     // RCS blocks only
```

`WEAR_RATE = 1.0e-6 / s` at full throttle, so a 4.6-day continuous burn at 1.0 accrues about 0.4
wear — a real cost on a long haul, invisible on a short hop. Wear never destroys a component; it
caps at 1.0 and costs 35% output.

### 5.3 Repair

```
can_repair_in_flight = crew.has_role(Role::Engineer) && component.integrity > 0.0
```

Repair rate `0.01 /s` of integrity, consuming spares from cargo. A component at exactly 0 integrity
needs a yard, not an engineer. PLAN-15 owns `crew.has_role`; this plan defines the gate and calls it.

---

## 6. What the shipyard gains

No new screen. `build_shipyard()` already shows derived stats; it now also shows the two budgets:

```
POWER     +142 / -118 kW      margin +24
COOLING    0.055 /s           occluded 1 of 3
```

and the hover delta from PLAN-09 U6 extends to both. A radiator dropped next to another radiator
now visibly shows `+0.000 /s` instead of `+0.020 /s`, which teaches the occlusion rule without a
tutorial.

---

## 7. Gates

| # | gate | how |
|---|---|---|
| G1 | the three stock designs still reproduce `SHIPS[]` exactly at full condition | `component_tests.cpp` unchanged |
| G2 | `condition_of` at `integrity=1, wear=0, online=true` returns exactly `1.0f` | unit |
| G3 | halving a drive's integrity halves `spec.thrust` and leaves `spec.dry` unchanged | unit |
| G4 | brownout order is identical across 100 runs with the same damage | unit, deterministic sort |
| G5 | a radiator with one same-face neighbour contributes exactly half its cooling | unit |
| G6 | `heat > 1.0` sustained for 50 s reduces every online component integrity by `0.02*(h-1)*50` | unit |
| G7 | a hit on a known shape reduces exactly one component | unit, using the recorded placement index |
| G8 | a 4.6-day burn at full throttle produces `wear` in `[0.35, 0.45]` | unit |

---

## 8. Files touched

```
edit  src/sim/component.h      Component struct, condition_of, occlusion
edit  src/sim/component.cpp    derive_spec condition-aware, occlusion pass
edit  src/sim/physics.h        ShipSpec gains power_supply, drive_heat
edit  src/sim/physics.cpp      power budget, heat loop, wear, heat damage
edit  src/sim/designs.h        PartSpec gains power, power_priority, power_output, heat_rate
edit  src/game/app.cpp         sync_ship_collider records placement index
edit  src/sim/combat.cpp       hit applies to the owning component
edit  src/ui/screens.cpp       shipyard shows power and cooling budgets
edit  src/sim/component_tests.cpp   G2-G8
edit  assets/*.json (sidecars) new authored fields, defaulted to 0
```
