// nose_stealth_needle: Variant C prow module, 4 m axial, nose_sensor.
//
// A low-RCS prow is a stack of flat facets that steps down in width, not a cone: every surface is a
// plane, every join is a chine, and nothing on it is normal to an incoming beam. The steps are what
// makes that read - three plates, each narrower and each rolled a little, so the return goes
// sideways instead of back.
//
// It is also the narrowest station on the ship by a long way. That is the point: 2.6 m here against
// 7.0 m at the sponsons is where the delta's flare comes from.
import * as THREE from 'three';
import {
  black, dark, glass, glow, lightArmor, metal, ochre,
  box, flange, standard_maps, type Maps,
} from './prims';

export function build(): THREE.Object3D {
  const root = new THREE.Group();
  root.name = 'nose_stealth_needle';

  flange(root, metal, [0, 0, 0], [0, -1, 0]).name = 'flange';

  // Root plate: the only light-armour on the module, and the width the delta hull mates to.
  box(root, lightArmor, [2.6, 1.3, 1.15], [0, 0.71, 0]);
  // Three faceted steps down to the edge. Radar-absorbent coat over armour, so: dark, then black.
  box(root, dark, [2.1, 1.3, 0.98], [0, 1.55, 0]);
  box(root, black, [1.55, 1.5, 0.82], [0, 2.85, 0]);
  box(root, black, [1.15, 1.7, 0.6], [0, 3.7, 0]);
  // Dorsal facet plates, canted off the deck line so the top is not one flat return either.
  box(root, dark, [1.9, 0.52, 0.4], [0, 1.0, 0.62]);
  box(root, black, [0.9, 1.8, 0.3], [0, 2.7, 0.56]);

  // Forward targeting optics, recessed behind a black shroud - an open aperture is the brightest
  // thing on a stealth hull, so it sits in a well.
  box(root, black, [0.72, 0.78, 0.42], [0, 2.5, 0.5]);
  box(root, glass, [0.52, 0.5, 0.24], [0, 2.58, 0.66]);

  // Chine roots: where the prow's edge picks up and runs aft into the delta section's chines.
  for (const sx of [-1, 1]) box(root, dark, [0.52, 2.1, 0.42], [sx * 0.95, 1.6, -0.2], -sx * 0.1);
  // Ventral keel fitting and the forward RCS rails.
  box(root, metal, [1.3, 0.6, 0.5], [0, 0.4, -0.6]);
  for (const sx of [-1, 1]) box(root, metal, [0.44, 1.2, 0.44], [sx * 0.55, 3.3, -0.3]);
  // Navigation pinpoints on the chine, and the starboard access strip (asymmetry).
  for (const sx of [-1, 1]) box(root, glow, [0.18, 0.18, 0.18], [sx * 0.72, 1.15, 0.44]);
  box(root, ochre, [0.14, 0.85, 0.42], [0.98, 1.5, 0]);

  return root;
}

export const maps: Maps = standard_maps({
  seed: 711, panel: 0.22, rivets: 0, seams: 1, wear: 0.02, grime: 0.03,
  stencils: [['S-01', 0.14, 0.24]],
});

export const meta = {
  name: 'nose_stealth_needle', kind: 'nose_sensor', span: 1, axial: true, modular: true,
  flange: { pos: [0, 0, 0], normal: [0, -1, 0] },
  mass: 2.5, thrust: 0, propellant: 0, cooling: 0, heat_capacity: 7,
  rcs_jets: 2, rcs_authority: 0.02847,
  scale: 1, collider: 'auto', untextured: ['glass', 'glow'],
};
