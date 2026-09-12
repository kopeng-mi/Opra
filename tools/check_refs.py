#!/usr/bin/env python3
"""Audits the generated reference images in research/refs/.

Three questions the eye is bad at answering:
  1. Do the tiles actually tile, and do the equirectangular maps wrap at the seam?
     Measured as edge-pair difference against the adjacent-column difference in the
     same image: a texture that wraps has a seam no worse than its own local noise.
  2. Is lighting baked in? A map lit by a sun has a bright side and a dark side, so
     column brightness follows a low-frequency ramp. A flat map does not.
  3. What colours are actually in there, versus the palette the prompt asked for.

    py tools/check_refs.py
"""
import pathlib
import sys

import numpy as np
from PIL import Image

REFS = pathlib.Path("research/refs")

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
    path = REFS / "{}.png".format(pid)
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


def main():
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
