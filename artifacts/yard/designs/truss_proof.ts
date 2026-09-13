// Four-part PLAN-07 proof design (§4.8). This is a preview/audit assembly, not a shipping asset.
import * as THREE from 'three';
import { build as spine } from '../parts/spine_truss_m';
import { build as drive } from '../parts/drive_fusion_main';
import { build as tank } from '../parts/tank_drum_1';
import { build as nose } from '../parts/nose_ogive';

function place(root: THREE.Group, obj: THREE.Object3D, pos: [number, number, number], rot: [number, number, number] = [0, 0, 0]) {
  obj.position.set(...pos);
  obj.rotation.set(...rot);
  root.add(obj);
}

export function build(): THREE.Object3D {
  const root = new THREE.Group();
  root.name = 'design_truss_proof';
  root.add(spine());

  // Spine is 20 m long: caps at y ±10. Parts are flange-at-origin and body +Y.
  place(root, drive(), [0, -10, 0], [0, 0, Math.PI]); // aft: body +Y rotates to ship -Y
  place(root, tank(), [0, 10, 0]);                    // first fore virtual station
  place(root, nose(), [0, 14, 0]);                    // stacks after the 4 m tank
  return root;
}

export const meta = { name: 'design_truss_proof', scale: 1, collider: 'auto' };
