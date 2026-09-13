// section_radiator_wing: Variant A reactor/thermal section, 4 m axial, section_utility.
//
// The shoulders. Reactor below deck, and the waste heat has to leave on panels big enough to do it:
// 5.8 m tip to tip, the widest station on the ship and the beat the silhouette is built around.
//
// Two deviations from the prototype, both deliberate and both measured:
//
//  - The vanes stood vertical there (0.08 m thick in X). At the 32 degree game camera and in the
//    plan raster that is a hairline: the widest station on the ship contributed 0.3 m2 and no
//    silhouette at all. They are canted plates here, 17 degrees off the play plane about Y.
//  - They stand clear of the hull on booms with a 0.5-0.6 m slot, which is how a deployed radiator
//    actually mounts: it has to radiate from both faces, so it cannot lie against warm plate. The
//    slot is also where the design's complexity score comes from. A panel welded to the flank adds
//    three sides of perimeter; a panel standing off adds four and takes area away.
import * as THREE from 'three';
import {
  black, copper, dark, glow, lightArmor, metal,
  bevelled, box, flange, standard_maps, strut, tube, type Maps,
} from '../../../tools/models/prims';

const CHINE = [0, Math.PI / 6, 0];
/** Panel centre. Plan half-width is 0.53 m after the cant, so the tips land on 2.95 m: 5.9 m span. */
const PANEL_X = 2.42;

export function build(): THREE.Object3D {
  const root = new THREE.Group();
  root.name = 'section_radiator_wing';

  flange(root, metal, [0, 0, 0], [0, -1, 0]).name = 'flange';

  // Reactor hull. The narrowest pressure section on the ship: it is mostly shield mass, and the
  // hull necks down here before the thrust frame opens back out.
  tube(root, lightArmor, 1.26, 1.4, 3.94, [0, 2.0, 0], 6, CHINE);
  tube(root, dark, 1.46, 1.46, 0.3, [0, 0.42, 0], 6, CHINE);

  for (const sx of [-1, 1]) {
    // Two booms per side, splayed fore and aft. One would let the panel flap under RCS torque.
    strut(root, metal, [sx * 1.3, 1.55, 0], [sx * 2.3, 1.55, -0.06], 0.17, 4);
    strut(root, metal, [sx * 1.3, 3.65, 0], [sx * 2.3, 3.65, -0.06], 0.17, 4);
    // Radiator panel. bevelled() with a sub-0.5 m radius degrades to a plain box (prims.ts:915), so
    // this costs 12 triangles like box() - it is used only because box() rotates about Z alone and
    // the cant is about Y.
    bevelled(root, dark, [1.16, 2.85, 0.26], [sx * PANEL_X, 2.68, -0.06], [0, -sx * 0.3, 0], { radius: 0.09 });
    // Coolant headers crossing the slot, supply forward and return aft, outboard where a suited
    // crew can isolate them.
    strut(root, copper, [sx * 1.3, 1.42, 0.42], [sx * 2.5, 1.42, 0.16], 0.12, 4);
    strut(root, copper, [sx * 1.3, 3.78, 0.42], [sx * 2.5, 3.78, 0.16], 0.12, 4);
    // Wingtip RCS quad and strobe, on the panel root spar at the longest lever arm aft of the COM.
    box(root, metal, [0.52, 0.52, 0.52], [sx * 2.55, 3.5, 0.16]);
    tube(root, black, 0.16, 0.2, 0.32, [sx * 2.8, 3.5, 0.16], 4, [0, 0, Math.PI / 2]);
    box(root, glow, [0.2, 0.2, 0.2], [sx * 2.55, 3.5, 0.48]);
  }

  // Dorsal heat-exchanger trunk: where the two loops cross the hull to reach both panels.
  box(root, metal, [1.2, 3.4, 0.36], [0, 2.0, 1.18]);
  // Reactor shield collar at the aft bulkhead, stepping out into the thrust frame.
  box(root, metal, [2.6, 0.5, 1.9], [0, 0.28, 0]);

  return root;
}

export const maps: Maps = standard_maps({
  seed: 704, panel: 0.2, rivets: 0.05, seams: 2, wear: 0.07, grime: 0.06,
  stencils: [['RAD-P', 0.12, 0.3], ['RAD-S', 0.62, 0.3]],
});

export const meta = {
  name: 'section_radiator_wing', kind: 'section_utility', span: 1, axial: true, modular: true,
  exempt: ['top_material'],
  flange: { pos: [0, 0, 0], normal: [0, -1, 0] },
  mass: 4.0, thrust: 0, propellant: 0, cooling: 0.020, heat_capacity: 3,
  rcs_jets: 0, rcs_authority: 0,
  scale: 1, collider: 'auto', untextured: ['glow'],
};
