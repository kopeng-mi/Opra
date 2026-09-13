// Variant C, "Arrowhead Waverider": stealth delta interceptor, five modules on the 4 m pitch.
//
//   station   y range      module
//   aft cap   -7 .. -3     drive_stealth_twin      (rotZ pi), 5.3 m shroud with a 1.3 m notch
//   -3        -3 .. +1     section_delta_aft       7.0 m span, the widest
//   +1        +1 .. +5     section_stealth_combat
//   +5        +5 .. +9     section_delta_fore
//   +9        +9 .. +13    nose_stealth_needle     2.6 m, the narrowest
import * as THREE from 'three';
import { build as drive } from '../parts/drive_stealth_twin';
import { build as aft } from '../parts/section_delta_aft';
import { build as combat } from '../parts/section_stealth_combat';
import { build as fore } from '../parts/section_delta_fore';
import { build as nose } from '../parts/nose_stealth_needle';

function place(root: THREE.Group, part: THREE.Object3D, y: number, roll = 0) {
  part.position.set(0, y, 0);
  part.rotation.set(0, 0, roll);
  root.add(part);
}

export function build(): THREE.Object3D {
  const root = new THREE.Group();
  root.name = 'variant_c_waverider';
  place(root, drive(), -3, Math.PI);
  place(root, aft(), -3);
  place(root, combat(), 1);
  place(root, fore(), 5);
  place(root, nose(), 9);
  return root;
}

export const meta = { name: 'variant_c_waverider', scale: 1, collider: 'auto' };
