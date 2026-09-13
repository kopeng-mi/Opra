// Mule: heavy salvage tug, re-authored to plan 05 s3 (K6, the kestrel pattern).
//
// Hull 58 m, 58 x 23.7 x 16.6. The s3.2 rules at the 3.00 px/m home framing: features under 1.5 m
// became map content (the clamp struts, the pipe run, the radiator ribs - all sub-pixel stumps at
// the framing that sees them), the identity bands are 3.5 m blocks on every pod, and the bold LOD
// keeps to the plan's 1500-triangle budget. Colliders and hardpoints do not move (s3.5).
//
// Axes as always: nose +Y, dorsal +Z, starboard +X, metres.
import * as THREE from 'three';
import {
  armor, black, copper, dark, frame, glass, lightArmor, metal, teal, ochre,
  bevelled, box, cylinder, dock, effect_cone, hp, lathe, plate, standard_maps, type Maps,
} from './prims';

// Plan frame: L = 58, half-width 11.85, deck +/-8.3.
const L = 58;
const HW = 11.85;
const DECK = 8.3;
const at = (f: number) => L / 2 - f * L;

/** One grapple arm: an open three-segment V ending in a claw pair. Boxes, not turned cylinders:
 *  the arm reads in silhouette, and a silhouette costs a tenth of what a lathe costs. */
function grapple_arm(root: THREE.Group, side: number): void {
  const shoulder = [side * 5.5, at(0.05), 0];
  const elbow = [side * 10.5, at(0.10), 0];
  const wrist = [side * 12.4, at(0.15), 0];
  box(root, copper, [2.2, 2.2, 2.2], shoulder);
  const segment = (to: number[], size: number, material: THREE.Material) => {
    // A box stretched along the segment: position the midpoint, yaw it to the chord.
    const span = Math.hypot(to[0] - shoulder[0], to[1] - shoulder[1]);
    const mid = [(shoulder[0] + to[0]) / 2, (shoulder[1] + to[1]) / 2, (shoulder[2] + to[2]) / 2];
    const mesh = box(root, material, [size, span, size], mid);
    mesh.rotation.z = Math.atan2(to[0] - shoulder[0], to[1] - shoulder[1]);
  };
  segment(elbow, 1.7, frame);
  segment(wrist, 1.5, frame);
  segment([wrist[0] - side * 1.6, wrist[1], 1.4], 1.5, dark);
  segment([wrist[0] - side * 1.6, wrist[1], -1.4], 1.5, dark);
}

export function build(): THREE.Object3D {
  const group = new THREE.Group(); group.name = 'mule';
  const flames: THREE.Mesh[] = [];

  // ---- towing end: grapple arms and the yoke between them (0.00-0.16) -------------------------
  for (const side of [-1, 1]) grapple_arm(group, side);
  box(group, dark, [2 * 0.30 * HW, 2.2, 2.2], [0, at(0.10), 0]);
  cylinder(group, metal, 1.5, 1.5, 5.6, [0, at(0.10), 0], 8);
  box(group, copper, [1.5, 1.6, 1.5], [0, at(0.135), 0]);

  // ---- cab (0.16-0.26): faceted command block, the bridge plated with sensor bands ------------
  plate(group, [
    [-3.6, at(0.16)], [3.6, at(0.16)], [5.33, at(0.21)], [5.33, at(0.26)],
    [-5.33, at(0.26)], [-5.33, at(0.21)],
  ], 7.5, -1.2, armor);
  box(group, glass, [7.6, 1.5, 3.0], [0, at(0.175), 2.6]);

  // ---- open rail chassis (0.26-0.80): two rails, four crossmembers, the tank between ----------
  const railFore = at(0.26), railAft = at(0.80), railX = 0.35 * HW;
  for (const side of [-1, 1]) {
    box(group, dark, [1.5, railFore - railAft, 2.2], [side * railX, (railFore + railAft) / 2, 0]);
  }
  for (let i = 0; i < 4; ++i) {
    const y = railFore - (i + 0.5) * ((railFore - railAft) / 4);
    box(group, frame, [2 * railX, 1.5, 1.5], [0, y, 0]);
  }
  cylinder(group, metal, 1.5, 1.5, railFore - railAft - 6, [0, (railFore + railAft) / 2, -2.4], 8);

  // ---- cargo pods, three per side (0.32, 0.48, 0.64), banded ----------------------------------
  // The identity bands ride the pods: teal abaft ochre, 3.5 m along every pod's flank, 15 m2 of
  // plan-view area per band. This hull carries the marks six times, as a working fleet does.
  for (const side of [-1, 1]) for (const f of [0.32, 0.48, 0.64]) {
    const x = side * 0.80 * HW, y = at(f);
    box(group, lightArmor, [4.4, 6.6, 5.2], [x, y, 0]);
    box(group, teal, [1.5, 3.5, 4.4], [x + side * 2.2, y - 0.9, 0]);
    box(group, ochre, [1.5, 3.5, 4.4], [x + side * 2.2, y + 1.2, 0]);
    box(group, dark, [3.6, 1.5, 4.2], [x, y, 2.8]);
  }

  // ---- dorsal turret (0.30, centreline): the pdc_turret's mount -------------------------------
  cylinder(group, dark, 1.15, 1.25, 1.5, [0, at(0.30), DECK + 1.6], 14);
  hp(group, 'pdc.dorsal', [0, at(0.30), DECK + 2.4]);

  // ---- radiator panels (0.62-0.80, +/-0.55), deployed -----------------------------------------
  for (const side of [-1, 1]) {
    const x = side * 0.55 * HW;
    box(group, copper, [1.5, 1.5, 2.2], [x, at(0.66), DECK - 0.4]);
    box(group, dark, [6.0, 3.0, 1.5], [x + side * 1.3, at(0.71), DECK + 2.6]);
  }

  // ---- drive bells, 2x2 square cluster (0.80-1.00) --------------------------------------------
  for (const sx of [-1, 1]) for (const sz of [-1, 1]) {
    const x = sx * 0.20 * HW, z = sz * 2.5;
    lathe(group, metal, [
      [1.30, 0.00], [0.90, 3.30], [0.78, 4.60], [0.92, 5.20],
    ], 8, [x, at(1.0), z]);
    cylinder(group, copper, 1.42, 1.34, 1.5, [x, at(1.0) - 0.6, z], 8);
    effect_cone(group, flames, { name: 'flame', radius: 1.25, length: 14.0,
      pos: [x, at(1.0) - 7.6, z] });
  }
  box(group, frame, [2 * 0.20 * HW + 3.4, 2.0, 7.4], [0, at(0.815), 0]);

  // ---- RCS corners: cab shoulders forward, drive frame abaft ----------------------------------
  for (const side of [-1, 1]) for (const [y, z] of [[at(0.20), 2.0], [at(0.86), 0.0]]) {
    const x = side * (y > 0 ? 8.2 : 5.2);
    box(group, dark, [1.6, 1.6, 1.6], [x, y, z]);
    effect_cone(group, flames, { name: 'rcs-jet', radius: 0.55, length: 4.5, pos: [x + side * 3.1, y, z], rot: [0, 0, -side * Math.PI / 2] });
  }

  // ---- berth: the tug docks stern-first through the drive frame -------------------------------
  dock(group, 'A', [0, at(1.0), 0], Math.PI, 'M');

  // ---- landing pads under the rails -----------------------------------------------------------
  for (const side of [-1, 1]) for (const f of [0.40, 0.60]) {
    box(group, metal, [1.5, 1.5, 1.5], [side * (railX + 1.2), at(f), -DECK + 1.5]);
    box(group, dark, [2.4, 2.0, 1.5], [side * (railX + 1.8), at(f) - 0.9, -DECK - 2.2]);
  }

  return group;
}

export const maps: Maps = standard_maps({
  seed: 24, panel: 0.045, rivets: 0.025, seams: 4, wear: 0.34, grime: 0.26, scorch: 2,
  stencils: [['M-12', 0.09, 0.14], ['SALVAGE', 0.3, 0.6], ['TUG', 0.66, 0.22]],
  hazard: { x: 0.04, y: 0.8, w: 0.92, h: 0.14, angle: -Math.PI / 3 },
});

// The plan's own collider table (s2.2): the spine box is the authored reference; the sidecar's
// compound shapes are derived from the geometry and are what the sim collides on.
export const meta = { name: 'mule', scale: 1.3, collider: { halfLength: 29, halfWidth: 11.85 } };
