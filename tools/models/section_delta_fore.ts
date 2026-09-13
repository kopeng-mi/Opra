// section_delta_fore: Variant C forward hull, 4 m axial, section_weapons.
//
// The delta opens here: 2.6 m at the prow joint to 4.7 m across the chines. The chines are not
// decoration and not aerodynamics - they are the hard edge where the dorsal and ventral facets
// meet, which is the whole low-observable trick, and they carry the forward RCS and the chine
// lights because that edge is the furthest structure from the centreline.
//
// The bridge canopy sits flush in the deck rather than standing proud. A blister is a corner
// reflector; a flush hatch with a slit is not.
import * as THREE from 'three';
import {
  black, dark, glass, glow, lightArmor, metal, ochre,
  box, flange, standard_maps, type Maps,
} from './prims';

export function build(): THREE.Object3D {
  const root = new THREE.Group();
  root.name = 'section_delta_fore';

  flange(root, metal, [0, 0, 0], [0, -1, 0]).name = 'flange';

  // Forward hull: flat, wide, low. 1.45 m of depth against 3.2 m of beam.
  box(root, lightArmor, [3.2, 3.94, 1.45], [0, 2.0, 0]);
  // Razor chines, swept out as they run aft.
  for (const sx of [-1, 1]) box(root, dark, [1.0, 3.8, 0.5], [sx * 1.85, 2.0, -0.05], -sx * 0.15);

  // Flush faceted bridge canopy and its viewport slit.
  box(root, black, [1.32, 1.9, 0.5], [0, 2.85, 0.88]);
  box(root, glass, [0.96, 0.46, 0.24], [0, 3.32, 1.0]);

  // Dorsal deck: armour step at the aft bulkhead, heat tiles either side of the canopy, and the two
  // spine rails that carry the tie-down and umbilical runs.
  box(root, dark, [2.6, 0.56, 0.3], [0, 0.42, 0.8]);
  for (const sx of [-1, 1]) box(root, black, [0.92, 1.35, 0.26], [sx * 1.0, 1.3, 0.79]);
  for (const sx of [-1, 1]) box(root, metal, [0.52, 2.0, 0.38], [sx * 1.05, 2.2, 0.78]);

  // Ventral avionics bays and the forward RCS rails on the chine tips.
  for (const sx of [-1, 1]) box(root, dark, [0.74, 1.6, 0.54], [sx * 1.1, 1.0, -0.78]);
  for (const sx of [-1, 1]) box(root, metal, [1.3, 0.48, 0.48], [sx * 0.9, 3.7, -0.56]);
  for (const sx of [-1, 1]) box(root, glow, [0.18, 0.18, 0.18], [sx * 1.78, 3.45, 0.28]);
  box(root, ochre, [0.14, 1.15, 0.56], [1.63, 1.4, 0.1]);

  return root;
}

export const maps: Maps = standard_maps({
  seed: 712, panel: 0.22, rivets: 0, seams: 1, wear: 0.02, grime: 0.03,
  stencils: [['BRG', 0.12, 0.2]],
});

export const meta = {
  name: 'section_delta_fore', kind: 'section_weapons', span: 1, axial: true, modular: true,
  flange: { pos: [0, 0, 0], normal: [0, -1, 0] },
  mass: 4.5, thrust: 0, propellant: 0, cooling: 0, heat_capacity: 9,
  rcs_jets: 0, rcs_authority: 0,
  scale: 1, collider: 'auto', untextured: ['glass', 'glow'],
};
