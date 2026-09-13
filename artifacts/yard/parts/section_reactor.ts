// section_reactor: Variant B reactor and fuel section, 4 m axial, section_tank.
//
// A heavy square hull with the fuel drum sitting half-proud on the dorsal deck rather than buried:
// a drum you can see is a drum you can inspect, and on a working hauler that matters more than a
// clean line. The manifold runs along its crown where a suited crew standing on the deck can reach
// every valve on it.
import * as THREE from 'three';
import {
  copper, dark, glow, lightArmor, metal, ochre,
  box, flange, standard_maps, strut, tube, type Maps,
} from '../../../tools/models/prims';

const FLAT = [0, Math.PI / 8, 0];

export function build(): THREE.Object3D {
  const root = new THREE.Group();
  root.name = 'section_reactor';

  flange(root, metal, [0, 0, 0], [0, -1, 0]).name = 'flange';

  // Hull box and the two bulkhead bands it is framed on.
  box(root, lightArmor, [3.2, 3.94, 2.6], [0, 2.0, 0]);
  box(root, dark, [3.3, 0.5, 2.7], [0, 0.4, 0]);
  box(root, dark, [3.3, 0.5, 2.7], [0, 3.6, 0]);

  // Fuel drum, banded, half-embedded in the deck.
  tube(root, metal, 1.15, 1.15, 3.5, [0, 2.0, 0.95], 8, FLAT);
  tube(root, dark, 1.24, 1.24, 0.3, [0, 1.05, 0.95], 8, FLAT);
  tube(root, dark, 1.24, 1.24, 0.3, [0, 2.95, 0.95], 8, FLAT);
  box(root, copper, [0.42, 3.3, 0.42], [0, 2.0, 2.04]);
  for (const sx of [-1, 1]) strut(root, metal, [sx * 0.82, 1.1, 1.82], [sx * 0.82, 2.9, 1.82], 0.14, 4);

  // Service raceways flanking the drum on the deck, hazard-marked reactor access on the flanks,
  // and the coolant pump housings underneath.
  for (const sx of [-1, 1]) box(root, metal, [0.6, 2.6, 0.6], [sx * 1.35, 2.0, 1.3]);
  for (const sx of [-1, 1]) box(root, ochre, [0.16, 1.35, 1.0], [sx * 1.66, 3.0, 0.2]);
  for (const sx of [-1, 1]) box(root, dark, [1.05, 1.35, 0.72], [sx * 1.05, 0.95, -1.5]);
  for (const sx of [-1, 1]) box(root, glow, [0.22, 0.22, 0.22], [sx * 1.66, 1.1, 0.9]);

  return root;
}

export const maps: Maps = standard_maps({
  seed: 709, panel: 0.16, rivets: 0.06, seams: 2, wear: 0.12, grime: 0.12, scorch: 1,
  stencils: [['RX-1', 0.12, 0.2], ['NO ENTRY', 0.58, 0.66]],
});

export const meta = {
  name: 'section_reactor', kind: 'section_tank', span: 1, axial: true, modular: true, exempt: ['top_material'],
  flange: { pos: [0, 0, 0], normal: [0, -1, 0] },
  mass: 9.0, thrust: 0, propellant: 10.0, cooling: 0, heat_capacity: 10,
  rcs_jets: 0, rcs_authority: 0,
  scale: 1, collider: 'auto', untextured: ['glow'],
};
