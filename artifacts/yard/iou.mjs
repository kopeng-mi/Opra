// PLAN-07 gate 6 / plan-05 s3.4: pairwise silhouette IoU across the designs, on the normalised
// plan view. Two ships whose outlines overlap above 0.70 are the same ship.
//
// export.mjs has this, but audit_silhouettes() filters to SHIP_NAMES - the three hulls plan 07 s9
// P8 retires - so the measurement it owns cannot see a design yet. The normalisation below is the
// same one (bounding box to a 64x64 grid, nearest sample), reproduced rather than exported, since
// the exporter is shipping code and this is a bench.
import { pathToFileURL } from 'node:url';
import { audit_legibility } from '../../tools/export.mjs';

const GRID = 64;

function mask(raster) {
  const out = new Uint8Array(GRID * GRID);
  let min_x = Infinity, max_x = -Infinity, min_y = Infinity, max_y = -Infinity;
  for (let y = 0; y < raster.height; ++y) {
    for (let x = 0; x < raster.width; ++x) {
      if (raster.id[y * raster.width + x] < 0) continue;
      min_x = Math.min(min_x, x); max_x = Math.max(max_x, x);
      min_y = Math.min(min_y, y); max_y = Math.max(max_y, y);
    }
  }
  const span_x = Math.max(1, max_x - min_x), span_y = Math.max(1, max_y - min_y);
  for (let gy = 0; gy < GRID; ++gy) {
    for (let gx = 0; gx < GRID; ++gx) {
      const sx = min_x + ((gx + 0.5) / GRID) * span_x;
      const sy = min_y + ((gy + 0.5) / GRID) * span_y;
      if (raster.id[Math.floor(sy) * raster.width + Math.floor(sx)] >= 0) out[gy * GRID + gx] = 1;
    }
  }
  return out;
}

const rows = [];
for (const source of process.argv.slice(2)) {
  const module = await import(pathToFileURL(source).href);
  const root = module.build();
  root.updateMatrixWorld(true);
  rows.push({ name: module.meta.name, mask: mask(audit_legibility(root, module.meta.name, 0).raster) });
}
let failed = 0;
for (let i = 0; i < rows.length; ++i) {
  for (let j = i + 1; j < rows.length; ++j) {
    let inter = 0, union = 0;
    for (let k = 0; k < GRID * GRID; ++k) {
      if (rows[i].mask[k] && rows[j].mask[k]) ++inter;
      if (rows[i].mask[k] || rows[j].mask[k]) ++union;
    }
    const iou = union > 0 ? inter / union : 0;
    const pass = iou < 0.70;
    if (!pass) failed++;
    console.log(`  IoU  ${rows[i].name.padEnd(22)} / ${rows[j].name.padEnd(22)} ${iou.toFixed(3)}  ${pass ? 'ok' : 'FAIL (max 0.70)'}`);
  }
}
process.exit(failed === 0 ? 0 : 1);
