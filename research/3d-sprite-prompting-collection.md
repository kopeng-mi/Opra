# 3D Sprite Image Generation — Prompting Data Collection (ChatGPT Web)

Collected 2026-09-12. Sources: OpenAI prompting guides, Meshy/Sloyd/Scenario docs, OpenAI community forums, Reddit (r/aigamedev, r/ChatGPT), prompt-marketplace keyword sets. Framed for **ChatGPT web** image generation (DALL-E 3 / GPT-4o lineage in the chat UI).

---

## 1. Core Prompt Formula (cross-source consensus)

**Subject + Material/Texture + Art Style + Technical Constraints** (Meshy's validated formula — earliest words carry the most weight).

For ChatGPT web sprite work, the proven structure from multiple sources:

```
1. Subject & type      — what the asset/character IS
2. Pose/action         — exact stance, per-frame action
3. Render style        — "3D render, low-poly, soft shading"
4. Camera/perspective  — "isometric, orthographic, 30-45° top-down"
5. Lighting            — "soft studio lighting, ambient occlusion"
6. Background          — "solid white background" / "transparent background"
7. Constraints         — "no shadows beyond object, no text, no watermark"
```

## 2. ChatGPT-web-specific technique: describe-before-generate

The most-upvoted Reddit method (r/ChatGPT "unlock next-level art"): **use ChatGPT itself as the visual-description engine first**.

1. Ask ChatGPT to *write* an extremely vivid, verbose description of the target image — subject, composition, style, lighting, camera, color palette (even hex codes), mood, intended use.
2. Then feed that description back verbatim as the image-generation prompt.
3. Tweak single elements of the description before re-rendering.

Why it works: separates creative brainstorming (LLM strength) from image synthesis; gives an editable "prompt source of truth" you can version and reuse for consistency across a whole asset set.

## 3. Isometric 3D sprite keyword block (validated set)

From prompt-marketplace/Leonardo/Midjourney sources, the keyword stack that reliably produces the "Overcooked/Chibi clean 3D isometric" look:

```
isometric 3D game asset, orthographic view, 45-degree top-down perspective,
white background, pure white studio lighting, ambient occlusion baked,
sharp edges, PBR textures, clean high-contrast lighting, low-poly stylized,
crisp render, sharp focus, no background clutter
```

Consistency rules for an asset *set* (keep identical across every prompt):
- Same render keywords ("3D render", "Blender-style rendering", "octane")
- Same lighting description (soft-box / rim / HDRI)
- Same camera settings (angle, distance, focal length)
- Same resolution/aspect; same palette words

Five-part structure that ends clean for compositing:
> "A high-poly sci-fi rifle, barrel pointing forward, brushed-metal PBR material, rendered in Octane with HDRI soft-box lighting, 50mm lens, eye-level view, solid white background, no shadows beyond the object."

## 4. Sprite-sheet prompting (ChatGPT web can do grids)

Working template (Reddit/ChatGPT+DALL-E workflow):

> "Create a transparent-background sprite sheet (4 × 4 frames) of a [character/asset] for a [theme] game, viewed from a top-down 30° isometric angle, rendered in low-poly 3D style with soft ambient occlusion, [palette] colors, consistent lighting and shadows across all frames; each frame shows [animation: idle / walk-right / walk-left / attack]. Same camera distance, same material settings in every frame."

Hard-won forum lessons (OpenAI community, gpt-image sprite-sheet thread, 38 posts / 102 likes):
- **Full sheets in one shot are unreliable** — repeated poses, incomplete grids. Model is optimized for visual quality, not spatial frame discipline.
- **Work frame-by-frame** for better results; generate sheet first only as a rough draft, then refine individual frames.
- **Left/right limb confusion is endemic** — the model interprets "left leg" as screen-left, not anatomical left. Work *with* the bias rather than fighting it; re-run the same prompt 2–3 times rather than asking for fixes (rerun beats repair).
- **Good reference images matter more than prompt wording**: low-res/pixel-art references make depth and limb order ambiguous; a **3D mannequin-style reference** (clear joints, shadows, structure) is the best anchor; **numbered frames** in the reference make specific poses targetable.
- Pose-transfer pattern: Image 1 = pose reference, Image 2 = character; prompt must explicitly say "transfer pose WITHOUT MIRRORING; left/right = subject's anatomical left/right; preserve leading/trailing limbs, planted/lifted feet, contact points."
- Normalization pass on assembled frames: "normalize the style, character consistency and size for this sprite sheet, keeping all the poses intact."
- Alternative that sidesteps frames entirely: **multi-panel time-lapse trick** — describe a wide image as a 6×4 borderless grid of "photographs taken one second apart, camera position drifting consistently" → cut quadrants into sprites.

## 5. Reference-image & edit workflow (ChatGPT web supports image inputs)

From OpenAI's prompting fundamentals (model-agnostic, applies to web UI):
- **Assign roles to references**: "Image 1 = pose, Image 2 = character, Image 3 = style." Say how they combine.
- **Separate changes from constraints**: "change only X; preserve identity, proportions, lighting, background."
- **Iterate deliberately**: pass previous output back as next input, one change per turn, restate critical constraints each time (drift is real).
- **Transparent background**: explicitly request "fully transparent background, no solid backdrop, no checkerboard, no watermark" — a *drawn* checkerboard is not transparency; verify alpha edges after download.
- **Style transfer**: "Use the same style from the input image and generate [new subject] on a white background."
- If a region must stay pixel-identical, composite externally instead of re-prompting.

## 6. Consistency across many assets (the real bottleneck)

Scenario's production write-up: generating frames is easy; keeping 80+ poses consistent is the hard part. Without anchoring, proportions shift, palette drifts, faces change between idle and attack.

Mitigations, in order of strength:
1. **Reference image anchoring** — upload one canonical character/asset image with every request; the model reads visual properties from it.
2. **Frozen style block** — identical keyword tail in every prompt (see §3).
3. **Custom-trained model** (Scenario: 5–15 reference images, 30–60 min train) — outside ChatGPT web, but the ceiling for large libraries.
4. **Video-to-frame pipeline** — generate character image → animate with a video model (Seedance/Veo) → extract 8 key frames → assemble sheet. Best frame-to-frame consistency of any method; Reddit consensus agrees ("best approach for smooth animation is generating a video and extracting frames").

Frame counts: 8 frames covers a basic loop; attacks 12–16.

## 7. Asset-type prompt patterns (Sloyd data-driven table)

| Asset type | Structure | Example |
|---|---|---|
| Props | Object + 2 key attributes | "stone well with oriental roof" |
| Weapons | Base type + specific style | "curved sword, damascus steel" |
| Buildings | Architecture + time period | "victorian mansion, 19th century" |
| Furniture | Item + material + style | "wooden chair, rustic farmhouse" |

Rules: split complex subjects (body / armor / weapons / accessories as separate prompts) — finer control, sharper results. Keep prompts under ~50 words for 3D generation. Curly-brace the must-haves: `{medieval knight armor} gold trim, game-ready`. Empty adjectives ("amazing beautiful epic") are noise; contradictory material lists fail.

## 8. Do / Don't (Meshy's good-vs-bad table, applies to image gen too)

| Do | Don't |
|---|---|
| "wooden treasure chest with iron bands" | "a box" |
| "front-facing, centered, white background" | "amazing beautiful epic" |
| "low-poly cartoon style" | "make it look good" |
| "weathered stone surface with moss" | "stone and wood and metal and glass" |

Negative prompts / exclusions: "no background elements", "no floating particles", "no text or labels", "no watermark".

## 9. Pipeline for feeding visuals into an LLM (your use case)

Since the goal is images as LLM input (vision feedback loop):
1. Generate on **solid white or transparent background** — removes segmentation ambiguity when the LLM analyzes the sprite.
2. One asset per image while iterating; grid sheets only for final review.
3. Number frames / label variants in-chat ("version A, version B") so the LLM can reference them unambiguously.
4. Ask the LLM to critique against the *same frozen spec* each round (palette hexes, angle, silhouette) — closes the loop without drift.
5. For pixel-art output: generate hi-res → downscale → re-pixelate; AI never produces true pixel grids natively (Reddit consensus; cleanup step required).

## 10. Source index

- OpenAI image-prompting guide — developers.openai.com/api/docs/guides/image-prompting (fundamentals §, edit patterns, transparency rules)
- OpenAI community: "Developing sprite sheets with gpt-image-2" — community.openai.com/t/1379831 (frame-by-frame, mannequin refs, pose-transfer prompt, normalization)
- Meshy 3D Prompting Guide — docs.meshy.ai/en/webapp/guides/prompting (formula, do/don't, game-ready keywords)
- Sloyd: 7 Best Practices — sloyd.ai/blog/7-best-practices-for-ai-generated-3d-models-in-game-development (asset-type table, decomposition)
- Scenario: AI Sprite Generator — scenario.com/blog/ai-sprite-generator (3 methods, video-to-frame, consistency problem)
- Reddit r/ChatGPT 1jr0qei — describe-before-generate technique
- Reddit r/aigamedev 1r9qfbh — consistent HD isometric (ControlNet Canny+Depth, seed/LoRA locking — SD-side equivalent)
- Reddit r/aigamedev 1u363g x / 1nckh1v / 1t0odg6 — best sprite methods; video-frame extraction; cleanup pipelines; Retro Diffusion for pixel art
- Keyword sets: neura.market, promptbase.com 3D-isometric generator, Medium isometric prompt collections
