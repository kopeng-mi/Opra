// section_freight: Variant B cargo section, 4 m axial, section_cargo.
//
// A freight carriage: a narrow structural core with two standard containers latched to it on
// twist-lock stanchions, hazard-striped tie-down braces over each end. The containers are external
// because that is the whole point of a hauler - they come off, and they are not the same shape
// twice, which is also what breaks the ship's plan view into something readable.
import * as THREE from 'three';
import {
  copper, dark, glow, lightArmor, metal, ochre,
  box, flange, standard_maps, strut, type Maps,
} from '../../../tools/models/prims';

/** Container centreline. Inner face lands on 1.8 m against a 1.3 m core half-beam: a 0.5 m slot,
 *  bridged only by the four stanchions, which is what a twist-lock mount looks like from above. */
const POD_X = 2.45;

export function build(): THREE.Object3D {
  const root = new THREE.Group();
  root.name = 'section_freight';

  flange(root, metal, [0, 0, 0], [0, -1, 0]).name = 'flange';

  // Structural core: the load path from the bow to the drive runs through this box and nothing else.
  box(root, lightArmor, [2.6, 4.0, 2.45], [0, 2.0, 0]);

  for (const sx of [-1, 1]) {
    box(root, dark, [1.3, 3.5, 1.55], [sx * POD_X, 2.0, 0.1]);            // ISO container
    box(root, ochre, [1.42, 0.36, 1.68], [sx * POD_X, 0.78, 0.1]);         // tie-down brace, aft
    box(root, ochre, [1.42, 0.36, 1.68], [sx * POD_X, 3.22, 0.1]);         // tie-down brace, fore
    box(root, metal, [0.66, 0.72, 1.7], [sx * 1.55, 0.95, 0.1]);           // twist-lock, aft
    box(root, metal, [0.66, 0.72, 1.7], [sx * 1.55, 3.05, 0.1]);           // twist-lock, fore
    box(root, metal, [1.45, 0.5, 0.5], [sx * POD_X, 0.35, 0.95]);          // container end stop
    box(root, metal, [0.85, 1.5, 0.7], [sx * 0.95, 3.4, -1.35]);           // ventral service bay
    box(root, glow, [0.22, 0.22, 0.22], [sx * 1.32, 3.7, 1.62]);           // deck marker
    strut(root, copper, [sx * 1.4, 0.6, -1.0], [sx * 1.4, 3.4, -1.0], 0.11, 4);
  }

  // Dorsal cargo deck: grating over the core with a rail at each end, so a container can be walked
  // fore and aft during a transfer.
  box(root, metal, [1.9, 3.5, 0.36], [0, 2.0, 1.4]);
  box(root, dark, [2.6, 0.5, 0.5], [0, 0.42, 1.35]);
  box(root, dark, [2.6, 0.5, 0.5], [0, 3.58, 1.35]);

  return root;
}

export const maps: Maps = standard_maps({
  seed: 707, panel: 0.14, rivets: 0.07, seams: 2, wear: 0.14, grime: 0.12,
  stencils: [['ISO 1AA', 0.12, 0.22], ['NET 24.0 t', 0.6, 0.6]],
});

export const meta = {
  name: 'section_freight', kind: 'section_cargo', span: 1, axial: true, modular: true,
  flange: { pos: [0, 0, 0], normal: [0, -1, 0] },
  mass: 12.6, thrust: 0, propellant: 0, cooling: 0, heat_capacity: 12,
  rcs_jets: 0, rcs_authority: 0,
  scale: 1, collider: 'auto', untextured: ['glow'],
};
