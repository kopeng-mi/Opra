// tank_drum_1: Propellant tank, 4 m axial. Cylindrical pressure vessel with structural bands.
// Expanse logic: tanks are tanks — cylindrical, banded, with fill valves and a level sensor.
// No decorative ribs. Faceted (6-sided) to match the industrial hull language.
import * as THREE from 'three';
import {
  copper, dark, glow, lightArmor, metal,
  box, flange, standard_maps, tube, type Maps,
} from '../../../tools/models/prims';

export function build(): THREE.Object3D {
  const root = new THREE.Group();
  root.name = 'tank_drum_1';

  // Dual flanges — stackable
  flange(root, metal, [0, 0, 0], [0, -1, 0]).name = 'flange';
  flange(root, metal, [0, 4.0, 0], [0, 1, 0]).name = 'flange';

  // Main pressure vessel — faceted hexagonal cylinder
  tube(root, lightArmor, 1.35, 1.35, 3.4, [0, 2.0, 0], 6);

  // Structural end caps — slightly narrower transition rings
  tube(root, metal, 1.25, 1.35, 0.25, [0, 0.42, 0], 6); // fore cap
  tube(root, metal, 1.35, 1.25, 0.25, [0, 3.58, 0], 6); // aft cap

  // Structural banding rings — bolted around the tank at regular intervals
  for (const y of [1.0, 2.0, 3.0]) {
    tube(root, dark, 1.38, 1.38, 0.15, [0, y, 0], 6);
  }

  // Fill/drain manifold cluster — one side only (asymmetry)
  box(root, dark, [0.25, 0.6, 0.2], [-1.42, 1.5, 0]);
  box(root, copper, [0.1, 0.35, 0.1], [-1.48, 1.5, 0]); // valve
  box(root, copper, [0.08, 0.08, 0.08], [-1.48, 1.85, 0]); // connector

  // Level sensor strip — thin vertical line on opposite side
  box(root, dark, [0.04, 2.4, 0.04], [1.38, 2.0, 0]);
  box(root, glow, [0.02, 1.8, 0.02], [1.40, 2.0, 0]); // tiny indicator

  // Hull panel seam lines implied by the material joints (dark bands do this)

  return root;
}

export const maps: Maps = standard_maps({
  seed: 720, panel: 0.18, rivets: 0.05, seams: 1, wear: 0.06, grime: 0.04,
  stencils: [['CRYO-LH2', 0.1, 0.18]],
});

export const meta = {
  name: 'tank_drum_1', kind: 'tank', span: 1, axial: true, modular: true, exempt: ['top_material'],
  flange: { pos: [0, 0, 0], normal: [0, -1, 0] },
  mass: 1.5, propellant: 5.0, thrust: 0, cooling: 0, heat_capacity: 3,
  rcs_jets: 0, rcs_authority: 0,
  scale: 1, collider: 'auto', untextured: ['glow'],
};
