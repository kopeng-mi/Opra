// Hull truss module (plan-04 §2.5, ref 17): the truss variant of the 4.0 m / 2.5 m flange
// family. An open lattice — four longerons, batten rings and one diagonal per bay per face —
// with NOTHING between the flanges but structure. The spine a modular ship grows on.
//
// Axis +Y, origin at the module centre.
import * as THREE from 'three';
import {
  dark, frame, metal,
  box, each_tile, flange, standard_maps, stencil_text, truss_box, tube, type Maps,
} from './prims';

/** Family envelope: 4.0 m flange to flange, 2.5 m across. */
const HALF = 2.0;

export function build(): THREE.Object3D {
  const group = new THREE.Group(); group.name = 'hull_truss';

  // Open lattice: four longerons on a 1.5 m section, six bays, one diagonal per bay per face.
  // `truss_box` is the same primitive the stations are built from, so the module reads as
  // part of the same built world.
  truss_box(group, frame, HALF * 2 - 0.2, 1.5, 1.5, 6, 0.09);
  // Gusset plates tying the longeron ends into the flange faces: the load path, made visible.
  for (const y of [-HALF + 0.2, HALF - 0.2])
    for (const sx of [-1, 1]) for (const sz of [-1, 1])
      box(group, metal, [0.22, 0.34, 0.22], [sx * 0.75, y, sz * 0.75]);
  // Midships hardpoint ring: a batten band with four lug blocks, where a module's outboard
  // stores bolt on without touching the lattice.
  tube(group, dark, 1.0, 1.0, 0.16, [0, 0, 0], 4);
  for (const sx of [-1, 1]) for (const sz of [-1, 1])
    box(group, metal, [0.2, 0.24, 0.2], [sx * 0.85, 0, sz * 0.85]);

  // The interface, both ends, one part.
  flange(group, metal, [0, -HALF, 0], [0, -1, 0]);
  flange(group, metal, [0, HALF, 0], [0, 1, 0]);
  return group;
}

/** A truss is all edges: coarse panels, honest weld seams, stencil on the longerons. */
export const maps: Maps = (() => {
  const kit = standard_maps({
    seed: 37, panel: 0.1, rivets: 0.05, seams: 3, wear: 0.36, grime: 0.3, scorch: 1,
    stencils: [['TRS-4', 0.1, 0.17]],
  });
  return {
    normal: kit.normal,
    roughness: kit.roughness,
    albedo: (ctx, size) => {
      kit.albedo?.(ctx, size);
      each_tile(ctx, size, (tile) => {
        stencil_text(tile, 'LIFT', { x: 0.5, y: 0.4, scale: 0.024 });
      });
    },
  };
})();

export const meta = { name: 'hull_truss', scale: 1, collider: 'auto' };
