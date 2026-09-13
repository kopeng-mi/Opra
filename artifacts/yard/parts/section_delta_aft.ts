// section_delta_aft: Variant C sponson section, 4 m axial, section_utility.
//
// The widest station on the ship: 7.0 m tip to tip. The delta wing extensions are the ship's heat
// rejection - the grilles on them vent the internal sinks - so they hang off the body on two short
// carry-through pylons with a 0.5 m slot open between wing and hull. A wing welded to a warm flank
// would be dumping its heat back into the hull it is trying to cool, and the slot is also where
// this design's silhouette stops being a solid arrowhead and starts having something to read.
import * as THREE from 'three';
import {
  black, copper, dark, glow, metal, ochre,
  box, flange, standard_maps, strut, type Maps,
} from '../../../tools/models/prims';

/**
 * Wing centreline. The sweep makes the slot a wedge rather than a parallel gap: the wing's inner
 * edge runs from 1.68 m at the aft corner out to 2.30 m at the fore corner, against a 1.8 m body
 * half-beam. It tucks under aft and opens to half a metre forward, which is what the raster sees.
 */
const WING_X = 2.6;

export function build(): THREE.Object3D {
  const root = new THREE.Group();
  root.name = 'section_delta_aft';

  flange(root, metal, [0, 0, 0], [0, -1, 0]).name = 'flange';

  // Aft body. Dark, not hull plate: this whole section is the thermal end of the ship and carries
  // the emissive coat, which is also what stops Variant C's plan view being one grey.
  box(root, dark, [3.6, 3.94, 1.75], [0, 2.0, 0]);
  // Dorsal heat-shield deck and the exhaust louvres set into it.
  box(root, metal, [2.4, 3.4, 0.34], [0, 2.0, 0.95]);
  for (const sx of [-1, 1]) box(root, black, [1.02, 1.25, 0.4], [sx * 1.05, 0.72, 0.98]);

  for (const sx of [-1, 1]) {
    // Two carry-through pylons per side. The gaps between and outboard of them are the slot.
    box(root, metal, [0.82, 0.62, 0.56], [sx * 2.08, 0.9, 0]);
    box(root, metal, [0.82, 0.62, 0.56], [sx * 2.08, 3.1, 0]);
    // Delta wing extension, swept.
    box(root, dark, [1.22, 3.8, 0.92], [sx * WING_X, 2.0, -0.05], -sx * 0.22);
    // Radiator vent grilles on the wing's dorsal face, and the wingtip beacon.
    box(root, black, [0.98, 2.2, 0.34], [sx * WING_X, 1.5, 0.44]);
    box(root, glow, [0.2, 0.2, 0.2], [sx * 3.0, 0.62, 0.18]);
    // Ventral coolant runs and longerons.
    strut(root, copper, [sx * 1.45, 0.6, -0.72], [sx * 1.45, 3.4, -0.72], 0.12, 4);
    box(root, metal, [0.56, 2.2, 0.56], [sx * 1.55, 2.0, -0.8]);
  }

  box(root, ochre, [0.14, 1.2, 0.52], [1.82, 1.2, 0.1]);

  return root;
}

export const maps: Maps = standard_maps({
  seed: 714, panel: 0.2, rivets: 0, seams: 2, wear: 0.03, grime: 0.05,
  stencils: [['HEAT', 0.14, 0.3], ['HEAT', 0.64, 0.3]],
});

export const meta = {
  name: 'section_delta_aft', kind: 'section_utility', span: 1, axial: true, modular: true,
  exempt: ['top_material'],
  flange: { pos: [0, 0, 0], normal: [0, -1, 0] },
  mass: 4.0, thrust: 0, propellant: 0, cooling: 0.025, heat_capacity: 6,
  rcs_jets: 0, rcs_authority: 0,
  scale: 1, collider: 'auto', untextured: ['glow'],
};
