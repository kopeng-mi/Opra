// nose_pushbow: Variant B bow module, 4 m axial, nose_armored.
//
// This ship shoves things. The bow is a push-knee: a heavy plate on rams, backed by the hull box,
// with elastomer cushions on the contact face so a barge takes the load over its frames rather than
// through a corner. Nothing here is tapered - taper is for atmosphere, and a blunt plate is what a
// tug needs (TASTE-PROFILE s5).
//
// The work cab sits on top rather than inside because the operator has to see the contact patch.
import * as THREE from 'three';
import {
  black, copper, dark, glass, glow, lightArmor, metal, ochre,
  box, flange, standard_maps, strut, type Maps,
} from '../../../tools/models/prims';

export function build(): THREE.Object3D {
  const root = new THREE.Group();
  root.name = 'nose_pushbow';

  flange(root, metal, [0, 0, 0], [0, -1, 0]).name = 'flange';

  // Forward hull box and its bulkhead band at the flange.
  box(root, lightArmor, [3.3, 2.5, 2.4], [0, 1.3, 0]);
  box(root, dark, [3.35, 0.42, 2.45], [0, 0.32, 0]);

  // Push-knee: the plate, its hazard chevrons, and the cushions that actually touch.
  box(root, metal, [3.85, 1.55, 2.5], [0, 3.15, 0]);
  for (const sx of [-1, 1]) box(root, ochre, [0.95, 1.5, 0.22], [sx * 1.3, 3.15, 1.3]);
  for (const sx of [-1, 1]) box(root, black, [0.8, 0.6, 2.5], [sx * 1.3, 4.08, 0]);
  box(root, black, [1.5, 0.5, 1.1], [0, 4.08, 0]);
  // Rams, ventral, where the load path from plate to keel is shortest.
  for (const sx of [-1, 1]) strut(root, metal, [sx * 1.15, 2.3, -0.85], [sx * 1.15, 3.5, -0.85], 0.2, 6);
  for (const sx of [-1, 1]) strut(root, copper, [sx * 0.6, 2.4, 1.05], [sx * 0.6, 3.4, 1.05], 0.11, 4);

  // Elevated work cab on the dorsal deck: the operator needs eyes on the contact patch.
  box(root, dark, [1.6, 1.5, 0.55], [0, 1.2, 1.45]);
  box(root, dark, [1.95, 1.55, 1.0], [0, 2.05, 1.7]);
  box(root, glass, [1.55, 0.45, 0.38], [0, 2.8, 1.9]);
  // Floodlights on the cab shoulders, aimed at the plate.
  for (const sx of [-1, 1]) box(root, glow, [0.3, 0.24, 0.24], [sx * 1.5, 3.4, 1.05]);

  // Flank handling rails and ventral grapple fittings: this is a working bow, and everything on it
  // is something a crew or a crane grabs.
  for (const sx of [-1, 1]) box(root, metal, [0.55, 2.3, 0.6], [sx * 1.72, 1.3, 0.9]);
  for (const sx of [-1, 1]) box(root, dark, [0.95, 1.6, 0.65], [sx * 1.15, 1.5, -1.35]);

  return root;
}

export const maps: Maps = standard_maps({
  seed: 706, panel: 0.16, rivets: 0.07, seams: 2, wear: 0.12, grime: 0.1, scorch: 1,
  stencils: [['MAX 400 t', 0.1, 0.2], ['CAB-1', 0.6, 0.68]],
});

export const meta = {
  name: 'nose_pushbow', kind: 'nose_armored', span: 1, axial: true, modular: true,
  flange: { pos: [0, 0, 0], normal: [0, -1, 0] },
  mass: 12.0, thrust: 0, propellant: 0, cooling: 0, heat_capacity: 20,
  rcs_jets: 2, rcs_authority: 0.00789,
  scale: 1, collider: 'auto', untextured: ['glass', 'glow'],
};
