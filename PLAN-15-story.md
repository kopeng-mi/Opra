# PLAN-15 — Story: qualities, storylets, crew, contracts

Depends on PLAN-10 (the `story/` module boundary) and PLAN-12 (crew strain).

Everything the player does that is not flying lives here, and all of it is data. The C++ is a small
interpreter; the game is `assets/story/*.json`.

---

## 1. Decisions

| id | decision | consequence |
|---|---|---|
| **S1** | **Quality-based narrative.** A flat bag of named numbers, plus storylets that unlock when their requirements are met. | §2. Proven model (Sunless Sea, Fallen London). Roughly 200 lines of C++. |
| **S2** | **Clocks.** Named counters that advance on a trigger and fire a storylet when full. | §4. Things happen while you are elsewhere. |
| **S3** | **Crew are data**, with a role gate, a g-tolerance and a trust quality. | §5 |
| **S4** | **A contract is a storylet chain with a deadline and a payout.** Not a separate system. | §6 |
| **S5** | **`story/` reads `const World&` and writes only `StoryState`.** A storylet may not move the ship. | §7. PLAN-10 F2. |
| **S6** | **Effects are a fixed vocabulary**, not an expression language. | §3.2. Same reasoning as the HUD predicates (PLAN-14 §5.2). |
| **S7** | **Deterministic.** A storylet roll draws from a seeded stream, so a save and a replay agree. | §8 |

---

## 2. Qualities (S1)

```cpp
// src/story/quality.h
class Qualities {
public:
    double get(const std::string &id) const;          // absent == 0
    void   set(const std::string &id, double value);
    void   add(const std::string &id, double delta);
    const std::unordered_map<std::string, double> &all() const;
};
```

One flat namespace, dotted by convention. Nothing is declared in advance; a quality exists the first
time it is written.

| prefix | meaning | examples |
|---|---|---|
| `rep.` | standing with a faction | `rep.authority`, `rep.combine`, `rep.independents` |
| `crew.` | a crew member's state | `crew.vhast.trust`, `crew.vhast.strain` |
| `flag.` | a thing that happened, 0 or 1 | `flag.war_record_known`, `flag.package_opened` |
| `ship.` | a durable fact about the hull | `ship.guns_refitted`, `ship.name_changed` |
| `contract.` | the live contract | `contract.cold_freight.stage` |
| `clock.` | a running clock, in ticks | `clock.debt_due` |

Qualities are the **only** durable story state. Save and load is one JSON object.

### 2.1 Mirrored qualities

Some qualities are owned by the sim, not the story, and are mirrored in read-only each tick so
requirements can test them:

```
sim.accel_g      sim.prop_transit_frac    sim.heat        sim.hull_integrity
sim.system       sim.docked               sim.cargo_mass  sim.elapsed_days
```

A storylet requirement may read these. An effect may **not** write them (S5). The mirror is one
function in `story/mirror.cpp` and it is the only place `story/` touches `World`.

---

## 3. Storylets

### 3.1 Schema

`assets/story/<pack>.json`:

```json
{
  "id": "vhast.radiator_complaint",
  "title": "Number three is still bent",
  "register": "compartment",
  "speaker": "vhast",
  "requires": [
    { "q": "sim.accel_g",        "op": ">=", "v": 0.5 },
    { "q": "crew.vhast.trust",   "op": ">=", "v": 2 },
    { "q": "flag.radiator_bent", "op": "==", "v": 1 }
  ],
  "once": true,
  "weight": 10,
  "body": "The number three radiator is still bent from Ceres...",
  "choices": [
    {
      "text": "Ease off to a third of a g.",
      "effects": [
        { "do": "add", "q": "crew.vhast.trust", "v": 1 },
        { "do": "set", "q": "flag.throttle_promise", "v": 1 }
      ]
    },
    {
      "text": "It holds or it does not.",
      "requires": [ { "q": "crew.vhast.trust", "op": ">=", "v": 4 } ],
      "effects": [
        { "do": "add", "q": "crew.vhast.trust", "v": -1 },
        { "do": "clock", "q": "clock.radiator_fails", "v": 3 }
      ]
    }
  ]
}
```

`op` is one of `==  !=  <  <=  >  >=`. That is the entire requirement language.

### 3.2 The effect vocabulary (S6)

| `do` | meaning |
|---|---|
| `set` | `qualities.set(q, v)` |
| `add` | `qualities.add(q, v)` |
| `clock` | start or advance a clock named `q` with `v` ticks remaining |
| `unlock` | make a storylet id available regardless of its `requires` |
| `contract` | advance the named contract to stage `v` |
| `toast` | one line of HUD text |

Six verbs. An effect naming anything else is a **fatal load error**, like an unknown HUD predicate.
This is deliberate: a story format that can express arbitrary computation is a second program, and
it will be debugged at three in the morning by whoever is unlucky.

### 3.3 Selection

```cpp
// Every storylet whose requires are met, and whose `once` has not fired.
std::vector<const Storylet *> available(const Qualities &q) const;

// One, chosen by weight from the seeded stream. Deterministic (S7).
const Storylet *draw(const Qualities &q, Rng &rng) const;
```

The UI asks for `available()` at a berth (a list to choose from) and `draw()` on a transit event (one
thing happens to you). Same table, two access patterns.

---

## 4. Clocks (S2)

```cpp
struct Clock {
    std::string id;
    int remaining = 0;
    std::string fires;     // storylet id
    std::string trigger;   // "day", "transit", "dock", "jump"
};
```

A clock advances one tick per matching trigger and fires its storylet at zero. Clocks are what make
the world move while the player is doing something else: a debt closing, a buyer losing patience, a
bent radiator finally going.

```
on trigger T:
    for each clock c where c.trigger == T:
        if (--c.remaining <= 0) { queue(c.fires); drop(c); }
```

Clocks live in `StoryState` and serialise with the qualities. Four triggers, no scheduler.

---

## 5. Crew (S3)

```cpp
// src/story/crew.h
enum class Role { Engineer, Pilot, Medic, Quartermaster, Gunner };

struct CrewMember {
    std::string id, name;
    Role        role;
    float       g_tolerance;   // 0.2 .. 1.4   (PLAN-12 6)
    std::string portrait;      // key into the portrait atlas
    std::string voice;         // a tone tag the writing uses
};
```

Authored in `assets/story/crew.json`. Live state (`strain`, `trust`, `on_duty`) is qualities, so it
saves with everything else and storylets can test it.

### 5.1 The role gate

The single mechanical thing crew do. Not a stat bonus — a gate.

```cpp
bool has_role(const StoryState &s, Role r);   // aboard, on duty, strain < 0.6
```

| role | gates |
|---|---|
| Engineer | in-flight repair (PLAN-11 §5.3) |
| Pilot | manoeuvre node execution at full precision; without one, nodes carry an error term |
| Medic | strain recovery rate doubles; without one it halves |
| Quartermaster | contract board access at a berth; cargo capacity accounting |
| Gunner | PDC weapons-free and torpedo launch |

**Burn hard and you close your own gates.** A 1 g burn drives a 0.45-tolerance engineer past 0.6
strain in about seven minutes of sim time, and repairs stop until they recover. That is the join
between PLAN-12's burn table and the crew, and it needs no dialogue to be felt.

---

## 6. Contracts (S4)

A contract is not a new system. It is a storylet chain plus four numbers.

```json
{
  "id": "cold_freight",
  "title": "Cold Freight",
  "issuer": "combine",
  "stages": ["accepted", "package_found", "jumped_out", "jumped_back", "delivered"],
  "deadline_days": 9.0,
  "payout": 42000,
  "rep": { "combine": 2, "authority": -1 },
  "on_accept":  "cold_freight.brief",
  "on_expire":  "cold_freight.late"
}
```

`contract.cold_freight.stage` is a quality. Storylets advance it with the `contract` effect;
requirements test it. The deadline is a clock on the `day` trigger.

The HUD contract line (the `SR-084 Resolve and recover` text in the top strip) reads the active
contract's title and current stage. That is the whole wiring.

---

## 7. Boundaries (S5)

```
World  ---- mirror (read) ---->  StoryState  ---- qualities ---->  HudFrame
                                      |
                                      +-- writes: qualities, clocks, contract stage
                                      +-- never:  ship position, velocity, design, cargo
```

A storylet that should move the ship instead sets a quality that `game/` acts on. For example
`flag.request_undock = 1` is read once by `game/frame.cpp`, which calls `world.undock()` and clears
it. **The story proposes; `game/` disposes.** One indirection, and `story/` stays testable without a
world.

---

## 8. Determinism (S7)

`StoryState` owns an `Rng` seeded from the save. Every `draw()` and every random effect pulls from
it, in a defined order. Two runs from the same save with the same inputs produce the same story.

This matters more than it looks: the existing `--selftest` and golden-capture harness depend on the
whole game being reproducible, and a story layer that reached for a global `rand()` would break
every golden image in the repo.

---

## 9. Gates

| # | gate | how |
|---|---|---|
| G1 | a quality never read returns exactly 0 | unit |
| G2 | an unknown `do` verb fails the load with the file and line | unit |
| G3 | an unknown `op` fails the load | unit |
| G4 | `available()` returns only storylets whose requires all pass | unit, table |
| G5 | `once: true` storylets never appear twice | unit |
| G6 | `draw()` from the same seed and state returns the same id 1000 times | unit |
| G7 | a clock fires exactly at zero, once, and is dropped | unit |
| G8 | no file in `story/` includes `ui/`, `hud/`, `render/` or `game/` | layer check |
| G9 | no effect can write a `sim.` quality | unit, the mirror is const |
| G10 | save -> load -> save produces a byte-identical qualities object | unit |
| G11 | strain >= 0.6 closes the matching role gate in the same tick | unit |

---

## 10. Files touched

```
new   src/story/quality.h/.cpp    Qualities
new   src/story/storylet.h/.cpp   schema, load, requires, effects, available, draw
new   src/story/clock.h/.cpp      Clock, triggers
new   src/story/crew.h/.cpp       CrewMember, Role, has_role
new   src/story/contract.h/.cpp   Contract, stages, deadline
new   src/story/mirror.cpp        the one read of World
new   src/story/state.h           StoryState: qualities + clocks + rng + roster
new   src/story/story_tests.cpp   G1-G11
new   assets/story/crew.json
new   assets/story/contracts.json
new   assets/story/core.json      the starting storylet pack
edit  src/game/app.h/.cpp         owns StoryState, ticks the mirror
edit  src/game/frame.cpp          acts on request flags (7)
edit  src/hud/frame.cpp           contract line, crew rows
edit  CMakeLists.txt              the story/ sources
```
