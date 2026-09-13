// drive_stealth_twin: Variant C propulsion module, 4 m axial, drive_fusion.
//
// Two nozzles sunk inside armoured shrouds. The shroud is not armour for its own sake - it blocks
// the line of sight to the hot throat from every direction except directly astern, which is the one
// direction a ship running from you cannot be seen from anyway. The exit planes sit level with the
// shroud mouth, not proud of it.
//
// The shrouds are separate boxes with a 1.3 m notch between them, closed forward by the vectoring
// tail plane and open aft. That notch is a real hole in the plan silhouette and is deliberate: a
// single 5.2 m transom is a rectangle, and a rectangle scores 21.3 (PLAN-07 s0).
import * as THREE from 'three';
import {
  black, copper, dark, exhaust, glow, lightArmor, metal, ochre,
  box, effect_cone, flange, standard_maps, strut, tube, type Maps,
} from './prims';

/** Shroud and nozzle centreline. Inner faces land on 0.65 m, so the stern notch is 1.3 m wide. */
const POD_X = 1.65;

export function build(): THREE.Object3D {
  const root = new THREE.Group();
  root.name = 'drive_stealth_twin';
  const effects: THREE.Mesh[] = [];

  flange(root, metal, [0, 0, 0], [0, -1, 0]).name = 'flange';

  // Thrust bulkhead: the full-width frame both shrouds hang off.
  box(root, lightArmor, [3.6, 0.84, 1.9], [0, 0.48, 0]);
  box(root, dark, [3.6, 0.52, 0.42], [0, 0.5, 1.0]);
  // Vectoring tail plane, in the notch and forward of it, so the aft 1.25 m stays open.
  box(root, metal, [0.36, 1.5, 1.3], [0, 1.9, 0.3]);

  for (const sx of [-1, 1]) {
    // Armoured shroud.
    box(root, dark, [2.0, 3.3, 2.0], [sx * POD_X, 2.25, 0]);
    // Recessed de Laval nozzle, exit plane flush with the shroud mouth. Throat stage bare, exit
    // stage coated - the same metal-to-dark gradient the other two drives carry.
    // Six sides, not eight: on this hull even the bell is faceted.
    tube(root, metal, 0.86, 0.7, 1.3, [sx * POD_X, 2.45, 0], 6);
    tube(root, dark, 1.02, 0.86, 0.86, [sx * POD_X, 3.47, 0], 6);
    tube(root, black, 1.1, 1.1, 0.2, [sx * POD_X, 3.85, 0], 6);
    tube(root, glow, 0.36, 0.36, 0.14, [sx * POD_X, 3.98, 0], 6);
    // Shroud crown: a bare structural deck where the yard lifts the nozzle out, with the
    // IR-suppression louvres set into its forward end.
    box(root, metal, [1.5, 1.9, 0.3], [sx * POD_X, 2.55, 1.06]);
    box(root, black, [1.32, 1.25, 0.3], [sx * POD_X, 1.1, 1.04]);
    // Shoulder RCS and the propellant feed.
    box(root, metal, [0.62, 0.62, 0.62], [sx * 2.4, 1.0, 0.86]);
    strut(root, copper, [sx * 2.3, 0.75, -0.6], [sx * 1.9, 1.9, -0.5], 0.11, 4);
    effect_cone(root, effects, {
      name: 'flame', radius: 0.88, length: 5.6, pos: [sx * POD_X, 3.96, 0],
      material: exhaust, flip: false,
    });
  }

  box(root, ochre, [0.12, 0.95, 0.52], [-2.66, 1.3, 0.2]);

  return root;
}

export const maps: Maps = standard_maps({
  seed: 715, panel: 0.2, rivets: 0, seams: 1, wear: 0.04, grime: 0.05, scorch: 2,
  stencils: [['NOZ-P', 0.12, 0.2], ['NOZ-S', 0.62, 0.2]],
});

export const meta = {
  name: 'drive_stealth_twin', kind: 'drive_fusion', span: 1, axial: true, modular: true,
  flange: { pos: [0, 0, 0], normal: [0, -1, 0] },
  mass: 8.5, thrust: 1200, propellant: 0, cooling: 0, heat_capacity: 10,
  rcs_jets: 2, rcs_authority: 0.02847,
  scale: 1, collider: 'auto', untextured: ['glow'],
};
