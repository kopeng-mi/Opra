// Hull cargo module (plan-04 §2.5, ref 17): the cargo variant of the 4.0 m / 2.5 m flange
// family. An open frame — longerons, end rings and battens — with a clamped container hung
// inside it, and the shared flange at BOTH ends.
//
// Axis +Y, origin at the module centre. This is the flight frame the Mule's pods clamp to;
// `cargo` stays the ground crate the scene drops.
import * as THREE from 'three';
import {
  dark, frame, hazardPaint, hullPaint, metal,
  box, each_tile, flange, standard_maps, stencil_text, strut, tube, type Maps,
} from './prims';

/** Family envelope: 4.0 m flange to flange, 2.5 m across. */
const HALF = 2.0;
const RADIUS = 1.25;

export function build(): THREE.Object3D {
  const group = new THREE.Group(); group.name = 'hull_cargo';

  // Open frame: four longerons on the flange diameter, end rings inside each flange face, and
  // a batten ring midships. The container hangs well clear of the longerons, so the frame reads
  // as structure you can see through — the point of the variant.
  for (const sx of [-1, 1]) for (const sz of [-1, 1])
    strut(group, frame, [sx * 1.0, -HALF + 0.1, sz * 1.0], [sx * 1.0, HALF - 0.1, sz * 1.0], 0.11, 6);
  for (const y of [-HALF + 0.15, 0, HALF - 0.15])
    tube(group, frame, 1.3, 1.3, 0.14, [0, y, 0], 4);
  // Corner gussets tying the longerons into the flange faces.
  for (const y of [-HALF + 0.3, HALF - 0.3])
    for (const sx of [-1, 1]) for (const sz of [-1, 1])
      box(group, metal, [0.24, 0.4, 0.24], [sx * 1.0, y, sz * 1.0]);

  // Clamped container: a slim corrugated box hung inside the frame on four clamp arms, with
  // corner castings standing proud — intermodal character at 2.5 m instead of 20. Slim enough
  // to leave real gaps to the longerons on all four faces.
  box(group, hullPaint, [1.2, 2.6, 1.2], [0, 0, 0]);
  for (const y of [-0.85, -0.42, 0, 0.42, 0.85]) {
    box(group, metal, [1.28, 0.09, 1.28], [0, y, 0]);
  }
  for (const sx of [-1, 1]) for (const sy of [-1, 1]) for (const sz of [-1, 1])
    box(group, dark, [0.24, 0.24, 0.24], [sx * 0.63, sy * 1.3, sz * 0.63]);
  // Clamp arms: frame longeron to container corner, with a turnbuckle body mid-arm.
  for (const y of [-1.0, 1.0]) for (const sx of [-1, 1]) for (const sz of [-1, 1]) {
    strut(group, metal, [sx * 1.0, y * 0.9, sz * 1.0], [sx * 0.58, y, sz * 0.58], 0.06, 5);
    box(group, dark, [0.16, 0.3, 0.16], [sx * 0.8, y * 0.95, sz * 0.8]);
  }
  // Placard: the hazard panel a loader reads, proud of the container flank.
  box(group, hazardPaint, [0.06, 0.7, 0.45], [0.64, 0.3, 0.3]);

  // The interface, both ends, one part.
  flange(group, metal, [0, -HALF, 0], [0, -1, 0]);
  flange(group, metal, [0, HALF, 0], [0, 1, 0]);
  return group;
}

/** Frame panels and the container's own decals. */
export const maps: Maps = (() => {
  const kit = standard_maps({
    seed: 29, panel: 0.14, rivets: 0.07, seams: 2, wear: 0.32, grime: 0.28, scorch: 1,
    stencils: [['CRG-7', 0.1, 0.17]],
  });
  return {
    normal: kit.normal,
    roughness: kit.roughness,
    albedo: (ctx, size) => {
      kit.albedo?.(ctx, size);
      each_tile(ctx, size, (tile) => {
        stencil_text(tile, 'MAX 5T', { x: 0.12, y: 0.44, scale: 0.024 });
      });
    },
  };
})();

export const meta = { name: 'hull_cargo', scale: 1, collider: 'auto' };
