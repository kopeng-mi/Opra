// drive_fusion_main: Fusion torch, 4 m axial. Faceted nozzle bell, thrust frame, RCS quads.
// Expanse logic: engine bell is a faceted cone (not smooth), radiator panels flanking,
// RCS thruster clusters at the widest point, glow only at the throat.
import * as THREE from 'three';
import {
  black, copper, dark, exhaust, glow, lightArmor, metal,
  box, effect_cone, flange, standard_maps, strut, tube, type Maps,
} from '../../../tools/models/prims';

export function build(): THREE.Object3D {
  const root = new THREE.Group();
  root.name = 'drive_fusion_main';
  const effects: THREE.Mesh[] = [];

  // Structural flange at fore end (connects to spine)
  flange(root, metal, [0, 0, 0], [0, -1, 0]);

  // Thrust frame — square structural cage around the engine
  const TF = 1.1; // thrust frame half-width
  for (const [x, z] of [[-TF, -TF], [TF, -TF], [TF, TF], [-TF, TF]] as const) {
    strut(root, metal, [x, 0.1, z], [x, 1.2, z], 0.1, 4);
  }
  // Horizontal thrust frame rings at two heights
  for (const y of [0.3, 1.0]) {
    for (let c = 0; c < 4; ++c) {
      const corners: [number, number][] = [[-TF, -TF], [TF, -TF], [TF, TF], [-TF, TF]];
      const p0 = corners[c], p1 = corners[(c + 1) % 4];
      strut(root, metal, [p0[0], y, p0[1]], [p1[0], y, p1[1]], 0.07, 4);
    }
  }

  // Engine bell — faceted cone (6 sides = hexagonal, industrial look)
  tube(root, dark, 0.55, 0.55, 0.3, [0, 1.2, 0], 6); // throat
  tube(root, lightArmor, 0.6, 1.4, 2.5, [0, 1.3, 0], 6); // expansion bell
  // Inner liner visible through the bell mouth
  tube(root, black, 0.5, 1.3, 2.4, [0, 1.35, 0], 6);

  // Throat glow — tiny, intense, at the narrowest point
  tube(root, glow, 0.48, 0.48, 0.08, [0, 1.25, 0], 6);

  // Nozzle exit lip — structural ring at bell mouth
  tube(root, metal, 1.42, 1.42, 0.12, [0, 3.8, 0], 6);

  // Radiator panels — 4 flat dark plates on strut arms (thermal management, extends width)
  for (const sx of [-1, 1]) {
    // Radiator arm strut
    strut(root, metal, [sx * 1.1, 1.8, 0], [sx * 2.8, 2.5, 0], 0.06, 4);
    // Radiator panel
    box(root, dark, [0.06, 2.0, 0.8], [sx * 2.8, 2.5, 0]);
    // Dorsal/ventral radiators
    strut(root, metal, [0, 1.8, sx * 1.0], [0, 2.5, sx * 2.2], 0.06, 4);
    box(root, dark, [0.6, 1.6, 0.06], [0, 2.5, sx * 2.2]);
  }

  // RCS thruster quads — 4 small nozzle clusters at the corners of the radiator frame
  for (const sx of [-1, 1]) {
    for (const sz of [-1, 1]) {
      box(root, metal, [0.15, 0.15, 0.15], [sx * 2.85, 3.2, sz * 0.5]);
      box(root, dark, [0.1, 0.2, 0.1], [sx * 2.85, 3.2, sz * 0.5]);
    }
  }

  // Feed lines — copper piping from thrust frame to throat
  for (const angle of [0, Math.PI / 2, Math.PI, 3 * Math.PI / 2]) {
    const r = 0.7;
    strut(root, copper, [Math.cos(angle) * r, 0.3, Math.sin(angle) * r],
                        [Math.cos(angle) * 0.55, 1.2, Math.sin(angle) * 0.55], 0.04, 3);
  }

  // Exhaust plume
  effect_cone(root, effects, {
    name: 'flame', radius: 1.15, length: 7.0, pos: [0, 3.85, 0], material: exhaust,
  });

  return root;
}

export const maps: Maps = standard_maps({
  seed: 710, panel: 0.2, rivets: 0.05, seams: 1, wear: 0.06, grime: 0.04, scorch: 0.02,
  stencils: [['FUS-TORCH', 0.1, 0.18]],
});

export const meta = {
  name: 'drive_fusion_main', kind: 'drive', span: 1, axial: true,
  flange: { pos: [0, 0, 0], normal: [0, -1, 0] },
  mass: 5.0, thrust: 1200, propellant: 0, cooling: 0, heat_capacity: 30,
  scale: 1, collider: 'auto', untextured: ['glow'],
};
