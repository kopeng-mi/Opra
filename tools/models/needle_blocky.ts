// Needle, blocky LOD (plan 05 s2.5's 8-60 px tier): the blade-plate silhouette and the fuselage
// as one lofted run, the identity blades kept. ~200 triangles.
import * as THREE from 'three';
import { ceramic, teal, ochre, dark, metal, box, cylinder, plate, standard_maps, type Maps } from './prims';

export function build(): THREE.Object3D {
  const group = new THREE.Group(); group.name = 'needle_blocky';
  // The blade plate: the collider-tuned outline, extruded.
  plate(group, [[0,47],[-8,17],[-9,-22],[-16,-34],[16,-34],[9,-22],[8,17]], 6, -3, ceramic);
  // The fuselage: one block where the loft was.
  box(group, ceramic, [11.0, 58.0, 11.0], [0, 8, 4]);
  // The identity blades: the marks the hull still carries at twenty pixels.
  for (const side of [-1, 1]) {
    box(group, teal, [3.5, 31, 1.6], [side * 14, -11, 2]);
    box(group, ochre, [3.5, 5.0, 1.6], [side * 14, 5.0, 2]);
  }
  // The drive: one cylinder.
  cylinder(group, metal, 6.5, 7.0, 12.0, [0, -40, 0], 8);
  // The canopy: one block.
  box(group, dark, [6.4, 13.0, 3.4], [0, 17, 9.6]);
  return group;
}

export const maps: Maps = standard_maps({ seed: 57, panel: 0.06, wear: 0.2 });

export const meta = { name: 'needle_blocky', scale: 1.3 };
