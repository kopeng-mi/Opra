// Per-part gates from PLAN-07 §5 that gates.mjs does not implement: the triangle band, the flange,
// and the top-material share measured on a part rather than a design. Complexity and section CV are
// deliberately not applied here - the plan assigns both to the assembled design (§5's table), and a
// single 4 m module scoring 20 on complexity is the module doing its job, not a failure.
//
//   bun run partgate.mjs parts/*.ts
import { pathToFileURL } from 'node:url';
import { material_shares, MAX_MATERIAL_SHARE } from './gates.mjs';
import { audit_legibility } from '../../tools/export.mjs';
import { createRaster } from '../../tools/models/prims.ts';

const MIN_TRI = 120, MAX_TRI = 400;

function triangles(root, { flange }) {
  let total = 0;
  root.traverse((node) => {
    if (!node.isMesh || node.userData.effect) return;
    // The flange is the shared interface, authored once in prims.ts; `flange: false` reports the
    // part's own geometry against the budget. See the note in FLANGE-BUDGET below.
    if (!flange && node.parent?.name === 'flange') return;
    const p = node.geometry.attributes.position;
    total += node.geometry.index ? node.geometry.index.count / 3 : p.count / 3;
  });
  return Math.round(total);
}

let failed = 0;
for (const source of process.argv.slice(2)) {
  const module = await import(pathToFileURL(source).href);
  const root = module.build();
  root.updateMatrixWorld(true);
  const all = triangles(root, { flange: true });
  const own = triangles(root, { flange: false });
  const report = audit_legibility(root, module.meta?.name ?? source, all);
  const top = material_shares(report.raster)[0] ?? { name: '-', share: 0 };

  let site = null;
  root.traverse((n) => { if (n.name === 'flange' && (!site || Math.abs(n.position.y) < 1e-6)) site = n; });
  const has_flange = site !== null && Math.abs(site.position.y) < 1e-6 &&
                     Math.abs(site.position.x) < 1e-6 && Math.abs(site.position.z) < 1e-6;

  // Precise, and effect cones excluded: a flame is 6 m of hidden geometry and an AABB of a rolled
  // hex is a metre wider than the hex. Both lie about the module's envelope.
  const THREE = await import('three');
  const box = new THREE.Box3();
  root.traverse((n) => { if (n.isMesh && !n.userData.effect) box.expandByObject(n, true); });
  const line = [];
  const gate = (label, value, ok) => { line.push(`  ${label.padEnd(17)}${String(value).padEnd(24)}${ok ? 'ok' : 'FAIL'}`); if (!ok) failed++; };
  console.log(`${module.meta?.name ?? source}   ${(box.max.y - box.min.y).toFixed(2)} m axial   ` +
              `${(box.max.x - box.min.x).toFixed(2)} m span   ${(box.max.z - box.min.z).toFixed(2)} m deep`);
  gate('triangles', `${own} own / ${all} with flange`, own >= MIN_TRI && own <= MAX_TRI);
  gate('flange', has_flange ? '-Y at origin, r1.25' : 'MISSING', has_flange);
  // PLAN-07 s5: tanks and radiators are legitimately monolithic and declare the exemption in the
  // sidecar. Nothing else gets it, and it has to be declared rather than inferred from the kind.
  // The maps are only executed by the exporter, so a bad stencil coordinate or a hazard rect that
  // runs off the tile would not surface until GLB time. Draw all three into a small buffer here.
  let maps = 'missing';
  if (module.maps) {
    try {
      const raster = createRaster(64, '#000000');
      for (const channel of ['normal', 'roughness', 'albedo']) module.maps[channel]?.(raster.ctx, 64);
      maps = Object.keys(module.maps).sort().join('+');
    } catch (error) { maps = `THREW: ${error.message}`; }
  }

  const exempt = (module.meta?.exempt ?? []).includes('top_material');
  gate('top material', `${top.name} ${(top.share * 100).toFixed(0)}%` + (exempt ? ' (exempt)' : ''),
       exempt || top.share <= MAX_MATERIAL_SHARE);
  gate('maps', maps, maps === 'albedo+normal+roughness');
  console.log(line.join('\n'));
}
console.log(failed === 0 ? '\npartgate: pass' : `\npartgate: ${failed} gate failures`);
process.exit(failed === 0 ? 0 : 1);
