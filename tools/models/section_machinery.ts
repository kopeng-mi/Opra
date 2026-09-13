// section_machinery: Variant B utility section, 4 m axial, section_utility.
//
// The widest station on the ship, 6.4 m. An octagonal machinery core - pumps, scrubbers, the
// secondary loop - with the radiator banks held well off it on lattice arms. The arms are short and
// the slot between core and bank is open, because a radiator lying against a warm machinery hull
// radiates into it; and because the open slot is most of where this design's complexity score comes
// from (PLAN-07 s0: parts standing proud with gaps between them).
//
// The banks are canted 7 degrees outboard, as approved: it stops the pair reading as one flat grey
// wing, and it gives the key light two different angles to catch.
import * as THREE from 'three';
import {
  copper, dark, glow, metal, ochre,
  box, flange, standard_maps, strut, tube, type Maps,
} from './prims';

/** Octagon roll that puts flats dorsal and ventral: a deck, not a ridge. */
const FLAT = [0, Math.PI / 8, 0];
const BANK_X = 2.68;

export function build(): THREE.Object3D {
  const root = new THREE.Group();
  root.name = 'section_machinery';

  flange(root, metal, [0, 0, 0], [0, -1, 0]).name = 'flange';

  // Machinery core, 3.33 m across the flats.
  tube(root, metal, 1.76, 1.8, 3.94, [0, 2.0, 0], 8, FLAT);

  for (const sx of [-1, 1]) {
    // Lattice arm plus two braces. The arm is deliberately short in Y so the slot stays open fore
    // and aft of it.
    box(root, metal, [1.45, 0.7, 0.62], [sx * 2.4, 2.0, 0]);
    strut(root, metal, [sx * 1.6, 0.95, 0.15], [sx * 2.55, 1.35, 0.02], 0.19, 4);
    strut(root, metal, [sx * 1.6, 3.05, 0.15], [sx * 2.55, 2.65, 0.02], 0.19, 4);
    // Radiator bank, canted outboard.
    box(root, dark, [1.05, 3.4, 0.28], [sx * BANK_X, 2.0, 0], sx * 0.12);
    // External high-flow cryo lines, supply aft and return forward, crossing the slot.
    strut(root, copper, [sx * 1.5, 0.62, 0.8], [sx * 3.0, 0.62, 0.2], 0.12, 4);
    strut(root, copper, [sx * 1.5, 3.38, 0.8], [sx * 3.0, 3.38, 0.2], 0.12, 4);
    // Bank-tip RCS quad and strobe.
    box(root, metal, [0.62, 0.62, 0.62], [sx * 2.95, 3.5, 0.3]);
    box(root, glow, [0.22, 0.22, 0.22], [sx * 2.95, 3.5, 0.68]);
    // Ventral coolant pump housings.
    box(root, dark, [1.15, 1.5, 0.75], [sx * 1.2, 0.75, -1.2]);
  }

  // Dorsal machinery trunk and the hatch that opens it.
  box(root, dark, [1.45, 3.4, 0.42], [0, 2.0, 1.48]);
  box(root, ochre, [1.05, 0.9, 0.16], [0, 1.0, 1.71]);

  return root;
}

export const maps: Maps = standard_maps({
  seed: 708, panel: 0.18, rivets: 0.06, seams: 2, wear: 0.12, grime: 0.1,
  stencils: [['LOOP-B', 0.12, 0.28], ['HOT', 0.62, 0.3]],
});

export const meta = {
  name: 'section_machinery', kind: 'section_utility', span: 1, axial: true, modular: true,
  exempt: ['top_material'],
  flange: { pos: [0, 0, 0], normal: [0, -1, 0] },
  mass: 6.5, thrust: 0, propellant: 0, cooling: 0.0075, heat_capacity: 6,
  rcs_jets: 0, rcs_authority: 0,
  scale: 1, collider: 'auto', untextured: ['glow'],
};
