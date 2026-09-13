import * as THREE from 'three';
const module = await import(new URL('file:///C:/Users/raman/Documents/MyGit/lostfleetdev/Opra/tools/models/surface_base.ts', import.meta.url).href);
const root = module.build();
root.updateWorldMatrix(true, true);
const rows = [];
root.traverse((node) => {
  if (!node.isMesh || node.userData.effect) return;
  const g = node.geometry;
  const count = (g.index ? g.index.count : g.attributes.position.count) / 3;
  rows.push([count, node.type, node.geometry.type, node.name || node.material?.name || '']);
});
rows.sort((a, b) => b[0] - a[0]);
let total = 0;
for (const r of rows) total += r[0];
console.log('total', total, 'parts', rows.length);
for (const r of rows.slice(0, 12)) console.log(r[0].toFixed(0), r[1], r[2], r[3]);
