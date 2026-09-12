// Kestrel: armoured patrol corvette, re-authored at F8 (PLAN-03 §7.1). The plan-01 envelope is kept
// deliberately: the outer plate outlines, the four RCS quads and the two drive bells stay where they
// were, so the compound collider the handling is tuned to survives the re-author. What is new is the
// body those plates armour — a lofted fuselage, a smooth canopy, lathed bells with gimbal rings,
// frame ribs, panel relief as geometry, landing legs — and the procedural maps.
//
// Axes as always: nose +Y, dorsal +Z, starboard +X, metres.
import * as THREE from 'three';
import {
  armor, black, copper, dark, frame, glass, lightArmor, metal, teal,
  bevelled, box, dock, drive, each_tile, hazard_stripes, lathe, panel_relief,
  plate, scorch, standard_maps, stencil_text, strut, thrusters, torus, tube, type Maps,
} from './prims';

/** Canopy glass: tinted, emissive, and deliberately left in the mapped set so the panel seams read
 *  as frame lines on the blister. Small portholes keep the shared `glass` and stay clean. */
const canopy = new THREE.MeshStandardMaterial({
  color: '#2c5f70', roughness: 0.14, metalness: 0.7, emissive: '#2b5f6d', emissiveIntensity: 0.5,
});
canopy.name = 'canopy';

/** A three-segment leg with a footpad: hip bracket, upper arm, knee, lower arm, ram, pad. */
function landing_leg(root: THREE.Object3D, x: number, y: number, side: number): void {
  const hip = [x, y, -4.4];
  const knee = [x + side * 2.6, y - 1.2, -8.2];
  const foot = [x + side * 1.2, y - 0.4, -12.4];
  bevelled(root, dark, [3.6, 4.4, 2.6], hip, [0, 0, 0], { radius: 0.4, segments: 1 });
  strut(root, metal, hip, knee, 1.05, 8);
  strut(root, metal, knee, foot, 0.9, 8);
  // Footpad: a shallow bell that spreads the load and reads flat from the side.
  lathe(root, dark, [[1.6, 0.2], [2.6, 0], [2.8, -0.5], [2.4, -1.0]], 12, [foot[0], foot[1], foot[2]]);
}

export function build(): THREE.Object3D {
  const group = new THREE.Group(); group.name = 'kestrel';
  const flames: THREE.Mesh[] = [], rcs: THREE.Mesh[] = [];

  // ---- the armoured frame the collider was fitted to: outlines verbatim ------------------------
  // Central keel plate: the widest single cluster in the compound collider.
  plate(group, [[-9,-32],[-25,-20],[-25,-3],[-14,17],[-10,39],[-3,39],[-3,29],[3,29],[3,39],[10,39],[14,17],[25,-3],[25,-20],[9,-32]], 7, -4, dark);
  for (const side of [-1, 1]) {
    plate(group, [[side*7,-25],[side*22,-17],[side*21,-2],[side*11,28],[side*5,22]], 5, 3, armor);
    // Shoulder blade: the same box footprint, bevelled, with a raised edge beam along it.
    bevelled(group, teal, [3, 23, 0.8], [side * 12, 0, 9], [0, 0, -side * 0.25], { radius: 0.35, segments: 1 });
    strut(group, metal, [side * 11.2, -10.6, 9.2], [side * 12.8, 10.4, 9.2], 0.42, 8);
    // Side hull: now a lofted pod rather than a prism, same envelope (x 16 ± 5, y -19 ± 15.5).
    lathe(group, lightArmor, [[0, -15.5], [3.4, -14.4], [5, -10.6], [5, 10.6], [3.4, 14.4], [0, 15.5]], 16, [side * 16, -19, 1]);
    torus(group, frame, 5.15, 0.3, [side * 16, -18.5, 1], [Math.PI / 2, 0, 0], 6, 16);
    // Vent louvres: cheap slats, read at any range, cost a box each.
    for (let i = 0; i < 4; i++) box(group, black, [7, 1.1, 1], [side * 16, -24 + i * 3, 5.8]);
    for (let i = 0; i < 3; i++) box(group, dark, [1.2, 0.6, 0.5], [side * 20.4, -27 + i * 6.5, 0.4]);
    drive(group, side * 16, -33, 5.3, flames);
  }

  // ---- the body the plates armour ---------------------------------------------------------------
  // Lofted fuselage: smooth, single surface, dorsal spine to ventral keel.
  lathe(group, lightArmor, [[0, -20.5], [3.6, -19.6], [5.4, -16], [6.2, -9], [6.2, 10], [5.4, 16], [3.6, 19.6], [0, 20.5]], 20, [0, 3, 8]);
  // Frame ribs: what makes a hull read as built in sections instead of moulded.
  for (const y of [-8, 8]) torus(group, frame, 6.32, 0.34, [0, y + 3, 8], [Math.PI / 2, 0, 0], 6, 14);
  // Dorsal spine fairing and its equipment run.
  tube(group, dark, 2.6, 2.6, 34, [0, 2, 14.4], 14);
  for (let i = 0; i < 4; i++) box(group, metal, [3.4, 1.8, 1.4], [0, -6 + i * 8.6, 15.6]);
  // Prow: sensor boom, whip antennas and the forward hardpoint.
  lathe(group, metal, [[1.5, 0], [1.5, 8], [1.0, 10.4], [0.6, 12]], 14, [0, 23.0, 8]);
  strut(group, copper, [0, 33.0, 8], [0, 38.6, 8], 0.32, 8);
  for (const side of [-1, 1]) strut(group, metal, [side * 3.4, 26.0, 10.4], [side * 5.6, 34.6, 11.4], 0.22, 6);

  // ---- canopy -----------------------------------------------------------------------------------
  lathe(group, canopy, [[0, -6.75], [3.2, -6.0], [4.0, -2.6], [4.0, 2.6], [3.2, 6.0], [0, 6.75]], 18, [0, 17.5, 12]);
  // Canopy frame: two arch ribs and a centre rail, so the glass reads as glazing.
  for (const y of [13.6, 21.4]) { torus(group, dark, 4.05, 0.3, [0, y, 12], [Math.PI / 2, 0, 0], 6, 18); }
  canopy_rail(group);

  // ---- panel relief as geometry -----------------------------------------------------------------
  // Dorsal deck: the plate's own +Z face gets a real raised panel grid.
  panel_relief(group, frame, [44, 60, 0.5], [0, 3.5, 3.4], [0, 0, 0], { cols: 3, rows: 4, thickness: 0.7, depth: 0.55 });
  // Outboard flanks of the side plates: relief on the plates' own dorsal face, not rotated off the hull.
  for (const side of [-1, 1]) {
    panel_relief(group, armor, [16, 44, 0.5], [side * 14, -4, 8.2], [0, 0, 0], { cols: 2, rows: 3, thickness: 0.6, depth: 0.5 });
    // Chines: rounded structural edge where the plate meets the fuselage.
    strut(group, metal, [side * 6.4, -29, 3.2], [side * 6.4, 22, 3.2], 0.6, 6);
  }

  // ---- engine bay -------------------------------------------------------------------------------
  for (const side of [-1, 1]) {
    // Bell skirt over the drive (a smooth surface of revolution, wider than the primitives it wraps).
    lathe(group, metal, [[6.1, -8.4], [5.6, -6.0], [4.9, -2.0], [4.3, 2.4], [3.6, 6.6], [3.1, 10.4]], 20, [side * 16, -33, 0]);
    torus(group, copper, 6.05, 0.32, [side * 16, -41.2, 0], [Math.PI / 2, 0, 0], 6, 20);
    // Gimbal ring and its two actuators: the bell is a thing that moves.
    torus(group, frame, 3.5, 0.55, [side * 16, -22.4, 0], [Math.PI / 2, 0, 0], 6, 12);
    for (const link of [-1, 1]) strut(group, copper, [side * 16 + link * 3.4, -24.6, 0], [side * 16 + link * 5.2, -35.0, 0], 0.42, 8);
    strut(group, copper, [side * 20.4, -26, 3.4], [side * 12.4, -26, 3.4], 0.3, 6);
    strut(group, copper, [side * 20.4, -26, -3.4], [side * 12.4, -26, -3.4], 0.3, 6);
    // Plumbing run to the aft frame.
    strut(group, metal, [side * 21.0, -30.5, 1.6], [side * 21.0, -8.0, 1.6], 0.34, 6);
    strut(group, frame, [side * 9.0, -33.5, -2.0], [side * 22.0, -33.5, -2.0], 0.4, 6);
  }
  // Radiator blades, swept back from the shoulder.
  for (const side of [-1, 1]) {
    bevelled(group, dark, [0.5, 18, 7.5], [side * 22.6, -12, 8.4], [0, 0, side * 0.22], { radius: 0.25, segments: 1 });
    for (let i = 0; i < 2; i++) box(group, copper, [0.7, 0.4, 7.0], [side * 22.9, -15 + i * 6.0, 8.4]);
  }

  // ---- RCS: the same four quad positions the sim lights -------------------------------------------
  thrusters(group, 24, 5, -22, rcs);
  for (const side of [-1, 1]) for (const y of [5, -22]) {
    strut(group, metal, [side * 20.4, y, 2], [side * 24.0, y, 2], 0.5, 8);
  }

  // ---- landing legs, tucked under the keel ------------------------------------------------------
  for (const side of [-1, 1]) { landing_leg(group, side * 15.0, -6.0, side); landing_leg(group, side * 14.0, 14.0, side); }

  // ---- berths ------------------------------------------------------------------------------------
  // Aft berth, between the bells: the one face a station can reach in the flight plane.
  dock(group, 'A', [0, -34, 0], Math.PI, 'M');
  torus(group, frame, 7.4, 0.5, [0, -30.5, 4], [Math.PI / 2, 0, 0], 8, 20);
  // Forward berth at the prow, normal +Y.
  dock(group, 'fwd', [0, 39.0, 8], 0, 'S');

  // Small clean portholes keep the shared glass and prove the untextured list.
  for (const side of [-1, 1]) for (const y of [2]) {
    const port = new THREE.Mesh(new THREE.CylinderGeometry(1.1, 1.1, 0.4, 12), glass);
    port.position.set(side * 5.9, y, 9.4); port.rotation.z = Math.PI / 2; group.add(port);
  }
  return group;
}

/** The canopy centre rail, split out only because it reads better named. */
function canopy_rail(root: THREE.Object3D): void {
  tube(root, dark, 0.32, 0.32, 13.2, [0, 17.5, 16.1], 8);
}

export const maps: Maps = (() => {
  const kit = standard_maps({
    seed: 12, panel: 0.05, rivets: 0.025, seams: 3, wear: 0.28, grime: 0.2, scorch: 2,
    stencils: [['K-7', 0.1, 0.16], ['KESTREL', 0.34, 0.66]],
    hazard: { x: 0.05, y: 0.82, w: 0.9, h: 0.12, angle: Math.PI / 3 },
  });
  return {
    normal: kit.normal,
    roughness: (ctx, size) => {
      kit.roughness?.(ctx, size);
      each_tile(ctx, size, (tile) => {
        // Polished fields: the deck plates kept waxed and the glass over the canopy. Dark in the
        // green channel is smooth, so this pulls those panels well under the hull's base roughness
        // while the wear speckle the kit drew stays bright and rough at the panel edges.
        tile.ctx.globalAlpha = 0.35;
        tile.ctx.fillStyle = [0, 58, 255];
        tile.ctx.fillRect(0.08 * tile.w, 0.08 * tile.h, 0.34 * tile.w, 0.26 * tile.h);
        tile.ctx.fillRect(0.58 * tile.w, 0.62 * tile.h, 0.3 * tile.w, 0.24 * tile.h);
        tile.ctx.globalAlpha = 1;
      });
    },
    albedo: (ctx, size) => {
      kit.albedo?.(ctx, size);
      each_tile(ctx, size, (tile) => {
        // Stencils this hull carries and the beacon does not: charge, rescue and the hull number.
        stencil_text(tile, 'RESCUE', { x: 0.62, y: 0.2, scale: 0.02 });
        stencil_text(tile, 'K-7', { x: 0.14, y: 0.44, scale: 0.03, mirror: true });
        hazard_stripes(tile, { x: 0.06, y: 0.42, w: 0.34, h: 0.1 }, { pitch: 0.04, angle: Math.PI / 4 });
        scorch(tile, { x: 0.5, y: 0.86, radius: 0.16, seed: 71, alpha: 0.7 });
      });
    },
  };
})();

// The legacy scalar box is the authored, gameplay-tuned table value (HULL_BOXES); the sidecar's
// compound `shapes` are always derived from the geometry and are what the sim actually collides on.
export const meta = { name: 'kestrel', scale: 1.3, collider: { halfLength: 59, halfWidth: 34 } };
