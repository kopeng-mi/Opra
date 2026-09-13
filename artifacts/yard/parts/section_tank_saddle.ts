// section_tank_saddle: Variant A propellant section, 4 m axial, section_tank.
//
// The narrow waist. Cutting the pressure hull to 2.6 m here is what makes the hammerhead forward
// and the radiator shoulders aft read as flares rather than as the same width twice, and the
// propellant that came out of the hull goes back on as conformal saddle tanks clamped to the
// flanks - external, so a tank rupture vents to space instead of into the crew volume.
import * as THREE from 'three';
import {
  copper, dark, glow, lightArmor, metal, ochre,
  box, flange, standard_maps, strut, tube, type Maps,
} from '../../../tools/models/prims';

const CHINE = [0, Math.PI / 6, 0];

export function build(): THREE.Object3D {
  const root = new THREE.Group();
  root.name = 'section_tank_saddle';

  flange(root, metal, [0, 0, 0], [0, -1, 0]).name = 'flange';

  // Waist pressure hull: 3.1 m across the chines, the narrowest station on the ship.
  tube(root, lightArmor, 1.26, 1.34, 3.94, [0, 2.0, 0], 6, CHINE);
  tube(root, dark, 1.4, 1.4, 0.3, [0, 0.5, 0], 6, CHINE);
  tube(root, dark, 1.34, 1.34, 0.3, [0, 3.5, 0], 6, CHINE);

  for (const sx of [-1, 1]) {
    // Conformal saddle tank, 8-sided: hugging the chine, not dangling on a pylon.
    tube(root, metal, 0.6, 0.6, 3.4, [sx * 1.2, 2.0, 0], 8);
    // Clamp bands, fore and aft, bolted through the hull frames.
    box(root, dark, [1.3, 0.34, 1.3], [sx * 1.2, 0.78, 0]);
    box(root, dark, [1.3, 0.34, 1.3], [sx * 1.2, 3.22, 0]);
    // Tank cradles at each bulkhead: the load path from tank to frame.
    box(root, metal, [1.3, 0.52, 0.5], [sx * 1.0, 0.5, 0]);
    box(root, metal, [1.3, 0.52, 0.5], [sx * 1.0, 3.5, 0]);
    // Cryogenic transfer line, outboard of the tank where it can be reached from a suit.
    strut(root, copper, [sx * 1.68, 0.55, 0.38], [sx * 1.68, 3.45, 0.38], 0.13, 4);
  }

  // Dorsal cable trunk running the station, and the crew airlock that sits on it.
  box(root, metal, [1.15, 3.3, 0.34], [0, 2.0, 1.2]);
  tube(root, dark, 0.58, 0.58, 0.36, [0, 2.2, 1.44], 6, [Math.PI / 2, 0, 0]);
  box(root, ochre, [0.94, 0.94, 0.14], [0, 2.2, 1.66]);
  box(root, glow, [0.2, 0.2, 0.2], [0, 0.75, 1.4]);        // airlock cycle lamp

  return root;
}

export const maps: Maps = standard_maps({
  seed: 703, panel: 0.18, rivets: 0.05, seams: 1, wear: 0.06, grime: 0.05,
  stencils: [['CRYO-LH2', 0.1, 0.18], ['LOCK-1', 0.56, 0.7]],
});

export const meta = {
  name: 'section_tank_saddle', kind: 'section_tank', span: 1, axial: true, modular: true, exempt: ['top_material'],
  flange: { pos: [0, 0, 0], normal: [0, -1, 0] },
  mass: 3.0, thrust: 0, propellant: 8.0, cooling: 0, heat_capacity: 4,
  rcs_jets: 0, rcs_authority: 0,
  scale: 1, collider: 'auto', untextured: ['glow'],
};
