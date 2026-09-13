// Kestrel, blocky LOD (plan 05 s2.5's 8-60 px tier): the silhouette extruded, the drive bells as
// cylinders, the bands kept. It is the bold model with everything but the outline and the colour
// blocking removed - what a hull at 20 px across can still say. ~200 triangles.
import * as THREE from 'three';
import { armor, teal, ochre, dark, metal, box, cylinder, plate, standard_maps, type Maps } from './prims';

const L = 46;
const HW = 7.4;
const DECK = 4.6;
const at = (f: number) => L / 2 - f * L;

const HULL_OUTLINE: number[][] = [
  [-4.07, at(0.0)], [4.07, at(0.0)],
  [6.5, at(0.10)], [HW, at(0.18)],
  [HW, at(0.30)], [5.0, at(0.30)], [5.0, at(0.44)], [HW, at(0.44)],
  [HW, at(0.72)], [6.0, at(0.86)],
  [4.6, at(0.96)], [4.6, at(1.0)], [1.6, at(1.0)], [-1.6, at(1.0)],
  [-4.6, at(1.0)], [-6.0, at(0.86)], [-HW, at(0.72)],
  [-HW, at(0.44)], [-5.0, at(0.44)], [-5.0, at(0.30)], [-HW, at(0.30)],
  [-HW, at(0.18)], [-6.5, at(0.10)],
];

export function build(): THREE.Object3D {
  const group = new THREE.Group(); group.name = 'kestrel_blocky';
  // The plan silhouette, extruded - the notch and the fork live in the outline, so they cost
  // nothing here either.
  plate(group, HULL_OUTLINE, 7.2, -3.6, armor);
  // The bands: the one thing colour still says at twenty pixels.
  box(group, ochre, [2 * HW - 0.4, 3.5, 1.5], [0, 18.2, DECK - 0.6]);
  box(group, teal, [2 * HW - 0.4, 3.5, 1.5], [0, 14.5, DECK - 0.6]);
  // The drive bells as cylinders, in the fork prongs.
  for (const side of [-1, 1]) {
    cylinder(group, metal, 1.3, 1.3, 5.2, [side * 3.1, at(1.0) + 2.6, 0], 8);
  }
  // The bay: one dark cut in the deck line.
  box(group, dark, [6.4, 6.4, 1.5], [0, (at(0.30) + at(0.44)) / 2, DECK - 1.1]);
  return group;
}

export const maps: Maps = standard_maps({ seed: 12, panel: 0.06, wear: 0.2 });

export const meta = { name: 'kestrel_blocky', scale: 1.3 };
