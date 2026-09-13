import * as THREE from 'three';
const module = await import(new URL('file:///C:/Users/raman/Documents/MyGit/lostfleetdev/Opra/tools/models/surface_base.ts', import.meta.url).href);
const root = module.build();
root.updateWorldMatrix(true, true);
const by_type = new Map();
let total = 0, parts = 0;
root.traverse((node) => {
  if (!node.isMesh || node.userData.effect) return;
  const g = node.geometry;
  const count = Math.round((g.index ? g.index.count : g.attributes.position.count) / 3);
  total += count; parts += 1;
  const key = `${node.geometry.type}`;
  by_type.set(key, (by_type.get(key) ?? 0) + count);
});
console.log('total', total, 'parts', parts);
for (const [k, v] of [...by_type].sort((a, b) => b[1] - a[1])) console.log(String(v).padStart(7), k);
