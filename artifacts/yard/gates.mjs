// The yard's bench gates. Run a part or an assembly through the shipping audit plus the two new
// measurements plan 07 adds, without touching assets/ or the exporter.
//
//   bun run gates.mjs parts/spine_truss_m.ts
//   bun run gates.mjs ../../tools/models/kestrel.ts
//
// Why two more gates. Every one of the 33 shipping models passes the existing audit and the Kestrel
// still reads as a lozenge, because the two things that made it a lozenge were never measured:
//
//   SECTION VARIANCE   a constant-width extrusion has nothing to read along its length. Sampled as
//                      the plan-view width at each station: (max - min) / max over the run.
//                      kestrel scores near zero; an assembly with proud parts scores high.
//   MATERIAL DOMINANCE one material owning the plan view is what "no colour differentiation" means
//                      numerically. kestrel: lightArmor 60% + armor 19% = 79% of one grey family.
//
// Both are cheap, both are objective, and either one alone would have caught the current hull.
import { pathToFileURL } from 'node:url';
import * as THREE from 'three';
import { audit_legibility, HOME_PX_PER_M } from '../../tools/export.mjs';

/** Ships must clear this; a 3:1 rectangle scores 21.3 and the old floor was 22 (plan 07 s5). */
export const MIN_SHIP_COMPLEXITY = 60.0;
/** Fraction of plan-view area the largest material may hold. */
export const MAX_MATERIAL_SHARE = 0.45;
/**
 * Coefficient of variation (std / mean) of the plan-view width sampled along the length.
 *
 * Range-based variance does not work here: it is dominated by the single narrowest band, so a
 * constant-width slab that happens to taper at one end scores the same as a real assembly. Measured:
 * kestrel scores 0.11 and mule 0.35 on CV, against 0.31 and 0.73 on range - only CV separates them.
 * 0.20 sits between the two with room either side.
 */
export const MIN_SECTION_VARIANCE = 0.20;
/** Station pitch, metres. The spine's stations and every axial part's length are multiples of it. */
export const STATION_PITCH_M = 4.0;

/**
 * Plan-view width at each sample band along +Y, in metres. Bands are one station tall so the
 * number means "how wide is the ship at station i", which is the thing the eye reads.
 */
export function section_widths(raster, band_m = STATION_PITCH_M) {
  const { id, width, height, px_per_m } = raster;
  const band_px = Math.max(1, Math.round(band_m * px_per_m));
  const widths = [];
  for (let y0 = 0; y0 < height; y0 += band_px) {
    let lo = Infinity, hi = -Infinity;
    for (let y = y0; y < Math.min(y0 + band_px, height); ++y) {
      for (let x = 0; x < width; ++x) {
        if (id[y * width + x] < 0) continue;
        if (x < lo) lo = x;
        if (x > hi) hi = x;
      }
    }
    // A band with no geometry is a real gap (the fork in a stern, the open bay of a truss) and
    // belongs in the run as a zero, not dropped - a gap is the strongest section change there is.
    widths.push(hi >= lo ? (hi - lo + 1) / px_per_m : 0);
  }
  return widths;
}

export function section_variance(raster) {
  const all = section_widths(raster);
  // The raster carries two pixels of padding, so the first and last bands can be empty through
  // no fault of the model. Trim only the empty ends: an empty band *between* two solid ones is a
  // real gap and is the whole point of the measurement.
  let a = 0, b = all.length - 1;
  while (a <= b && all[a] === 0) a++;
  while (b >= a && all[b] === 0) b--;
  const widths = all.slice(a, b + 1);
  if (widths.length < 2) return { variance: 0, widths };
  const mean = widths.reduce((s, w) => s + w, 0) / widths.length;
  if (mean <= 0) return { variance: 0, widths };
  const std = Math.sqrt(widths.reduce((s, w) => s + (w - mean) * (w - mean), 0) / widths.length);
  return { variance: std / mean, widths };
}

export function material_shares(raster) {
  const counts = new Array(raster.materials.length).fill(0);
  let total = 0;
  for (let i = 0; i < raster.id.length; ++i) {
    const m = raster.id[i];
    if (m < 0) continue;
    counts[m]++;
    total++;
  }
  if (total === 0) return [];
  return counts
    .map((n, i) => ({ name: raster.materials[i]?.name || `mat${i}`, share: n / total }))
    .filter((row) => row.share > 0)
    .sort((a, b) => b.share - a.share);
}

/** perimeter^2 / area of the plan silhouette. A circle is 4pi (12.6), a 3:1 rectangle 21.3. */
export function complexity(raster) {
  const { id, width, height, px_per_m } = raster;
  const solid = (x, y) => x >= 0 && y >= 0 && x < width && y < height && id[y * width + x] >= 0;
  let area = 0, boundary = 0;
  for (let y = 0; y < height; ++y) {
    for (let x = 0; x < width; ++x) {
      if (!solid(x, y)) continue;
      area++;
      // Each of the four edges that faces empty space contributes one pixel of perimeter.
      if (!solid(x - 1, y)) boundary++;
      if (!solid(x + 1, y)) boundary++;
      if (!solid(x, y - 1)) boundary++;
      if (!solid(x, y + 1)) boundary++;
    }
  }
  if (area === 0) return 0;
  const perimeter_m = boundary / px_per_m;
  const area_m2 = area / (px_per_m * px_per_m);
  return (perimeter_m * perimeter_m) / area_m2;
}

function triangles_of(root) {
  let total = 0;
  root.traverse((node) => {
    if (!node.isMesh || node.userData.effect) return;
    const position = node.geometry.attributes.position;
    total += node.geometry.index ? node.geometry.index.count / 3 : position.count / 3;
  });
  return Math.round(total);
}

export async function bench(source, { ship = true } = {}) {
  const module = await import(pathToFileURL(source).href);
  const root = module.build();
  root.updateMatrixWorld(true);
  const report = audit_legibility(root, module.meta?.name || 'part', triangles_of(root));
  const raster = report.raster;
  const out = { name: module.meta?.name || source, ok: report.ok, lines: [...report.lines] };
  if (!raster) return out;

  const shape = complexity(raster);
  const { variance, widths } = section_variance(raster);
  const shares = material_shares(raster);
  const top = shares[0] || { name: '-', share: 0 };

  const gate = (label, value, pass, rule) => {
    out.lines.push(`  ${label.padEnd(17)}${value}${pass ? ' ok' : ' FAIL'}   (${rule})`);
    if (!pass) out.ok = false;
  };
  gate('complexity', shape.toFixed(1), !ship || shape >= MIN_SHIP_COMPLEXITY,
       `ships >= ${MIN_SHIP_COMPLEXITY}; rectangle scores 21.3`);
  gate('section variance', variance.toFixed(2), variance >= MIN_SECTION_VARIANCE,
       `>= ${MIN_SECTION_VARIANCE}, std/mean of width per station`);
  gate('top material', `${top.name} ${(top.share * 100).toFixed(0)}%`, top.share <= MAX_MATERIAL_SHARE,
       `<= ${(MAX_MATERIAL_SHARE * 100).toFixed(0)}% of plan area`);
  out.lines.push('  width per station: ' + widths.map((w) => w.toFixed(1)).join(' '));
  return out;
}

if (import.meta.main) {
  const args = process.argv.slice(2);
  if (args.length === 0) {
    console.error('usage: bun run gates.mjs <part.ts> [...]   (px/m at home framing: ' + HOME_PX_PER_M + ')');
    process.exit(2);
  }
  let failed = 0;
  for (const source of args) {
    const report = await bench(source);
    console.log(report.lines.join('\n'));
    if (!report.ok) failed++;
  }
  console.log(failed === 0 ? `\nbench: ${args.length} pass` : `\nbench: ${failed} of ${args.length} FAIL`);
  process.exit(failed === 0 ? 0 : 1);
}
