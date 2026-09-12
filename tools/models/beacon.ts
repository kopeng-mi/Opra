// Relay beacon (PLAN-03 §7.1, re-authored at F8): a skirted base, a ribbed drum, four radiator
// fins and a tapered relay mast with a lamp cluster and the additive halo the loader draws as an
// effect. The envelope is the plan-01 beacon's: same skirt radius, mast a little taller.
import * as THREE from 'three';
import {
  black, dark, lightArmor, metal, copper, bevelled, box, cylinder, each_tile, hazard_stripes, lamps,
  lathe, standard_maps, stencil_text, strut, tube, type Maps,
} from './prims';

export const beaconLampMaterial = new THREE.MeshBasicMaterial({ color: '#dce6e8' });
export const beaconHaloMaterial = new THREE.MeshBasicMaterial({ color: '#83b9b5', transparent: true, opacity: 0.5, blending: THREE.AdditiveBlending, depthWrite: false, side: THREE.DoubleSide });
beaconLampMaterial.userData.shared = true;
beaconHaloMaterial.userData.shared = true;

export function build(): THREE.Object3D {
  const group = new THREE.Group(); group.name = 'beacon';
  // Skirted base: a lathe step and a bolted flange, so the beacon reads as planted on something.
  lathe(group, dark, [[11.0, -11.0], [11.4, -10.2], [10.6, -9.0], [10.6, -7.6]], 20);
  tube(group, copper, 10.2, 10.2, 1.1, [0, -7.9, 0], 20);
  // Housing drum with two stiffening bands and a ring of ventilation louvres.
  lathe(group, metal, [[8.6, -7.6], [9.3, -6.0], [9.3, 3.6], [8.2, 5.4], [7.0, 6.6]], 24);
  tube(group, dark, 9.0, 9.0, 1.0, [0, -2.6, 0], 20);
  tube(group, dark, 8.9, 8.9, 1.0, [0, 1.6, 0], 20);
  for (let i = 0; i < 8; i++) {
    const angle = (i / 8) * Math.PI * 2;
    box(group, black, [3.2, 1.0, 1.2], [Math.cos(angle) * 8.7, 4.6, Math.sin(angle) * 8.7], -angle);
  }
  // Four radiator fins, bevelled because their edges read at range.
  for (let i = 0; i < 4; i++) {
    const angle = Math.PI / 4 + (i * Math.PI) / 2;
    const x = Math.cos(angle) * 9.2, z = Math.sin(angle) * 9.2;
    bevelled(group, dark, [11.6, 10.4, 0.9], [x, 0.4, z], [0, -angle, 0], { radius: 0.3, segments: 1 });
    box(group, metal, [12.8, 1.2, 1.0], [x, 5.2, z], -angle);
    box(group, metal, [12.8, 1.2, 1.0], [x, -4.4, z], -angle);
  }
  // Service collar and the equipment ring the mast steps out of.
  lathe(group, lightArmor, [[4.6, 6.6], [5.4, 7.8], [5.4, 8.8], [4.2, 9.6]], 20);
  tube(group, copper, 5.2, 5.2, 0.8, [0, 8.9, 0], 16);
  for (let i = 0; i < 6; i++) {
    const angle = (i / 6) * Math.PI * 2 + 0.3;
    const x = Math.cos(angle) * 4.0, z = Math.sin(angle) * 4.0;
    bevelled(group, metal, [2.6, 2.4, 2.4], [x, 10.8, z], [0, -angle, 0], { radius: 0.35, segments: 1 });
    strut(group, metal, [x, 10.8, z], [Math.cos(angle) * 1.2, 14.4, Math.sin(angle) * 1.2], 0.26, 6);
  }
  // Mast: a stepped taper with three guy wires back down to the collar.
  lathe(group, lightArmor, [[2.2, 9.6], [1.7, 11.2], [1.7, 15.2], [1.1, 16.8], [1.1, 20.2]], 16);
  for (let i = 0; i < 3; i++) {
    const angle = (i / 3) * Math.PI * 2;
    strut(group, dark, [Math.cos(angle) * 5.2, 8.9, Math.sin(angle) * 5.2], [Math.cos(angle) * 0.9, 17.2, Math.sin(angle) * 0.9], 0.18, 5);
  }
  // Cross arm with two relay horns and the lamp mast.
  bevelled(group, lightArmor, [11.6, 0.9, 0.9], [0, 15.2, 0], [0, 0, 0], { radius: 0.35, segments: 1 });
  for (const side of [-1, 1]) {
    cylinder(group, metal, 0.35, 0.7, 3.2, [side * 5.4, 17.4, 0], 10);
    const horn = new THREE.Mesh(new THREE.SphereGeometry(0.9, 8, 6), copper);
    horn.position.set(side * 5.7, 19.3, 0); group.add(horn);
  }
  cylinder(group, metal, 1.1, 2.2, 6.0, [0, 21.2, 0], 14);
  bevelled(group, lightArmor, [4.4, 2.4, 2.4], [0, 25.0, 0], [0, 0, 0], { radius: 0.5, segments: 1 });
  box(group, metal, [10.0, 0.5, 0.5], [0, 25.0, 1.2]);
  // Navigation lamps: six around the collar, one on each relay horn.
  const ring: number[][] = [];
  for (let i = 0; i < 6; i++) {
    const angle = (i / 6) * Math.PI * 2;
    ring.push([Math.cos(angle) * 5.6, 9.8, Math.sin(angle) * 5.6]);
  }
  lamps(group, '#b7dfdd', ring, 0.5, 6);
  lamps(group, '#dce6e8', [[-5.7, 19.9, 0], [5.7, 19.9, 0]], 0.7, 8);
  const lamp = new THREE.Mesh(new THREE.SphereGeometry(1.6, 10, 8), beaconLampMaterial);
  lamp.position.set(0, 27.0, 2.6); group.add(lamp);
  const halo = new THREE.Mesh(new THREE.RingGeometry(2.4, 7, 28), beaconHaloMaterial);
  halo.name = 'halo'; halo.userData.effect = true;
  halo.position.set(0, 27.0, 2.3); group.add(halo);
  return group;
}

/** Panels, rivets and a hazard skirt, with the service number and a NO STEP stencil. */
export const maps: Maps = (() => {
  const kit = standard_maps({ seed: 41, panel: 0.2, rivets: 0.1, seams: 1, wear: 0.24, grime: 0.22, stencils: [['RLY-04', 0.12, 0.18]] });
  return {
    normal: kit.normal,
    roughness: kit.roughness,
    albedo: (ctx, size) => {
      kit.albedo?.(ctx, size);
      each_tile(ctx, size, (tile) => {
        hazard_stripes(tile, { x: 0.06, y: 0.72, w: 0.88, h: 0.2 }, { pitch: 0.05, angle: Math.PI / 5 });
        stencil_text(tile, 'NO STEP', { x: 0.58, y: 0.34, scale: 0.022 });
      });
    },
  };
})();

export const meta = { name: 'beacon', scale: 1, collider: 'auto' };
