# PLAN-16 — Assets: one manifest, no fatal loads, portraits, and LOD

Depends on PLAN-10 (D9, D10). Covers the "better asset handling" requirement.

The existing pipeline — three.js sources, `tools/export.mjs`, `.glb` plus a JSON sidecar carrying
colliders, ports and hardpoints — is **good and stays**. What is missing is everything around it:
one index, a safe failure mode, reload without a restart, and a 2D path for portraits.

---

## 1. Decisions

| id | decision | consequence |
|---|---|---|
| **A1** | **One manifest indexes every asset**, with a content hash and a kind. | §2 |
| **A2** | **A missing or broken asset is never fatal in a dev build.** Placeholder, one log line, keep running. Release keeps fatal. | §3. PLAN-10 D9 made this a rule; this plan makes it a system. |
| **A3** | **Hot reload for models, textures, portraits, story packs and the HUD layout.** Poll the manifest hashes. | §4. `--debug` already polls assets at 1 Hz; extend it. |
| **A4** | **Portraits are 2D, chart-ink line engravings**, packed into one atlas. | §5 |
| **A5** | **LOD stays lazy.** One level per ship, `_blocky` used when present, never authored speculatively. | §6. PLAN-08 A7 unchanged. |
| **A6** | **Validation runs at export, not at load.** A bad asset fails `export.mjs`, not the game. | §7 |

---

## 2. The manifest (A1)

Today `assets/models.json` indexes meshes, `assets/textures/manifest.json` indexes maps, and
`assets/designs.json` indexes ships. Portraits and story packs have no index at all. Merge:

`assets/manifest.json`

```json
{
  "version": 2,
  "assets": {
    "nose_hammerhead":  { "kind": "model",    "path": "nose_hammerhead.glb",  "hash": "9c1f..." },
    "rock_silicate":    { "kind": "texture",  "path": "textures/rock_silicate.png",
                          "hash": "4ab2...", "layout": "tile", "srgb": true },
    "tessera_albedo":   { "kind": "texture",  "path": "textures/tessera_albedo.png",
                          "hash": "77de...", "layout": "equirect", "srgb": true },
    "portrait.vhast":   { "kind": "portrait", "path": "portraits/vhast.png",  "hash": "0e41..." },
    "hammerhead":       { "kind": "design",   "path": "designs/hammerhead.json","hash": "b901..." },
    "core":             { "kind": "story",    "path": "story/core.json",      "hash": "2f88..." },
    "flight":           { "kind": "hud",      "path": "hud/flight.json",      "hash": "c730..." }
  }
}
```

`export.mjs` writes it. `hash` is the first 8 bytes of SHA-256 of the file, hex. Every loader in the
engine resolves a **name**, never a path, and `asset_path()` is applied in exactly one place:

```cpp
// src/render/assets.h — the single resolution point
class AssetIndex {
public:
    void load(const std::string &manifest_path);
    bool has(const std::string &name) const;
    const AssetEntry &entry(const std::string &name) const;
    std::string resolve(const std::string &name) const;   // asset_path applied here, once
};
```

PLAN-10 D11 existed because three call sites built paths by hand. After this there is one, and the
bug class is closed rather than patched.

---

## 3. Failure (A2)

```cpp
template <class T>
struct Loaded {
    T           value;
    bool        ok = true;
    std::string why;      // empty when ok
};
```

| kind | placeholder on failure |
|---|---|
| model | a 1 m magenta wireframe cube, named in the viewer as `MISSING:<name>` |
| texture | 8x8 magenta/black checker |
| portrait | the silhouette card, name rendered as text |
| design | the stock hull for that class |
| story pack | skipped, logged; the game runs with fewer storylets |
| hud layout | the built-in default table (PLAN-14 §5.2) |

Each failure logs **once**, with the name, the resolved path and the reason. A missing asset shows
up as magenta in the frame, which is impossible to miss and impossible to ship by accident.

```cpp
#if OPRA_DEV
  #define ASSET_FAIL(name, why) (log_once(name, why), placeholder_for(kind))
#else
  #define ASSET_FAIL(name, why) fatal(...)
#endif
```

Release keeps the old behaviour: a shipped build with a missing asset is a broken build and should
say so loudly.

---

## 4. Hot reload (A3)

`--debug` already polls assets at 1 Hz. Point it at the manifest:

```
every 1 s in dev:
    re-read assets/manifest.json
    for each entry whose hash differs from the loaded one:
        reload that asset in place
        if it is a design, call world.rebuild_from_design()
        if it is the hud layout, rebuild the block table
        if it is a story pack, reload storylets (qualities are untouched)
```

Reloading a **model** must not invalidate the merged vertex buffer mid-frame, so the reload is
queued and applied at the top of the next frame, before `upload_mesh_library`. That function already
releases the previous buffers on re-entry, so the path exists.

Qualities are never reset by a story reload (PLAN-15 §2) — you can edit dialogue and see it without
losing the run.

---

## 5. Portraits (A4)

### 5.1 Direction

Chart-ink line engravings: `VELLUM` on `BULKHEAD`, hatched, the register of a personnel file the
ship carries rather than a painting. Chosen because it matches `tokens.h` exactly, stays consistent
across twenty-plus characters, and costs a fraction of painted portraits.

| property | value |
|---|---|
| size | 480 x 600 source, displayed at 240 x 300 |
| format | PNG, single channel + alpha, tinted at draw time |
| line | 2 px at source, one weight only |
| shading | hatching, never gradient |
| crop | head and shoulders, three-quarter, eyeline at 38% from the top |
| background | transparent — the compartment ground shows through |

Single channel means the portrait is **tinted by token at draw**, so a crew member under strain can
render in `THREAT` without a second asset.

### 5.2 The atlas

Twenty portraits at 480x600 single-channel is 5.8 MB — one 2048x2048 atlas holds 12, so two pages.
`export.mjs` packs them and writes the UV table into the manifest:

```json
"portrait.vhast": { "kind": "portrait", "page": 0, "uv": [0.0, 0.0, 0.234, 0.293] }
```

One texture bind for the whole crew, and the compartment panel is a single quad.

---

## 6. LOD (A5)

Unchanged from PLAN-08 A7. `lod_model()` falls back to the full mesh when `<name>_blocky` is absent.
One LOD level is chosen per **ship**, not per part, so a chain never shows mixed detail.

Add `_blocky` exports only when a 12-module ship at 60 px measurably costs frames, and not before.
The `--debug` budget log already reports per-frame triangle counts; that is the trigger.

---

## 7. Validation at export (A6)

`tools/export.mjs` already audits triangle budgets and minimum feature size. Extend it to fail on:

| check | rule |
|---|---|
| sidecar completeness | every part has mass, kind, span, flange, collider |
| unit sanity | mass in tonnes 0.1-50, thrust in kN 0-5000 |
| collider bounds | `bounds_radius` >= the furthest shape extent |
| port normals | unit length to 1e-6 |
| name collision | no two assets share a manifest name |
| orphan reference | every name in `designs.json` and `crew.json` exists in the manifest |
| power fields | PLAN-11 fields present, defaulted to 0 with a warning |

A broken asset fails the export, which is a build-time error with a file name, not a magenta cube
someone notices in a screenshot three weeks later.

---

## 8. Gates

| # | gate | how |
|---|---|---|
| G1 | exactly one call to `asset_path()` exists outside `AssetIndex` | grep |
| G2 | deleting any single asset file still boots the dev build, with one log line | scripted, over every asset |
| G3 | the same deletion is fatal in a release build | scripted |
| G4 | every manifest hash matches the file on disk after `export.mjs` | unit |
| G5 | editing a design file reloads the ship without a restart | manual, `--debug` |
| G6 | editing a story pack reloads storylets and preserves qualities | unit |
| G7 | an asset named in `designs.json` but absent from the manifest fails the export | `export.mjs` test |
| G8 | the portrait atlas round-trips: every crew id resolves to a valid UV rect | unit |
| G9 | a model reload never applies mid-frame | unit on the queue |
| G10 | `--selftest` passes with the merged manifest | existing harness |

---

## 9. Files touched

```
new   src/render/assets.h/.cpp     AssetIndex, Loaded<T>, placeholders, log_once
new   src/render/portrait.h/.cpp   atlas lookup, tinted draw
new   assets/manifest.json         written by export.mjs
new   assets/portraits/*.png
edit  tools/export.mjs             manifest + hashes + atlas packing + 7 validation
edit  src/render/renderer.cpp      resolve through AssetIndex; delete hand-built paths
edit  src/render/gltf.cpp          non-fatal model load
edit  src/render/texture.cpp       non-fatal texture load
edit  src/sim/designs.cpp          non-fatal design load
edit  src/game/app.cpp             AssetIndex owns loading; hot reload poll
edit  src/game/viewer.cpp          show MISSING: entries
delete assets/models.json, assets/textures/manifest.json   (folded into manifest.json)
```
