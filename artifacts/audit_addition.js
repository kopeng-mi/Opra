// ---------------------------------------------------------------------------
// Plan 05 s3: the legibility audit. Rasterises the merged geometry orthographically down +Z at the
// home pixels-per-metre and answers the only question that matters: will a player see this?
// ---------------------------------------------------------------------------

/** s3.1: the home framing is 150 m of half-height at 900 px tall - 3.00 px per metre. */
export const HOME_PX_PER_M = 3.0;
/** s3.2's floors. */
export const MIN_FEATURE_M = 1.5;
export const MIN_IDENTITY_M = 3.0;
export const MIN_REGION_M2 = 6.0;
/** s3.2's bold triangle budget; structures carry a looser one until their own pass lands. */
export const BOLD_TRI_BUDGET = 1500;
export const STRUCTURE_TRI_BUDGET = 6000;
/** s3.4's silhouette gates. */
export const MAX_SILHOUETTE_IOU = 0.7;
export const MIN_COMPLEXITY = 22.0;

const SHIP_NAMES = new Set(['kestrel', 'mule', 'needle']);

/**
 * The plan-view raster: every static triangle projected down +Z at `px_per_m`, z-buffered so the
 * deck's material wins over the keel's. Returns the per-pixel material id, the silhouette that
 * falls out of it, and the smallest per-part dimension the geometry carries.
 */
function raster_plan(root, px_per_m) {
  const triangles = [];
  const part_sides = [];
  const materials = [];
  const mat_ids = new Map();
  root.traverse((node) => {
    if (!node.isMesh || node.userData.effect) return;
    const material = Array.isArray(node.material) ? node.material[0] : node.material;
    let id = mat_ids.get(material);
    if (id === undefined) {
      id = materials.length;
      mat_ids.set(material, id);
      materials.push(material);
    }
    const geometry = node.geometry.index ? node.geometry.toNonIndexed() : node.geometry;
    const position = geometry.attributes.position;
    const a = new THREE.Vector3(), b = new THREE.Vector3(), c = new THREE.Vector3();
    for (let i = 0; i + 2 < position.count; i += 3) {
      a.fromBufferAttribute(position, i).applyMatrix4(node.matrixWorld);
      b.fromBufferAttribute(position, i + 1).applyMatrix4(node.matrixWorld);
      c.fromBufferAttribute(position, i + 2).applyMatrix4(node.matrixWorld);
      triangles.push([a.x, a.y, a.z, b.x, b.y, b.z, c.x, c.y, c.z, id]);
    }
    if (geometry !== node.geometry) geometry.dispose();
    // The part's own AABB, for the smallest-feature check (s3.2): a feature is a part here, which
    // is how the models are authored - one primitive call, one part.
    geometry.computeBoundingBox();
    const box = geometry.boundingBox.clone().applyMatrix4(node.matrixWorld);
    part_sides.push(Math.min(box.max.x - box.min.x, box.max.y - box.min.y, box.max.z - box.min.z));
  });
  if (triangles.length === 0) return null;

  let min_x = Infinity, max_x = -Infinity, min_y = Infinity, max_y = -Infinity;
  for (const t of triangles) {
    for (let k = 0; k < 9; k += 3) {
      min_x = Math.min(min_x, t[k]); max_x = Math.max(max_x, t[k]);
      min_y = Math.min(min_y, t[k + 1]); max_y = Math.max(max_y, t[k + 1]);
    }
  }
  const pad = 2;
  const width = Math.ceil((max_x - min_x) * px_per_m) + pad * 2;
  const height = Math.ceil((max_y - min_y) * px_per_m) + pad * 2;
  const id = new Int32Array(width * height).fill(-1);
  const depth = new Float64Array(width * height).fill(-Infinity);
  const to_px = (x, y) => [(x - min_x) * px_per_m + pad, (max_y - y) * px_per_m + pad];

  for (const t of triangles) {
    const [ax, ay] = to_px(t[0], t[1]);
    const [bx, by] = to_px(t[3], t[4]);
    const [cx, cy] = to_px(t[6], t[7]);
    const x0 = Math.max(0, Math.floor(Math.min(ax, bx, cx)));
    const x1 = Math.min(width - 1, Math.ceil(Math.max(ax, bx, cx)));
    const y0 = Math.max(0, Math.floor(Math.min(ay, by, cy)));
    const y1 = Math.min(height - 1, Math.ceil(Math.max(ay, by, cy)));
    const area = (bx - ax) * (cy - ay) - (cx - ax) * (by - ay);
    if (Math.abs(area) < 1e-9) continue;
    for (let py = y0; py <= y1; ++py) {
      for (let px = x0; px <= x1; ++px) {
        // Barycentric containment at the pixel centre.
        const w0 = ((bx - ax) * (py + 0.5 - ay) - (px + 0.5 - ax) * (by - ay)) / area;
        const w1 = ((px + 0.5 - ax) * (cy - ay) - (cx - ax) * (py + 0.5 - ay)) / area;
        const w2 = 1.0 - w0 - w1;
        if (w0 < 0 || w1 < 0 || w2 < 0) continue;
        const z = w0 * t[2] + w1 * t[5] + w2 * t[8];
        const at = py * width + px;
        if (z > depth[at]) {
          depth[at] = z;
          id[at] = t[9];
        }
      }
    }
  }
  let smallest = Infinity;
  for (const side of part_sides) smallest = Math.min(smallest, side);
  return { width, height, id, materials, bbox: [min_x, min_y, max_x, max_y], smallest_feature: smallest, px_per_m };
}

/** Material pixel counts over the plan raster, largest first. */
function material_areas(raster) {
  const counts = new Map();
  for (let i = 0; i < raster.id.length; ++i) {
    const at = raster.id[i];
    if (at < 0) continue;
    counts.set(at, (counts.get(at) ?? 0) + 1);
  }
  const px_per_m2 = raster.px_per_m * raster.px_per_m;
  const rows = [];
  for (const [id, pixels] of counts) {
    const name = raster.materials[id]?.name ?? `material ${id}`;
    rows.push({ name, pixels, area_m2: pixels / px_per_m2 });
  }
  rows.sort((a, b) => b.area_m2 - a.area_m2);
  return rows;
}

/** The silhouette's complexity (s3.4): perimeter squared over area, from boundary edges. */
function silhouette_complexity(raster) {
  const { width, height, id } = raster;
  let area = 0;
  let boundary = 0;
  const inside = (x, y) => x >= 0 && y >= 0 && x < width && y < height && id[y * width + x] >= 0;
  for (let y = 0; y < height; ++y) {
    for (let x = 0; x < width; ++x) {
      if (!inside(x, y)) continue;
      ++area;
      if (!inside(x + 1, y)) ++boundary;
      if (!inside(x - 1, y)) ++boundary;
      if (!inside(x, y + 1)) ++boundary;
      if (!inside(x, y - 1)) ++boundary;
    }
  }
  if (area === 0) return { complexity: 0, perimeter_m: 0, area_m2: 0 };
  const perimeter_m = (boundary / 2) / raster.px_per_m;
  const area_m2 = area / (raster.px_per_m * raster.px_per_m);
  return { complexity: (perimeter_m * perimeter_m) / area_m2, perimeter_m, area_m2 };
}

/** Normalises a silhouette to a shared grid by its own bounding box, for the pairwise IoU. */
function normalised_silhouette(raster, grid = 64) {
  const out = new Uint8Array(grid * grid);
  let min_x = Infinity, max_x = -Infinity, min_y = Infinity, max_y = -Infinity;
  for (let y = 0; y < raster.height; ++y) {
    for (let x = 0; x < raster.width; ++x) {
      if (raster.id[y * raster.width + x] < 0) continue;
      min_x = Math.min(min_x, x); max_x = Math.max(max_x, x);
      min_y = Math.min(min_y, y); max_y = Math.max(max_y, y);
    }
  }
  if (min_x === Infinity) return out;
  const span_x = Math.max(1, max_x - min_x), span_y = Math.max(1, max_y - min_y);
  for (let gy = 0; gy < grid; ++gy) {
    for (let gx = 0; gx < grid; ++gx) {
      const sx = min_x + ((gx + 0.5) / grid) * span_x;
      const sy = min_y + ((gy + 0.5) / grid) * span_y;
      if (raster.id[Math.floor(sy) * raster.width + Math.floor(sx)] >= 0) out[gy * grid + gx] = 1;
    }
  }
  return out;
}

function silhouette_iou(a, b) {
  let inter = 0, union = 0;
  for (let i = 0; i < a.length; ++i) {
    if (a[i] && b[i]) ++inter;
    if (a[i] || b[i]) ++union;
  }
  return union > 0 ? inter / union : 0;
}

/**
 * One model's legibility report (s3.3). `failures` lists the violations; an empty list passes.
 */
export function audit_legibility(root, name, triangles) {
  const raster = raster_plan(root, HOME_PX_PER_M);
  const report = { name, ok: true, lines: [] };
  const fail = (line) => { report.ok = false; report.lines.push(line); };
  if (!raster) {
    fail('  nothing static to audit');
    return report;
  }
  const is_ship = SHIP_NAMES.has(name);
  const budget = is_ship ? BOLD_TRI_BUDGET : STRUCTURE_TRI_BUDGET;

  // The report's header, in the plan's own format.
  const span_m = (raster.bbox[2] - raster.bbox[0]);
  report.lines.push(
    `${name}   ${span_m.toFixed(1)} m   ${Math.round(span_m * HOME_PX_PER_M)} px at home framing`);

  const tri_ok = triangles <= budget;
  report.lines.push(`  triangles        ${triangles}${tri_ok ? ' ok' : ' FAIL'}   (budget ${budget})`);
  if (!tri_ok) fail(`  triangles ${triangles} over the ${budget} budget`);

  const feature_ok = raster.smallest_feature >= MIN_FEATURE_M;
  report.lines.push(`  smallest feature ${raster.smallest_feature.toFixed(2)} m = ` +
    `${(raster.smallest_feature * HOME_PX_PER_M).toFixed(1)} px${feature_ok ? ' ok' : ' FAIL'}   (min ${MIN_FEATURE_M} m)`);
  if (!feature_ok) {
    fail(`  smallest feature ${raster.smallest_feature.toFixed(2)} m is under the ${MIN_FEATURE_M} m floor`);
  }

  report.lines.push('  material regions, plan-view area:');
  for (const region of material_areas(raster)) {
    const px2 = region.area_m2 * HOME_PX_PER_M * HOME_PX_PER_M;
    const ok = region.area_m2 >= MIN_REGION_M2 || region.pixels === 0;
    report.lines.push(`    ${region.name.padEnd(14)} ${px2.toFixed(0).padStart(5)} px2  ` +
      `${((region.area_m2 / Math.max(1, material_area_total(raster))) * 100).toFixed(0).padStart(2)}%` +
      `${ok ? '' : '   FAIL      below 54 px2 minimum'}`);
    if (!ok) fail(`  material ${region.name} is ${region.area_m2.toFixed(1)} m2, under the ${MIN_REGION_M2} m2 minimum`);
  }

  const shape = silhouette_complexity(raster);
  const complex_ok = shape.complexity >= MIN_COMPLEXITY;
  report.lines.push(`  complexity       ${shape.complexity.toFixed(1)}` +
    `${complex_ok ? ' ok' : ' FAIL'}   (perimeter^2/area >= ${MIN_COMPLEXITY})`);
  if (!complex_ok) {
    fail(`  silhouette complexity ${shape.complexity.toFixed(1)} is under ${MIN_COMPLEXITY}: the plan outline reads as a blob`);
  }

  report.raster = raster;
  return report;
}

function material_area_total(raster) {
  let total = 0;
  for (let i = 0; i < raster.id.length; ++i) if (raster.id[i] >= 0) ++total;
  return total / (raster.px_per_m * raster.px_per_m);
}

/**
 * s3.4's distinctness gate across the ships: pairwise IoU of the normalised plan silhouettes, all
 * to be below 0.70. This is the check no single-reference pass can make - it sees the ships
 * together.
 */
export function audit_silhouettes(reports) {
  const lines = [];
  let ok = true;
  const ships = reports.filter((report) => SHIP_NAMES.has(report.name) && report.raster);
  const masks = ships.map((report) => ({ name: report.name, mask: normalised_silhouette(report.raster) }));
  for (let i = 0; i < masks.length; ++i) {
    for (let j = i + 1; j < masks.length; ++j) {
      const iou = silhouette_iou(masks[i].mask, masks[j].mask);
      const pass = iou < MAX_SILHOUETTE_IOU;
      ok = ok && pass;
      lines.push(`  IoU ${masks[i].name}/${masks[j].name}   ${iou.toFixed(2)}${pass ? ' ok' : ' FAIL   (max 0.70)'}`);
    }
  }
  return { ok, lines };
}
