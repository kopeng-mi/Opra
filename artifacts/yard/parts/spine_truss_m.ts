// spine_truss_m: Structural backbone, 20 m. Exposed lattice truss, square cross-section.
// Expanse logic: open frame with diagonal bracing, conduit runs, flange hardpoints.
import * as THREE from 'three';
import {
  copper, dark, frame, glow, lightArmor, metal,
  box, flange, standard_maps, strut, tube, type Maps,
} from '../../../tools/models/prims';

const HALF_L = 10.0;
const HW = 1.5; // half-width of the square cross-section
const STATIONS = [8, 4, 0, -4, -8]; // 5 hardpoint stations

export function build(): THREE.Object3D {
  const root = new THREE.Group();
  root.name = 'spine_truss_m';

  // Square cross-section corners
  const C: [number, number][] = [
    [-HW, -HW], [HW, -HW], [HW, HW], [-HW, HW],
  ];

  // 4 primary longerons — the main structural rails running full length
  for (const [x, z] of C) {
    strut(root, metal, [x, -HALF_L, z], [x, HALF_L, z], 0.14, 4);
  }

  // Diagonal lattice bracing per bay — the key visual of an exposed truss
  for (let bay = 0; bay < STATIONS.length - 1; ++bay) {
    const y0 = STATIONS[bay], y1 = STATIONS[bay + 1];
    const flip = bay % 2 === 0;
    for (let c = 0; c < 4; ++c) {
      const p0 = C[c], p1 = C[(c + 1) % 4];
      // Horizontal ring member at each station
      strut(root, frame, [p0[0], y0, p0[1]], [p1[0], y0, p1[1]], 0.08, 4);
      // Diagonal brace alternating direction per bay
      const d0 = flip ? p0 : p1;
      const d1 = flip ? p1 : p0;
      strut(root, frame, [d0[0], y0, d0[1]], [d1[0], y1, d1[1]], 0.06, 4);
    }
  }
  // Close final station ring
  for (let c = 0; c < 4; ++c) {
    const p0 = C[c], p1 = C[(c + 1) % 4];
    strut(root, frame, [p0[0], STATIONS[4], p0[1]], [p1[0], STATIONS[4], p1[1]], 0.08, 4);
  }

  // Station hardpoint collars — faceted (4-sided = square) structural rings
  for (const y of STATIONS) {
    tube(root, dark, 1.35, 1.35, 0.5, [0, y, 0], 4);
    // Gusset plates at each corner — flat plates bolting collar to longerons
    for (const [x, z] of C) {
      box(root, lightArmor, [0.35, 0.5, 0.12], [x * 0.55, y, z * 0.55]);
    }
  }

  // Central conduit run — cable/pipe bundle along the axis
  strut(root, dark, [0, -HALF_L + 0.3, 0], [0, HALF_L - 0.3, 0], 0.18, 4);
  // Small conduit offset to one side (asymmetry — functional detail)
  strut(root, copper, [0.22, -HALF_L + 0.5, 0.22], [0.22, HALF_L - 0.5, 0.22], 0.05, 3);

  // End spider frames — structural transition from square truss to circular flange
  for (const [y, dir] of [[HALF_L, 1], [-HALF_L, -1]] as const) {
    for (const [x, z] of C) {
      strut(root, metal, [x, y, z], [0, y, 0], 0.09, 4);
    }
    flange(root, metal, [0, y, 0], [0, dir, 0]);
  }

  // Tiny status indicator lights at two stations (pinpoint glow, not strips)
  box(root, glow, [0.06, 0.06, 0.06], [HW + 0.02, 4, 0]);
  box(root, glow, [0.06, 0.06, 0.06], [HW + 0.02, -4, 0]);

  return root;
}

export const maps: Maps = standard_maps({
  seed: 101, panel: 0.12, rivets: 0.05, seams: 1, wear: 0.06, grime: 0.04, scorch: 0,
  stencils: [['SYS-TRUSS', 0.1, 0.15]],
});

export const meta = {
  name: 'spine_truss_m', kind: 'spine', family: 'truss', stations: 5, pitch: 4.0,
  half_width: 1.5, recess: 0.0, mass: 3.8, scale: 1, collider: 'auto',
};
