// Needle: recon interceptor, re-authored to plan 05 s3 (K6, the kestrel pattern).
//
// The plan-01 envelope is kept - the long central blade plate, the swept side plates, the single
// oversized drive and the two RCS quads stay exactly where they were, because the compound collider
// and the lit-hull check are tuned to that silhouette. The s3 pass trades sub-pixel detail for
// budget: the turned ribs, struts and portholes (0.3-0.9 m stumps at the home framing) become map
// content, the identity blades widen to the 3.5 m band rule, and the bold LOD keeps to the plan's
// 1500 triangles.
//
// Axes: nose +Y, dorsal +Z, starboard +X, metres.
import * as THREE from 'three';
import {
  black, ceramic, copper, dark, frame, glass, lightArmor, metal, ochre, teal,
  bevelled, box, cylinder, dock, drive, lathe, plate, standard_maps, type Maps,
} from './prims';

/** Canopy glass: tinted and mapped, so the glazing's frame seams read on it. */
const canopy = new THREE.MeshStandardMaterial({
  color: '#2d5f6e', roughness: 0.15, metalness: 0.7, emissive: '#2a5967', emissiveIntensity: 0.5,
});
canopy.name = 'canopy';

export function build(): THREE.Object3D {
  const group = new THREE.Group(); group.name = 'needle';
  const flames: THREE.Mesh[] = [], rcs: THREE.Mesh[] = [];

  // ---- the frame the collider was fitted to (outlines verbatim) ---------------------------------
  plate(group, [[0,47],[-8,17],[-9,-22],[-16,-34],[16,-34],[9,-22],[8,17]], 6, -3, ceramic);
  for (const side of [-1, 1]) {
    plate(group, [[side*7,8],[side*22,-25],[side*21,-34],[side*11,-24]], 2, -1, dark);
    // Identity blades: the 3.5 m band rule (s3.2) - 3.5 x 31 of teal along each side plate, with
    // an ochre tip block, 108 and 15 m2 of plan-view area. They carry the marks and the shadow.
    box(group, teal, [3.5, 31, 1.6], [side * 14, -11, 2]);
    box(group, ochre, [3.5, 5.0, 1.6], [side * 14, 5.0, 2]);
    cylinder(group, copper, 0.9, 1.2, 17, [side * 5, 36, 0], 8);
  }

  // ---- body: one lofted run from the drive mount to the needle prow ------------------------------
  lathe(group, lightArmor, [[0, -25], [3.4, -23.6], [5.2, -19], [6.0, -10], [6.0, 10], [5.2, 20], [3.6, 27], [1.6, 31], [0, 32.5]], 8, [0, 8, 4]);
  // The dorsal avionics spine; its access panels are one 2.4 m block each.
  box(group, dark, [3.8, 30, 3.8], [0, 8, 8.4]);
  for (let i = 0; i < 2; i++) box(group, metal, [2.6, 3.2, 1.5], [0, 2 + i * 10, 10.6]);
  // Prow: the needle itself, on a shoulder that steps down to it.
  lathe(group, ceramic, [[0, -12], [3.4, -10.4], [4.6, -6], [4.6, 2], [3.0, 7], [1.4, 10], [0, 12]], 8, [0, 36, 3]);

  // ---- canopy -----------------------------------------------------------------------------------
  lathe(group, canopy, [[0, -7.2], [2.6, -6.5], [3.6, -2.4], [3.6, 2.4], [2.6, 6.5], [0, 7.2]], 8, [0, 17, 8.4]);

  // ---- drive ------------------------------------------------------------------------------------
  drive(group, 0, -37, 7, flames);
  // Bell skirt over the primitives; the gimbal ring's torus became a drum.
  lathe(group, metal, [[8.0, -8.4], [6.9, -4.4], [5.8, -0.4], [4.9, 3.6], [4.2, 8.0], [3.6, 12.0]], 10, [0, -37, 0]);
  cylinder(group, frame, 4.1, 4.4, 1.6, [0, -25.2, 0], 10);

  // ---- RCS: the four quad positions the sim lights ----------------------------------------------
  for (const side of [-1, 1]) for (const y of [14, -24]) {
    box(group, dark, [2.4, 2.4, 2.4], [side * 11.4, y, 2]);
    effectJet(group, flames, [side * 12.4, y, 2], side);
  }

  // ---- landing legs: a tricycle, which is what a nose-heavy interceptor wants -------------------
  leg(group, -9.5, -26, -1);
  leg(group, 9.5, -26, 1);
  leg(group, 0, 30, 1);

  // ---- berths -----------------------------------------------------------------------------------
  // Aft berth either side of the single bell, and a forward berth at the prow.
  dock(group, 'A', [0, -38, 0], Math.PI, 'M');
  box(group, metal, [15, 1.5, 1.5], [0, -36.5, 0]);
  dock(group, 'fwd', [0, 48, 3], 0, 'S');

  // The sensor blister the recon role wants.
  const blister = new THREE.Mesh(new THREE.SphereGeometry(1.7, 10, 8), dark);
  blister.position.set(0, -16, 10.2); group.add(blister);
  return group;
}

/** The RCS jet cone, posed outboard of its quad. One per corner, named for the loader's rule. */
function effectJet(group: THREE.Group, flames: THREE.Mesh[], at: number[], side: number): void {
  const geometry = new THREE.ConeGeometry(0.55, 4.5, 8, 1, true);
  geometry.rotateZ(Math.PI / 2); geometry.translate(-2.25, 0, 0);
  const flame = new THREE.Mesh(geometry, exhaustMaterial());
  flame.name = 'rcs-jet'; flame.userData.effect = true;
  flame.rotation.y = side * -Math.PI / 2;
  flame.position.set(at[0], at[1], at[2]); flame.visible = false;
  group.add(flame); flames.push(flame);
}

import { exhaust, jetMaterial } from './prims';
function exhaustMaterial(): THREE.Material { return exhaust; }
void jetMaterial;

/** A lighter three-segment leg: the needle lands on skids rather than a freighter's gear. */
function leg(root: THREE.Object3D, x: number, y: number, side: number): void {
  const hip = [x, y, -3.4];
  const knee = [x + side * 2.0, y - 0.8, -7.0];
  const foot = [x + side * 1.0, y - 0.2, -10.4];
  bevelled(root, dark, [2.8, 3.6, 2.2], hip, [0, 0, 0], { radius: 0.35, segments: 1 });
  const span = (from: number[], to: number[], size: number) => {
    const length = Math.hypot(to[0] - from[0], to[1] - from[1], to[2] - from[2]);
    const mid = [(from[0] + to[0]) / 2, (from[1] + to[1]) / 2, (from[2] + to[2]) / 2];
    const mesh = box(root, metal, [size, length, size], mid);
    mesh.rotation.x = Math.atan2(to[2] - from[2], Math.hypot(to[0] - from[0], to[1] - from[1]));
    void length;
    return mesh;
  };
  span(hip, knee, 1.5);
  span(knee, foot, 1.5);
  box(root, dark, [2.6, 4.0, 1.5], [foot[0], foot[1], foot[2]]);
}

export const maps: Maps = standard_maps({
  seed: 57, panel: 0.04, rivets: 0.025, seams: 2, wear: 0.3, grime: 0.18, scorch: 2,
  stencils: [['N-3', 0.1, 0.15], ['RECON', 0.36, 0.62]],
  hazard: { x: 0.06, y: 0.78, w: 0.88, h: 0.1, angle: Math.PI / 3 },
});

// The plan-01 envelope's collider, unchanged (plan-04 H4): the compound shapes the exporter
// derives from this geometry are what the sim collides on; the box stays the lit-hull reference.
export const meta = { name: 'needle', scale: 1.3 };
