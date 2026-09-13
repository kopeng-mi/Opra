// Variant B, "Caterpillar Ingot": heavy industrial hauler, five modules on the 4 m station pitch.
//
//   station   y range      module
//   aft cap   -7 .. -3     drive_quad_block     (rotZ pi)
//   -3        -3 .. +1     section_reactor
//   +1        +1 .. +5     section_machinery    6.4 m span, the widest
//   +5        +5 .. +9     section_freight
//   +9        +9 .. +13    nose_pushbow
import * as THREE from 'three';
import { build as drive } from '../parts/drive_quad_block';
import { build as reactor } from '../parts/section_reactor';
import { build as machinery } from '../parts/section_machinery';
import { build as freight } from '../parts/section_freight';
import { build as nose } from '../parts/nose_pushbow';

function place(root: THREE.Group, part: THREE.Object3D, y: number, roll = 0) {
  part.position.set(0, y, 0);
  part.rotation.set(0, 0, roll);
  root.add(part);
}

export function build(): THREE.Object3D {
  const root = new THREE.Group();
  root.name = 'variant_b_ingot';
  place(root, drive(), -3, Math.PI);
  place(root, reactor(), -3);
  place(root, machinery(), 1);
  place(root, freight(), 5);
  place(root, nose(), 9);
  return root;
}

export const meta = { name: 'variant_b_ingot', scale: 1, collider: 'auto' };
