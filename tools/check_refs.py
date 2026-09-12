#!/usr/bin/env python3
"""Audits the generated reference images in research/refs/.

Three questions the eye is bad at answering:
  1. Do the tiles actually tile, and do the equirectangular maps wrap at the seam?
     Measured as edge-pair difference against the adjacent-column difference in the
     same image: a texture that wraps has a seam no worse than its own local noise.
  2. Is lighting baked in? A map lit by a sun has a bright side and a dark side, so
     column brightness follows a low-frequency ramp. A flat map does not.
  3. What colours are actually in there, versus the palette the prompt asked for.

With --fix, the seam-repair pass of plan-04 s3.3 writes the maps the pipeline loads:
repaired copies into assets/textures/ (a 32-px band across the wrap is linearly
interpolated between the content just outside it, so the seam joins two columns a
noise step apart by construction), resized to power-of-two through cyclically
extended borders, with a manifest the loader reads. The originals in research/refs/
are never touched, so a regenerated image gets the same treatment.

    python tools/check_refs.py
    python tools/check_refs.py --fix
"""
import argparse
import json
import pathlib
import sys

import numpy as np
from PIL import Image

REFS = pathlib.Path("research/refs")
OUT = pathlib.Path("assets/textures")

TILES = ["40", "41", "42", "43", "56"]
EQUIRECT = ["50", "51", "52", "53", "54", "55"]

# What the prompts asked for, to compare against what came back.
PALETTE = {
    "hull plate": "BAC4C3",
    "light plate": "E2E3D8",
    "shadowed structure": "202E38",
    "bare metal": "667681",
    "copper trim": "C88755",
    "black recess": "0C141A",
    "tinted glass": "376C7C",
    "teal band": "326B70",
    "ochre band": "BC783C",
}


def load(pid):
    # The mule sheet is 02-mule: the needle's own id 03 is reserved for the image that prompt
    # 03-needle-cutter.txt has yet to produce (plan-04 s0).
    name = "02-mule" if pid == "03" else pid
    path = REFS / "{}.png".format(name)
    if not path.exists():
        return None
    return np.asarray(Image.open(path).convert("RGB"), dtype=np.float64)


def seam(img, axis):
    """Edge-pair mean |difference| and the adjacent-pair baseline, same units."""
    if axis == "lr":
        edge = np.abs(img[:, 0] - img[:, -1]).mean()
        base = np.abs(img[:, 0] - img[:, 1]).mean()
    else:
        edge = np.abs(img[0, :] - img[-1, :]).mean()
        base = np.abs(img[0, :] - img[1, :]).mean()
    return edge, base


def baked_light(img):
    """Is a sun baked into this map?

    A sun on a sphere unrolled to equirectangular is exactly one cosine in
    longitude: one maximum, one minimum 180 degrees away, and it darkens ocean and
    continent alike. Geography is broadband - continents, deserts and ice caps put
    energy across many harmonics.

    So: take the column-mean brightness, drop the DC term, and measure what
    fraction of the remaining energy sits in the first harmonic. A lit map is
    dominated by it. A flat map spreads out.

    Returns (fraction in harmonic 1, peak-to-trough of the fitted cosine in 0-255).
    """
    cols = img.mean(axis=(0, 2))
    spectrum = np.abs(np.fft.rfft(cols - cols.mean()))
    energy = (spectrum ** 2).sum()
    if energy <= 1e-9:
        return 0.0, 0.0
    share = float(spectrum[1] ** 2 / energy)
    amplitude = 2.0 * spectrum[1] / len(cols)
    return share, float(2.0 * amplitude)


def dominant(img, k=6, drop_white=False):
    """Coarse colour census: quantise to a 4x4x4 cube and report the top buckets.

    `drop_white` removes the studio backdrop, which is most of a turnaround sheet
    by area and would otherwise be the headline colour of every ship.
    """
    pixels = img.reshape(-1, 3)
    if drop_white:
        keep = pixels.min(axis=1) < 232
        pixels = pixels[keep]
        if len(pixels) == 0:
            return []
    q = (pixels // 64).astype(np.int64)
    flat = q[:, 0] * 16 + q[:, 1] * 4 + q[:, 2]
    counts = np.bincount(flat, minlength=64)
    out = []
    for idx in np.argsort(counts)[::-1][:k]:
        if counts[idx] == 0:
            continue
        mean = pixels[flat == idx].mean(axis=0)
        out.append(("#%02X%02X%02X" % tuple(int(v) for v in mean),
                    100.0 * counts[idx] / len(pixels)))
    return out


def nearest_palette(hexstr):
    rgb = np.array([int(hexstr[i:i + 2], 16) for i in (0, 2, 4)], dtype=np.float64)
    best, dist = None, 1e9
    for name, ref in PALETTE.items():
        r = np.array([int(ref[i:i + 2], 16) for i in (0, 2, 4)], dtype=np.float64)
        d = np.linalg.norm(rgb - r)
        if d < dist:
            best, dist = name, d
    return best, dist


# --------------------------------------------------------------------------- fix
# What the pipeline loads, and which axes plan-04 s0 measured as seamed. 50 and 52 wrap
# left/right badly, 56 tiles left/right but not top/bottom; 40 was accepted as marginal, so
# it goes through with no repair. The originals stay in research/refs/.
TARGETS = {
    "40": ("rock_silicate", "tile", ()),
    "41": ("rock_carbonaceous", "tile", ()),
    "42": ("rock_ore", "tile", ()),
    "43": ("rock_regolith", "tile", ()),
    "50": ("cinder_albedo", "equirect", ("lr",)),
    "51": ("tessera_albedo", "equirect", ()),
    "52": ("tessera_clouds", "equirect", ("lr",)),
    "53": ("tessera_night", "equirect", ()),
    "54": ("vesk_albedo", "equirect", ()),
    "55": ("halberd_albedo", "equirect", ()),
    "56": ("nereid_photosphere", "tile", ("tb",)),
}
BAND = 32  # the plan's own band: 32 px across the wrap


def repair(img, axis, band=BAND):
    """A linear cross-fade across the wrap.

    A band of `band` lines centred on the seam is replaced by an interpolation between the
    content just outside the band on either side. The two anchors are unmodified and the ramp
    reaches them at the band's edges, so the seam joins two lines a noise step apart by
    construction - which is exactly what the wrap audit measures.
    """
    n = img.shape[1] if axis == "lr" else img.shape[0]
    half = band // 2
    before = img[:, n - half - 1] if axis == "lr" else img[n - half - 1, :]
    after = img[:, half] if axis == "lr" else img[half]
    ramp = np.linspace(0.0, 1.0, band)
    out = img.copy()
    for k in range(band):
        index = (n - half + k) % n
        if axis == "lr":
            out[:, index] = (1.0 - ramp[k]) * before + ramp[k] * after
        else:
            out[index, :] = (1.0 - ramp[k]) * before + ramp[k] * after
    return out


def wrap_resize(img, size, wrap):
    """Resize to `size` with cyclically extended borders.

    A Lanczos kernel that clamps at the border invents a seam exactly where the repair just
    removed one (measured: tessera_albedo 3.5x its own noise after a plain resize). The
    feather is extended cyclically in SOURCE pixels, so extended column 0 maps exactly onto
    the output's column 0 and the kernel's border effects live entirely in the cropped strips.
    """
    h, w = img.shape[:2]
    tw, th = size
    sx = BAND
    sy = BAND if wrap == "xy" else 0
    pad = ((sy, sy), (sx, sx), (0, 0)) if sy else ((0, 0), (sx, sx), (0, 0))
    extended = np.pad(np.clip(img, 0, 255).astype(np.uint8), pad, mode="wrap")
    out_w = round((w + 2 * sx) * tw / w)
    out_h = round((h + 2 * sy) * th / h)
    out = Image.fromarray(extended).resize((out_w, out_h), Image.LANCZOS)
    return np.asarray(out)[round(sy * th / h):round(sy * th / h) + th,
                           round(sx * tw / w):round(sx * tw / w) + tw]


def audit_outputs():
    """The review gate (plan-04 s5.1), run on what the pipeline will actually load."""
    manifest = json.loads((OUT / "manifest.json").read_text())
    print("== assets/textures ==")
    print("  {:<22} {:>9} {:>9} {:>9} {:>9}  {}".format(
        "name", "L/R seam", "L/R noise", "T/B seam", "T/B noise", "verdict"))
    worst_share = 0.0
    failed = False
    for name, entry in manifest.items():
        img = np.asarray(Image.open(OUT / "{}.png".format(name)).convert("RGB"), dtype=np.float64)
        lr_e, lr_b = seam(img, "lr")
        tb_e, tb_b = seam(img, "tb")
        tile = entry["layout"] == "tile"
        ok_lr = lr_e <= lr_b * 2.0 + 2.0
        ok_tb = tb_e <= tb_b * 2.0 + 2.0
        if tile:
            verdict = "tiles" if (ok_lr and ok_tb) else "DOES NOT TILE"
        else:
            verdict = "wraps" if ok_lr else "SEAM AT 0/360"
        failed = failed or verdict in ("DOES NOT TILE", "SEAM AT 0/360")
        share, _ = baked_light(img)
        worst_share = max(worst_share, share)
        print("  {:<22} {:>9.2f} {:>9.2f} {:>9.2f} {:>9.2f}  {}  sun {:>4.0%}".format(
            name, lr_e, lr_b, tb_e, tb_b, verdict, share))
    if failed or worst_share > 0.45:
        sys.exit("the audit gate did not pass on assets/textures")


def fix():
    OUT.mkdir(parents=True, exist_ok=True)
    manifest = {}
    for pid, (name, layout, axes) in TARGETS.items():
        img = load(pid)
        if img is None:
            sys.exit("{} missing from research/refs; nothing to repair".format(pid))
        for axis in axes:
            img = repair(img, axis)
        size = (1024, 1024) if layout == "tile" else (2048, 1024)
        wrapped = "xy" if layout == "tile" else "x"
        Image.fromarray(wrap_resize(img, size, wrapped)).save(OUT / "{}.png".format(name))
        manifest[name] = {"source": "research/refs/{}.png".format(pid), "layout": layout,
                          "srgb": True, "size": list(size)}
        print("  {} <- {} repaired: {} -> {}".format(pid, name, axes or "as-is", size))
    (OUT / "manifest.json").write_text(json.dumps(manifest, indent=2, sort_keys=True))
    print("  manifest written: {} entries".format(len(manifest)))
    print()
    audit_outputs()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--fix", action="store_true",
                        help="write the repaired, power-of-two maps into assets/textures/")
    options = parser.parse_args()
    if options.fix:
        fix()
        return

    if not REFS.exists():
        sys.exit("no research/refs")

    print("== tiling and wrap ==")
    print("  a texture wraps when its seam is no worse than its own local noise")
    print("  {:<6} {:>9} {:>9} {:>9} {:>9}  {}".format(
        "id", "L/R seam", "L/R noise", "T/B seam", "T/B noise", "verdict"))
    for pid in TILES + EQUIRECT:
        img = load(pid)
        if img is None:
            print("  {:<6} missing".format(pid))
            continue
        lr_e, lr_b = seam(img, "lr")
        tb_e, tb_b = seam(img, "tb")
        want_tb = pid in TILES
        ok_lr = lr_e <= lr_b * 2.0 + 2.0
        ok_tb = tb_e <= tb_b * 2.0 + 2.0
        if want_tb:
            verdict = "tiles" if (ok_lr and ok_tb) else (
                "L/R only" if ok_lr else ("T/B only" if ok_tb else "DOES NOT TILE"))
        else:
            verdict = "wraps" if ok_lr else "SEAM AT 0/360"
        print("  {:<6} {:>9.2f} {:>9.2f} {:>9.2f} {:>9.2f}  {}".format(
            pid, lr_e, lr_b, tb_e, tb_b, verdict))

    print()
    print("== baked lighting ==")
    print("  a sun is one cosine in longitude; geography is broadband. share is the")
    print("  fraction of column-brightness energy in the first harmonic")
    for pid in EQUIRECT + TILES:
        img = load(pid)
        if img is None:
            continue
        share, amp = baked_light(img)
        flag = "SUN BAKED IN" if (share > 0.45 and amp > 6.0) else "flat"
        print("  {:<6} share {:>5.0%}  cosine peak-to-trough {:>5.1f}/255   {}".format(
            pid, share, amp, flag))

    print()
    print("== observed palette ==")
    for pid, label, drop in (("00", "material board", False),
                             ("01", "kestrel hull", True),
                             ("03", "tug hull", True)):
        img = load(pid)
        if img is None:
            print("  {} ({}) missing".format(pid, label))
            continue
        print("  {} - {}:".format(pid, label))
        for hexstr, pct in dominant(img, 8, drop_white=drop):
            name, dist = nearest_palette(hexstr[1:])
            note = name if dist < 55 else "(no close match)"
            print("    {}  {:>5.1f}%   nearest: {:<20} d={:.0f}".format(hexstr, pct, note, dist))


if __name__ == "__main__":
    main()
