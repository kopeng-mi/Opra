// Variant A, "Hammerhead Submarine": naval patrol corvette, five modules on the 4 m station pitch.
//
// Section-module architecture (DESIGN-DIRECTION-v2): no spine object. Each module's flange is at
// its own -Y with the body toward +Y, so a fore-facing module is placed at the station it starts
// from and the drive is placed at the aft cap with rotZ(pi) - which is what puts its plume at
// ship -Y (PLAN-07 s3.4).
//
//   station   y range      module
//   aft cap   -7 .. -3     drive_twin_torch        (rotZ pi)
//   -3        -3 .. +1     section_radiator_wing   5.8 m span, the widest
//   +1        +1 .. +5     section_tank_saddle     3.1 m waist, the narrowest
//   +5        +5 .. +9     section_combat_a
//   +9        +9 .. +13    nose_hammerhead         4.8 m sponsons
import * as THREE from 'three';
import { build as drive } from '../parts/drive_twin_torch';
import { build as radiator } from '../parts/section_radiator_wing';
import { build as tank } from '../parts/section_tank_saddle';
import { build as combat } from '../parts/section_combat_a';
import { build as nose } from '../parts/nose_hammerhead';

function place(root: THREE.Group, part: THREE.Object3D, y: number, roll = 0) {
  part.position.set(0, y, 0);
  part.rotation.set(0, 0, roll);
  root.add(part);
}

export function build(): THREE.Object3D {
  const root = new THREE.Group();
  root.name = 'variant_a_hammerhead';
  place(root, drive(), -3, Math.PI);
  place(root, radiator(), -3);
  place(root, tank(), 1);
  place(root, combat(), 5);
  place(root, nose(), 9);
  return root;
}

export const meta = { name: 'variant_a_hammerhead', scale: 1, collider: 'auto' };
