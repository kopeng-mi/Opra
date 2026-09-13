// section_stealth_combat: Variant C weapons section, 4 m axial, section_weapons.
//
// Everything that shoots is flush. Four missile cells sit under hatches level with the deck plate,
// their actuator rails running between them; the PDC bays are conformal blisters faired into the
// chine with the barrels retracted. A protruding turret on this hull would undo the prow.
//
// This is the widest armoured body on the ship at 4.4 m, and the PDC bays carry it out to 5.6 m.
import * as THREE from 'three';
import {
  black, copper, dark, glow, lightArmor, metal, ochre,
  box, flange, standard_maps, tube, type Maps,
} from '../../../tools/models/prims';

/** Missile cell centres: two pairs, offset from the keel so the blast tubes clear the spine. */
const CELL_X = 1.0;

export function build(): THREE.Object3D {
  const root = new THREE.Group();
  root.name = 'section_stealth_combat';

  flange(root, metal, [0, 0, 0], [0, -1, 0]).name = 'flange';

  // Armoured body.
  box(root, lightArmor, [4.4, 3.94, 1.6], [0, 2.0, 0]);
  // Dorsal armour steps at each bulkhead: the frames the cells are hung between.
  box(root, dark, [2.24, 0.64, 0.3], [0, 0.42, 0.85]);
  box(root, dark, [2.24, 0.64, 0.3], [0, 3.6, 0.85]);

  // Flush missile cells: four hatches, each with the actuator pin that drives it.
  for (const sx of [-1, 1]) {
    box(root, dark, [1.0, 1.34, 0.18], [sx * CELL_X, 1.15, 0.86]);
    box(root, dark, [1.0, 1.34, 0.18], [sx * CELL_X, 2.95, 0.86]);
    box(root, copper, [0.32, 0.36, 0.26], [sx * 0.42, 1.15, 0.86]);
    box(root, copper, [0.32, 0.36, 0.26], [sx * 0.42, 2.95, 0.86]);
    // Conformal PDC bay, faired into the chine, barrel retracted flush with the fairing.
    box(root, dark, [0.94, 1.5, 0.44], [sx * 2.35, 2.0, 0.42]);
    tube(root, metal, 0.15, 0.15, 1.2, [sx * 2.35, 2.95, 0.42], 4);
    // Dorsal heat-sink louvres, ventral longerons, chine light, in that order outboard.
    box(root, black, [1.5, 1.0, 0.28], [sx * 1.32, 3.45, 0.84]);
    box(root, metal, [0.52, 2.4, 0.52], [sx * 2.0, 2.0, -0.72]);
    box(root, glow, [0.18, 0.18, 0.18], [sx * 2.62, 1.15, 0.48]);
  }

  // Port magazine access, hazard-marked. One side only - there is one magazine.
  box(root, ochre, [0.14, 1.05, 0.56], [-2.2, 3.2, 0.2]);

  return root;
}

export const maps: Maps = standard_maps({
  seed: 713, panel: 0.22, rivets: 0, seams: 2, wear: 0.02, grime: 0.04,
  stencils: [['VLS 1-4', 0.12, 0.2], ['PDC-S', 0.66, 0.5]],
});

export const meta = {
  name: 'section_stealth_combat', kind: 'section_weapons', span: 1, axial: true, modular: true,
  flange: { pos: [0, 0, 0], normal: [0, -1, 0] },
  mass: 6.0, thrust: 0, propellant: 0, cooling: 0, heat_capacity: 11,
  rcs_jets: 0, rcs_authority: 0,
  scale: 1, collider: 'auto', untextured: ['glow'],
};
