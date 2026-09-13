// Mule, blocky LOD (plan 05 s2.5's 8-60 px tier): the open chassis closes into one slab, the pods
// and arms stay as the blocks they already were, the bands kept. ~200 triangles.
import * as THREE from 'three';
import { armor, teal, ochre, dark, metal, box, cylinder, standard_maps, type Maps } from './prims';

const HW = 11.85;
const DECK = 8.3;
const at = (f: number) => 29 - f * 58;

export function build(): THREE.Object3D {
  const group = new THREE.Group(); group.name = 'mule_blocky';
  // The hull: one armoured slab where the open chassis was - at twenty pixels the gaps are mud.
  box(group, armor, [2 * HW - 3.0, 34.0, 12.0], [0, -4.0, 0]);
  // The cab.
  box(group, metal, [10.7, 7.5, 7.5], [0, 13.5, 2.5]);
  // The grapple arms: two prongs, splayed.
  for (const side of [-1, 1]) {
    box(group, dark, [1.7, 18.0, 1.7], [side * 9.0, 22.0, 0]);
  }
  // The pods: one block per side.
  for (const side of [-1, 1]) {
    box(group, metal, [4.4, 22.0, 5.2], [side * (0.8 * HW), -8.0, 0]);
    box(group, teal, [1.5, 3.5, 5.2], [side * (0.8 * HW + 2.2), -10.0, 0]);
    box(group, ochre, [1.5, 3.5, 5.2], [side * (0.8 * HW + 2.2), -5.0, 0]);
  }
  // The bell cluster: one cylinder per bell, no skirt detail.
  for (const sx of [-1, 1]) for (const sz of [-1, 1]) {
    cylinder(group, dark, 1.3, 1.3, 5.2, [sx * 0.20 * HW, at(1.0) + 2.6, sz * 2.5], 8);
  }
  return group;
}

export const maps: Maps = standard_maps({ seed: 24, panel: 0.06, wear: 0.25 });

export const meta = { name: 'mule_blocky', scale: 1.3 };
