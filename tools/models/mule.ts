// Mule: heavy salvage tug, re-authored at F8 (PLAN-03 §7.1). The plan-01 layout is kept — the six
// external freight pods, the open chassis rails, the two main and two auxiliary drive bells and the
// four RCS quads all stay where they were, because the compound collider and the port gate are tuned
// to that envelope. The pods grow a little (heavy freight, 15 x 20 x 12) so that they remain
// collision shapes: the exporter fits shapes from the *sum* of every cluster's projected area, so
// small decorative plates dilute the 3% cut and would otherwise drop the pods out of the collider.
// What the F8 pass adds: rounded command and bow surfaces, lathed bells with gimbal rings, bevelled
// pod frames with panel relief, and landing legs.
//
// Axes: nose +Y, dorsal +Z, starboard +X, metres.
import * as THREE from 'three';
import {
  black, copper, dark, frame, glass, hullPaint, lightArmor, metal, ochre,
  bevelled, box, dock, drive, each_tile, hazard_stripes, hp, hull, lathe, panel_relief,
  standard_maps, stencil_text, strut, thrusters, torus, tube, type Maps,
} from './prims';

/** Canopy glass for the tug's flight deck: one mapped blister, so the frame seams read on it. */
const canopy = new THREE.MeshStandardMaterial({
  color: '#2b5a68', roughness: 0.15, metalness: 0.72, emissive: '#28596a', emissiveIntensity: 0.45,
});
canopy.name = 'canopy';

/** A gimballed bell over the drive the prims already draw, so the flame keeps its exact place. */
function bell(root: THREE.Object3D, x: number, y: number, radius: number, segments: number, gimbal: boolean): void {
  lathe(root, metal, [[radius * 1.14, -8.4], [radius * 0.96, -3.4], [radius * 0.82, 2.4], [radius * 0.7, 6.6], [radius * 0.6, 10.4]], segments, [x, y, 0]);
  torus(root, copper, radius * 1.12, radius * 0.06, [x, y - 8.3, 0], [Math.PI / 2, 0, 0], 6, 14);
  if (gimbal) {
    torus(root, frame, radius * 0.68, radius * 0.1, [x, y + 10.4, 0], [Math.PI / 2, 0, 0], 6, 12);
    strut(root, copper, [x + radius * 0.62, y + 8.4, 0], [x + radius, y - 1.2, 0], radius * 0.08, 6);
  }
}

/** One external freight pod: bevelled shell, end bands, a lashing stripe and a mounting shoe. The
 *  bands are plain boxes, not bevelled: the shell's rounded edges are what the eye reads at range,
 *  and every extra bevelled solid here costs 108 triangles for a banded ring. */
function freight_pod(root: THREE.Object3D, x: number, y: number): void {
  bevelled(root, ochre, [15, 20, 12], [x, y, 1], [0, 0, 0], { radius: 0.8, segments: 1 });
  for (const dy of [-6, 6]) box(root, lightArmor, [15.5, 2.2, 12.6], [x, y + dy, 1]);
  for (const dy of [-9.2, 9.2]) box(root, dark, [15.2, 1.6, 12.8], [x, y + dy, 1]);
  box(root, black, [9, 0.9, 0.4], [x, y, 7.3]);
  box(root, metal, [11, 4, 3], [x > 0 ? x - 9 : x + 9, y, -1]);
}

export function build(): THREE.Object3D {
  const group = new THREE.Group(); group.name = 'mule';
  const flames: THREE.Mesh[] = [], rcs: THREE.Mesh[] = [];

  // ---- the load-bearing chassis the collider was fitted to (envelope verbatim) ------------------
  hull(group, 28, 78, 10, dark, 0, -1, 0);
  for (const side of [-1, 1]) {
    // Chassis rail: the same 3 x 66 x 4 beam, bevelled and tied into the spine by ribs.
    bevelled(group, metal, [3, 66, 4], [side * 15, -5, 0], [0, 0, 0], { radius: 0.5, segments: 1 });
    for (let i = 0; i < 4; i++) strut(group, frame, [side * 13.4, -33 + i * 19, 0], [side * 4.6, -33 + i * 19, 0], 0.5, 6);
    for (let i = 0; i < 3; i++) freight_pod(group, side * 24, -24 + i * 21);
    hull(group, 13, 22, 5, lightArmor, side * 18, 30, 2);
    for (let i = 0; i < 4; i++) box(group, black, [3.4, 1.6, 0.5], [side * 19.5, -12 + i * 6, 7.2]);
    drive(group, side * 10, -38, 5.9, flames);
    drive(group, side * 25, -37, 4.1, flames);
    bell(group, side * 10, -38, 5.9, 16, true);
    bell(group, side * 25, -37, 4.1, 14, false);
    // Coolant run down the rail, and the brace back to the main bell's gimbal.
    strut(group, copper, [side * 16.6, -30, 3.6], [side * 16.6, 28, 3.6], 0.34, 6);
    strut(group, frame, [side * 15.4, -30, 0], [side * 10, -28, 0], 0.6, 6);
  }

  // ---- command and bow --------------------------------------------------------------------------
  // The old bridge box becomes a rounded block over the same footprint, and the glazing becomes a
  // smooth blister instead of a flat pane.
  lathe(group, hullPaint, [[0, -11], [7.4, -9.6], [10.4, -4], [11, 3], [10, 8.6], [6.4, 10.6], [0, 11.6]], 20, [0, 24, 9]);
  lathe(group, canopy, [[0, -5.4], [3.0, -4.7], [4.4, -1.8], [4.4, 1.8], [3.0, 4.7], [0, 5.4]], 16, [0, 29, 13.4]);
  for (const x of [-5, 5]) box(group, dark, [0.9, 5, 2.2], [x, 29, 14.5]);
  bevelled(group, ochre, [12, 34, 3], [0, -12, 7], [0, 0, 0], { radius: 0.6, segments: 1 });
  for (let i = 0; i < 5; i++) box(group, dark, [9, 2, 0.6], [0, -22 + i * 4.6, 9]);
  // Tow yoke: a tug's whole job is the thing on its nose.
  strut(group, frame, [-9, 36, -1], [0, 46, -1], 0.7, 6);
  strut(group, frame, [9, 36, -1], [0, 46, -1], 0.7, 6);
  lathe(group, copper, [[1.4, 0], [1.4, 3], [0.9, 4.4]], 12, [0, 46, -1]);
  // hp.tow marks the yoke's throat, where a future tow constraint attaches.
  hp(group, 'tow', [0, 47.2, -1]);

  // ---- panel relief -----------------------------------------------------------------------------
  for (const side of [-1, 1]) {
    panel_relief(group, frame, [16, 60, 0.5], [side * 15.7, -3, 0], [0, side * Math.PI / 2, 0], { cols: 2, rows: 4, thickness: 0.6, depth: 0.5 });
  }
  panel_relief(group, dark, [18, 18, 0.5], [0, 24, 20.4], [0, 0, 0], { cols: 3, rows: 3, thickness: 0.6, depth: 0.5 });

  // ---- RCS: the four quad positions the sim lights ----------------------------------------------
  thrusters(group, 32, 24, -24, rcs);
  for (const side of [-1, 1]) for (const y of [24, -24]) {
    strut(group, metal, [side * 28.4, y, 2], [side * 31.6, y, 2], 0.5, 6);
  }

  // ---- landing legs, under the rail feet --------------------------------------------------------
  for (const side of [-1, 1]) for (const y of [-30, 20]) leg(group, side * 14.0, y, side);

  // ---- berths -----------------------------------------------------------------------------------
  // Aft mouth of the exposed frame, between the four bells: where a tug takes a tow or a barge.
  dock(group, 'A', [0, -44, 0], Math.PI, 'L');
  torus(group, frame, 7.0, 0.5, [0, -40.0, 0], [Math.PI / 2, 0, 0], 6, 12);
  strut(group, metal, [-9.4, -40, 2], [9.4, -40, 2], 0.6, 6);
  // Forward berth at the tow yoke, normal +Y.
  dock(group, 'fwd', [0, 48, -1], 0, 'S');

  for (const side of [-1, 1]) {
    const port = new THREE.Mesh(new THREE.CylinderGeometry(1.2, 1.2, 0.4, 10), glass);
    port.position.set(side * 4.5, 26.5, 19.2); port.rotation.x = Math.PI / 2; group.add(port);
  }
  return group;
}

/** A heavier two-stage leg than the corvette's, sized for the freight loads the tug carries. */
function leg(root: THREE.Object3D, x: number, y: number, side: number): void {
  const hip = [x, y, -4.8];
  const knee = [x + side * 3.2, y - 1.4, -9.4];
  const foot = [x + side * 1.6, y - 0.6, -14.2];
  bevelled(root, dark, [4.2, 5.0, 3.0], hip, [0, 0, 0], { radius: 0.5, segments: 1 });
  strut(root, metal, hip, knee, 1.25, 8);
  strut(root, metal, knee, foot, 1.0, 8);
  strut(root, copper, [hip[0], hip[1] - 1.8, hip[2] + 0.6], [knee[0] - side * 0.8, knee[1] + 0.6, knee[2] + 1.0], 0.45, 6);
  lathe(root, dark, [[1.8, 0.2], [3.0, 0], [3.2, -0.6], [2.8, -1.2]], 12, [foot[0], foot[1], foot[2]]);
}

export const maps: Maps = (() => {
  const kit = standard_maps({
    seed: 24, panel: 0.045, rivets: 0.022, seams: 4, wear: 0.34, grime: 0.26, scorch: 2,
    stencils: [['M-12', 0.09, 0.14], ['SALVAGE', 0.3, 0.6], ['TUG', 0.66, 0.22]],
    hazard: { x: 0.04, y: 0.8, w: 0.92, h: 0.14, angle: -Math.PI / 3 },
  });
  return {
    normal: kit.normal,
    roughness: (ctx, size) => {
      kit.roughness?.(ctx, size);
      each_tile(ctx, size, (tile) => {
        // Polished fields: the pods' door faces and the bridge glazing. Dark green is smooth.
        tile.ctx.globalAlpha = 0.35;
        tile.ctx.fillStyle = [0, 62, 255];
        tile.ctx.fillRect(0.1 * tile.w, 0.12 * tile.h, 0.32 * tile.w, 0.24 * tile.h);
        tile.ctx.fillRect(0.6 * tile.w, 0.6 * tile.h, 0.28 * tile.w, 0.22 * tile.h);
        tile.ctx.globalAlpha = 1;
      });
    },
    albedo: (ctx, size) => {
      kit.albedo?.(ctx, size);
      each_tile(ctx, size, (tile) => {
        // Freight-handling warnings: this hull is the one that moves other people's cargo.
        stencil_text(tile, 'LIFT', { x: 0.56, y: 0.4, scale: 0.022 });
        stencil_text(tile, 'NO STEP', { x: 0.1, y: 0.72, scale: 0.02 });
        hazard_stripes(tile, { x: 0.06, y: 0.5, w: 0.3, h: 0.09 }, { pitch: 0.038, angle: Math.PI / 4 });
      });
    },
  };
})();

export const meta = { name: 'mule', scale: 1.3, collider: { halfLength: 65, halfWidth: 43 } };
