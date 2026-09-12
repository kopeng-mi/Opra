// Needle: recon interceptor, re-authored at F8 (PLAN-03 §7.1). The plan-01 envelope is kept — the
// long central blade plate, the swept side plates, the single oversized drive and the two RCS quads
// stay exactly where they were, because the compound collider and the lit-hull check are tuned to
// that silhouette. The F8 pass gives it the body a real interceptor has: a lofted fuselage, a needle
// prow, a curved canopy, a lathed bell with a gimbal ring, radiator blades with panel relief, and
// landing legs.
//
// Axes: nose +Y, dorsal +Z, starboard +X, metres.
import * as THREE from 'three';
import {
  black, ceramic, copper, dark, frame, glass, lightArmor, metal, teal,
  bevelled, box, cylinder, dock, drive, each_tile, hazard_stripes, hull, lathe, panel_relief,
  plate, scorch, standard_maps, stencil_text, strut, thrusters, torus, tube, type Maps,
} from './prims';

/** Canopy glass: tinted and mapped, so the glazing's frame seams and polished field read on it.
 *  The small portholes keep the shared `glass`, which the maps leave clean. */
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
    // Radiator blade: the old flat plate, bevelled, with a tube run along its root.
    bevelled(group, teal, [2, 31, 0.8], [side * 14, -11, 2], [0, 0, side * 0.4], { radius: 0.3, segments: 1 });
    strut(group, copper, [side * 12.6, -24, 2], [side * 15.0, -2, 2], 0.28, 6);
    for (let i = 0; i < 4; i++) box(group, metal, [5, 0.8, 1], [side * 15, -19 + i * 3, 2]);
    cylinder(group, copper, 0.4, 0.7, 17, [side * 5, 36, 0], 6);
  }

  // ---- body: one lofted run from the drive mount to the needle prow ------------------------------
  lathe(group, lightArmor, [[0, -25], [3.4, -23.6], [5.2, -19], [6.0, -10], [6.0, 10], [5.2, 20], [3.6, 27], [1.6, 31], [0, 32.5]], 22, [0, 8, 4]);
  // Frame ribs, then a dorsal avionics spine and its access panels.
  for (const y of [-12, 0, 12, 24]) torus(group, frame, 6.12, 0.3, [0, y + 8, 4], [Math.PI / 2, 0, 0], 6, 12);
  tube(group, dark, 1.9, 1.9, 30, [0, 8, 9.4], 12);
  for (let i = 0; i < 4; i++) box(group, metal, [2.6, 1.6, 1.2], [0, -3 + i * 7, 10.6]);
  // Prow: the needle itself, on a shoulder that steps down to it.
  lathe(group, ceramic, [[0, -12], [3.4, -10.4], [4.6, -6], [4.6, 2], [3.0, 7], [1.4, 10], [0, 12]], 18, [0, 36, 3]);
  strut(group, copper, [0, 42, 3], [0, 47.5, 3], 0.28, 8);

  // ---- canopy -----------------------------------------------------------------------------------
  lathe(group, canopy, [[0, -7.2], [2.6, -6.5], [3.6, -2.4], [3.6, 2.4], [2.6, 6.5], [0, 7.2]], 18, [0, 17, 8.4]);
  for (const y of [11.6, 22.4]) torus(group, dark, 3.62, 0.26, [0, y, 8.4], [Math.PI / 2, 0, 0], 6, 14);
  tube(group, dark, 0.26, 0.26, 14.0, [0, 17, 12.0], 6);

  // ---- drive ------------------------------------------------------------------------------------
  drive(group, 0, -37, 7, flames);
  // Bell skirt over the primitives, a gimbal ring at the throat and two actuators.
  lathe(group, metal, [[8.0, -8.4], [6.9, -4.4], [5.8, -0.4], [4.9, 3.6], [4.2, 8.0], [3.6, 12.0]], 20, [0, -37, 0]);
  torus(group, copper, 7.9, 0.42, [0, -45.3, 0], [Math.PI / 2, 0, 0], 6, 16);
  torus(group, frame, 4.1, 0.65, [0, -25.2, 0], [Math.PI / 2, 0, 0], 6, 14);
  for (const side of [-1, 1]) strut(group, copper, [side * 4.0, -27.4, 0], [side * 6.4, -37.6, 0], 0.5, 6);
  for (const side of [-1, 1]) strut(group, metal, [side * 8.6, -34.0, 1.8], [side * 8.6, -14.0, 1.8], 0.36, 6);

  // ---- panel relief -----------------------------------------------------------------------------
  panel_relief(group, dark, [26, 42, 0.5], [0, 5, 3.4], [0, 0, 0], { cols: 3, rows: 4, thickness: 0.6, depth: 0.5 });
  for (const side of [-1, 1]) {
    panel_relief(group, frame, [15, 42, 0.5], [side * 14.5, -13, 1.0], [0, 0, 0], { cols: 2, rows: 3, thickness: 0.55, depth: 0.45 });
  }

  // ---- RCS: the four quad positions the sim lights ----------------------------------------------
  thrusters(group, 12, 14, -24, rcs);
  for (const side of [-1, 1]) for (const y of [14, -24]) {
    strut(group, metal, [side * 10.4, y, 2], [side * 12.4, y, 2], 0.42, 6);
  }

  // ---- landing legs: a tricycle, which is what a nose-heavy interceptor wants ------------------
  leg(group, -9.5, -26, -1);
  leg(group, 9.5, -26, 1);
  leg(group, 0, 30, 1);

  // ---- berths -----------------------------------------------------------------------------------
  // Aft berth either side of the single bell, and a forward berth at the prow.
  dock(group, 'A', [0, -38, 0], Math.PI, 'M');
  strut(group, metal, [-7.4, -36.5, 0], [7.4, -36.5, 0], 0.5, 6);
  dock(group, 'fwd', [0, 48, 3], 0, 'S');

  // Portholes on the fuselage flanks, and the sensor blister the recon role wants.
  for (const side of [-1, 1]) for (const y of [2, 14]) {
    const port = new THREE.Mesh(new THREE.CylinderGeometry(0.9, 0.9, 0.4, 10), glass);
    port.position.set(side * 5.9, y, 6); port.rotation.z = Math.PI / 2; group.add(port);
  }
  const blister = new THREE.Mesh(new THREE.SphereGeometry(1.7, 12, 8), dark);
  blister.position.set(0, -16, 10.2); group.add(blister);
  return group;
}

/** A lighter three-segment leg: the needle lands on skids rather than a freighter's gear. */
function leg(root: THREE.Object3D, x: number, y: number, side: number): void {
  const hip = [x, y, -3.4];
  const knee = [x + side * 2.0, y - 0.8, -7.0];
  const foot = [x + side * 1.0, y - 0.2, -10.4];
  bevelled(root, dark, [2.8, 3.6, 2.2], hip, [0, 0, 0], { radius: 0.35, segments: 1 });
  strut(root, metal, hip, knee, 0.8, 6);
  strut(root, metal, knee, foot, 0.65, 6);
  strut(root, copper, [hip[0], hip[1] - 1.2, hip[2] + 0.4], [knee[0] - side * 0.5, knee[1] + 0.4, knee[2] + 0.6], 0.32, 6);
  lathe(root, dark, [[1.4, 0.2], [2.2, 0], [2.4, -0.45], [2.0, -0.9]], 12, [foot[0], foot[1], foot[2]]);
}

export const maps: Maps = (() => {
  const kit = standard_maps({
    seed: 57, panel: 0.04, rivets: 0.02, seams: 2, wear: 0.3, grime: 0.18, scorch: 2,
    stencils: [['N-3', 0.1, 0.15], ['RECON', 0.36, 0.62]],
    hazard: { x: 0.05, y: 0.8, w: 0.9, h: 0.12, angle: Math.PI / 4 },
  });
  return {
    normal: kit.normal,
    roughness: (ctx, size) => {
      kit.roughness?.(ctx, size);
      each_tile(ctx, size, (tile) => {
        // Polished fields: the interceptor's blade plates and the canopy glazing. Dark is smooth.
        tile.ctx.globalAlpha = 0.35;
        tile.ctx.fillStyle = [0, 54, 255];
        tile.ctx.fillRect(0.09 * tile.w, 0.1 * tile.h, 0.3 * tile.w, 0.22 * tile.h);
        tile.ctx.fillRect(0.62 * tile.w, 0.58 * tile.h, 0.28 * tile.w, 0.24 * tile.h);
        tile.ctx.globalAlpha = 1;
      });
    },
    albedo: (ctx, size) => {
      kit.albedo?.(ctx, size);
      each_tile(ctx, size, (tile) => {
        stencil_text(tile, 'NO STEP', { x: 0.6, y: 0.18, scale: 0.02 });
        stencil_text(tile, 'N-3', { x: 0.16, y: 0.46, scale: 0.028, mirror: true });
        hazard_stripes(tile, { x: 0.06, y: 0.44, w: 0.32, h: 0.09 }, { pitch: 0.036, angle: -Math.PI / 4 });
        scorch(tile, { x: 0.5, y: 0.88, radius: 0.14, seed: 91, alpha: 0.65 });
      });
    },
  };
})();

export const meta = { name: 'needle', scale: 1.3, collider: { halfLength: 62, halfWidth: 27 } };
