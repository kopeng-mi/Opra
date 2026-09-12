// RCS quad (plan-04 §2.4, ref 12): a 1.3 m casting with four nozzles in a cross in ONE plane —
// the gameplay plane (X/Y) — which is exactly right for a 2D plane. The -Z face is the mount;
// the bells fire ±X and ±Y, so one block gives the simulation pitch, yaw and roll authority.
//
// The bells are `jet_nozzle`'s, each with the effect cone it owns: those cones are invisible
// and flagged `userData.effect`, so the exporter carries them to the sidecar as jet.0..jet.3.
import * as THREE from 'three';
import {
  bevelled, box, copper, dark, each_tile, hazard_stripes, jet_nozzle, lightArmor, metal,
  scorch, standard_maps, stencil_text, tube, type Maps,
} from './prims';

/** The four in-plane axes, starboard first, so jet.0..jet.3 walk the cross. */
const CROSS: { pos: number[]; dir: number[] }[] = [
  { pos: [0.65, 0, 0], dir: [1, 0, 0] },
  { pos: [0, 0.65, 0], dir: [0, 1, 0] },
  { pos: [-0.65, 0, 0], dir: [-1, 0, 0] },
  { pos: [0, -0.65, 0], dir: [0, -1, 0] },
];

export function build(): THREE.Object3D {
  const group = new THREE.Group(); group.name = 'rcs_block';
  const effects: THREE.Mesh[] = [];
  // Housing: one 1.3 m bevelled casting. A raw box edge catches a highlight like a prop, and on
  // a fighter this block sits close enough to camera that the edge is what a pilot sees.
  bevelled(group, metal, [1.3, 1.3, 0.9], [0, 0, 0], [0, 0, 0], { radius: 0.18, segments: 1 });
  // Mounting face: a gasket plate and four pads, the only surfaces that touch the hull.
  box(group, dark, [1.1, 1.1, 0.08], [0, 0, -0.48]);
  for (const qx of [-1, 1]) for (const qy of [-1, 1])
    box(group, lightArmor, [0.34, 0.34, 0.18], [qx * 0.37, qy * 0.37, -0.56]);
  // Propellant inlet on the dorsal edge with a harness clamp: a block that reads connected.
  tube(group, copper, 0.12, 0.12, 0.4, [0, 0.45, 0.45], 8);
  box(group, dark, [0.5, 0.18, 0.18], [0, 0.62, 0.35]);
  // Corner plumbing: copper runs up the four vertical corners feeding the bell bases, with a
  // valve manifold on the aft face — the sheet's pipework, at this scale.
  for (const qx of [-1, 1]) for (const qy of [-1, 1])
    tube(group, copper, 0.05, 0.05, 0.8, [qx * 0.6, qy * 0.6, 0], 6);
  box(group, dark, [0.7, 0.3, 0.2], [0, 0, -0.62]);
  for (const qx of [-1, 1]) tube(group, metal, 0.07, 0.07, 0.2, [qx * 0.2, 0, -0.66], 6, [Math.PI / 2, 0, 0]);
  // Bells on the four faces, one per in-plane axis, each with its own hidden jet.
  CROSS.forEach(({ pos, dir }, index) => {
    jet_nozzle(group, effects, {
      name: `jet.${index}`,
      radius: 0.16,
      pos,
      direction: dir,
      length: 0.35,
    });
  });
  // Face hatches: one inset panel per free face, perimeter fasteners implied by the maps.
  box(group, lightArmor, [0.5, 0.5, 0.06], [0, 0, 0.47]);
  return group;
}

/** Close-pitch panels and rivets, a hazard band under the mounting face and a quad stencil. */
export const maps: Maps = (() => {
  const kit = standard_maps({
    seed: 61, panel: 0.3, rivets: 0.1, seams: 1, wear: 0.3, grime: 0.26, scorch: 1,
    stencils: [['RCS 04', 0.09, 0.14]],
  });
  return {
    normal: kit.normal,
    roughness: kit.roughness,
    albedo: (ctx, size) => {
      kit.albedo?.(ctx, size);
      each_tile(ctx, size, (tile) => {
        hazard_stripes(tile, { x: 0.06, y: 0.74, w: 0.88, h: 0.14 }, { pitch: 0.04, angle: 0 });
        stencil_text(tile, 'NO STEP', { x: 0.5, y: 0.42, scale: 0.024 });
      });
    },
  };
})();

export const meta = { name: 'rcs_block', scale: 1, collider: 'auto' };
