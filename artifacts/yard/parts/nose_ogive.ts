// nose_ogive: Command/sensor prow, 4 m axial. Blunt faceted wedge, not aerodynamic.
// Expanse logic: no atmosphere = no need for a pointy nose. Flat sensor face,
// small bridge windows, antenna mast, navigation lights.
import * as THREE from 'three';
import {
  black, copper, dark, glass, glow, lightArmor, metal,
  box, flange, standard_maps, strut, tube, type Maps,
} from '../../../tools/models/prims';

export function build(): THREE.Object3D {
  const root = new THREE.Group();
  root.name = 'nose_ogive';

  // Flange at base (connects to spine)
  flange(root, metal, [0, 0, 0], [0, -1, 0]);

  // Base collar — structural transition from flange to hull
  tube(root, metal, 1.25, 1.3, 0.35, [0, 0.17, 0], 6);

  // Main hull — faceted truncated pyramid tapering from base to blunt top
  // Built as stacked hexagonal sections (faceted, not smooth)
  tube(root, lightArmor, 1.3, 1.2, 1.0, [0, 0.5, 0], 6);   // lower section
  tube(root, lightArmor, 1.2, 0.9, 1.2, [0, 1.5, 0], 6);   // mid taper
  tube(root, lightArmor, 0.9, 0.5, 1.0, [0, 2.7, 0], 6);   // upper taper

  // Blunt top cap — flat sensor face
  tube(root, dark, 0.5, 0.5, 0.2, [0, 3.3, 0], 6);

  // Sensor windows — flat dark panels recessed into forward face
  box(root, black, [0.4, 0.3, 0.06], [0, 2.2, 0.92]);   // main sensor aperture
  box(root, glass, [0.25, 0.15, 0.04], [0, 2.2, 0.94]); // window/lens behind it

  // Bridge windows — very small, recessed, two on the upper section
  for (const sx of [-1, 1]) {
    box(root, glass, [0.12, 0.08, 0.04], [sx * 0.35, 2.8, 0.52]);
  }

  // Antenna mast — small vertical rod on top
  strut(root, metal, [0, 3.4, 0], [0, 4.0, 0], 0.03, 3);
  box(root, copper, [0.06, 0.06, 0.06], [0, 4.0, 0]); // tip

  // Panel seam line — horizontal dark band where hull sections meet
  tube(root, dark, 1.22, 1.22, 0.06, [0, 1.5, 0], 6);

  // Navigation lights — pinpoint glow at extremities (port/starboard)
  box(root, glow, [0.04, 0.04, 0.04], [1.25, 0.8, 0]);  // starboard
  box(root, glow, [0.04, 0.04, 0.04], [-1.25, 0.8, 0]); // port

  // Small equipment box on one side (asymmetry — ECM or comms unit)
  box(root, dark, [0.2, 0.3, 0.15], [0.95, 1.8, 0.4]);
  box(root, copper, [0.06, 0.1, 0.06], [1.05, 1.8, 0.4]);

  return root;
}

export const maps: Maps = standard_maps({
  seed: 730, panel: 0.2, rivets: 0.05, seams: 1, wear: 0.06, grime: 0.04,
  stencils: [['AVIONICS', 0.12, 0.2]],
});

export const meta = {
  name: 'nose_ogive', kind: 'cap', span: 1, axial: true,
  flange: { pos: [0, 0, 0], normal: [0, -1, 0] },
  mass: 1.6, propellant: 0, thrust: 0, cooling: 0, heat_capacity: 20,
  scale: 1, collider: 'auto', untextured: ['glass', 'glow'],
};
