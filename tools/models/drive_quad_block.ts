// drive_quad_block: Variant B propulsion module, 4 m axial, drive_fusion.
//
// Four bells on a square transom. An industrial hauler carries redundancy the way a truck carries
// spare axles: lose one and the block still pushes, off-centre, and the RCS trims it out. The 2x2
// grid is also what makes the stern read as this ship from astern and not as the corvette's pair.
//
// The confinement glow is amber, not the fleet cyan - a dirtier, hotter, cheaper plasma, and the
// colour is the tell. prims.ts carries one glow (cyan), so the amber comes from its own
// lamp_material() rather than a hand-authored material.
import * as THREE from 'three';
import {
  black, copper, dark, exhaust, lightArmor, metal, ochre,
  box, effect_cone, flange, lamp_material, standard_maps, strut, tube, type Maps,
} from './prims';

const amber = lamp_material('#f0922e');
amber.name = 'glowAmber';

/** The 2x2 grid: bell centres in (x, z). */
const BELLS: [number, number][] = [[-1.05, -0.78], [1.05, -0.78], [-1.05, 0.78], [1.05, 0.78]];

export function build(): THREE.Object3D {
  const root = new THREE.Group();
  root.name = 'drive_quad_block';
  const effects: THREE.Mesh[] = [];

  flange(root, metal, [0, 0, 0], [0, -1, 0]).name = 'flange';

  // Transition collar, then the transom the four mounts bolt through. Its forward half is hull
  // plate and its aft half is casting: four exhausts soak that end, and it is built for it.
  box(root, lightArmor, [3.4, 0.74, 2.7], [0, 0.43, 0]);
  box(root, lightArmor, [4.2, 1.15, 3.15], [0, 1.35, 0]);
  box(root, dark, [4.2, 1.3, 3.2], [0, 2.55, 0]);
  // Dorsal handling deck over the crown: where a yard crane lands to pull a bell.
  box(root, metal, [2.2, 1.9, 0.32], [0, 1.7, 1.62]);
  box(root, ochre, [1.25, 0.6, 0.16], [0, 1.75, 1.82]);
  for (const sx of [-1, 1]) strut(root, copper, [sx * 1.75, 0.7, 1.35], [sx * 1.15, 1.9, 1.0], 0.11, 4);

  for (const [bx, bz] of BELLS) {
    // Six-sided bells: pressed from plate, not spun. Cheaper to build and cheaper to replace, which
    // is the whole brief for this ship.
    tube(root, metal, 0.86, 0.56, 2.2, [bx, 2.95, bz], 6);
    tube(root, black, 0.94, 0.94, 0.2, [bx, 4.04, bz], 6);
    tube(root, amber, 0.34, 0.34, 0.12, [bx, 4.16, bz], 6);
    effect_cone(root, effects, {
      name: 'flame', radius: 0.8, length: 4.8, pos: [bx, 4.14, bz], material: exhaust, flip: false,
    });
  }

  return root;
}

export const maps: Maps = standard_maps({
  seed: 710, panel: 0.18, rivets: 0.06, seams: 1, wear: 0.14, grime: 0.1, scorch: 4,
  stencils: [['DRV 1-4', 0.1, 0.18]],
});

// B's drive is a four-bell block with its own radiator crown; carries cooling 0.0325.
export const meta = {
  name: 'drive_quad_block', kind: 'drive_fusion', span: 1, axial: true, modular: true,
  flange: { pos: [0, 0, 0], normal: [0, -1, 0] },
  mass: 16.5, thrust: 1950, propellant: 0, cooling: 0.0325, heat_capacity: 19,
  rcs_jets: 2, rcs_authority: 0.00789,
  scale: 1, collider: 'auto', untextured: ['glowAmber'],
};
