// Wayfarer: the ring station, re-authored at F8 (PLAN-03 §7.1). The plan-01 envelope is kept — the
// pressure torus is still 73 m radius on an 8 m tube, the two arm trusses still reach ±147 m and the
// whole thing still spins about +Z — so the berths stay where the docking gate expects them. What is
// new: a hub perpendicular to the ring plane, sixteen rim modules, a window band, cross-braced
// spokes, four berths on the ring itself (A..D) plus the arm-tip berth for heavy hulls (E), approach
// lighting on every corridor, and panel relief.
//
// Axes: nose +Y, dorsal +Z, starboard +X, metres. The ring lies in the flight plane (XY).
import * as THREE from 'three';
import {
  armor, black, copper, dark, deckPlate, frame, glass, hullPaint, insulation, lightArmor, metal,
  bevelled, box, dock, dome, each_tile, hazard_stripes, lamps, lathe, panel_relief,
  scorch, solarCell, standard_maps, stencil_text, strut, torus, tube, type Maps,
} from './prims';

const TAU = Math.PI * 2;
const RING = 73;

/** The berth face plate: its own hazard material, so `meta.untextured` can leave its paint alone. */
const berthPaint = new THREE.MeshStandardMaterial({ color: '#c19a3e', roughness: 0.8, metalness: 0.2 });
berthPaint.name = 'berthPaint';

/**
 * One berth: a cradle standing off the ring's outer skin, six latches around the collar, a
 * hazard-striped face, the `dock.<id>` anchor at the mating plane and a run of approach lamps out
 * along the corridor. `site` is placed in the ring plane and rotated so its local +Y is the
 * outward normal.
 */
function berth(root: THREE.Object3D, id: string, x: number, y: number, z: number, rotation: number, cls: string): void {
  const site = new THREE.Group();
  site.position.set(x, y, z);
  site.rotation.z = rotation;
  root.add(site);
  lathe(site, frame, [[8.6, -2.0], [9.4, 0], [9.4, 3.0], [8.0, 5.0]], 20, [0, 7.0, 0]);
  torus(site, copper, 8.2, 0.6, [0, 12.0, 0], [Math.PI / 2, 0, 0], 6, 18);
  for (let i = 0; i < 6; i++) {
    const angle = (i / 6) * TAU;
    bevelled(site, metal, [2.4, 3.4, 2.4], [Math.cos(angle) * 9.2, 13.5, Math.sin(angle) * 9.2], [0, -angle, 0], { radius: 0.4, segments: 1 });
  }
  box(site, berthPaint, [17, 0.5, 0.5], [0, 16.4, 0]);
  // The port sits on the ring's outer equator (73 + 8), so a corridor from outside meets the skin.
  dock(site, id, [0, 8.0, 0], 0, cls);
  // Approach lighting stays close to the cradle: the station spins, so a long lamp run out into
  // space would sweep through the sky rather than mark a fixed corridor.
  lamps(site, '#b7dfdd', [[-6, 14, 0], [6, 14, 0], [-6, 19, 0], [6, 19, 0]], 0.8, 6);
  // Two red lamps mark the cradle mouth: the "hold here" pair a pilot lines up on.
  lamps(site, '#e6554d', [[-7.4, 15.5, 0], [7.4, 15.5, 0]], 0.7, 6);
}

/** The berth face plate is a named hazard material so the maps leave its paint alone. */
function hazardPaintFace(): THREE.Material { return new THREE.MeshStandardMaterial({ color: '#c19a3e', roughness: 0.8, metalness: 0.2 }); }

/** One arm: the truss out to the solar wings, in the ring plane, with its own relief and bus run. */
function arm(root: THREE.Object3D, side: number): void {
  const site = new THREE.Group();
  site.position.set(side * 72, 0, -12);
  root.add(site);
  bevelled(site, metal, [150, 3, 3], [0, 0, 0], [0, 0, 0], { radius: 0.5, segments: 1 });
  for (let i = 0; i < 7; i++) box(site, black, [120, 0.6, 0.6], [0, -36 + i * 12, 0.6]);
  for (let i = 0; i < 5; i++) strut(site, frame, [-60 + i * 30, -1.5, 0], [-60 + i * 30, 1.5, 0], 0.4, 6);
  strut(site, copper, [side * -60, 1.6, 1.2], [side * 60, 1.6, 1.2], 0.35, 6);
  panel_relief(site, deckPlate, [120, 6, 0.5], [0, 0, 1.6], [0, 0, 0], { cols: 4, rows: 1, thickness: 0.6, depth: 0.4 });
  // Solar wing on a yoke at the outboard end: frame, eight string bays, and the feed run.
  const wing = new THREE.Group();
  wing.position.set(side * 40, 0, 2);
  site.add(wing);
  bevelled(wing, frame, [50, 90, 1.0], [0, 0, 0], [0, 0, 0], { radius: 0.4, segments: 1 });
  for (let i = 0; i < 6; i++) bevelled(wing, solarCell, [46, 13, 1.6], [0, -37.5 + i * 15, 2.0], [0, 0, 0], { radius: 0.2, segments: 1 });
  for (let i = 0; i < 5; i++) box(wing, metal, [50, 0.6, 0.6], [0, -30 + i * 15, 3.1]);
  strut(wing, copper, [0, 0, 3.2], [0, 44, 3.2], 0.28, 6);
}

export function build(): THREE.Object3D {
  const group = new THREE.Group(); group.name = 'station';

  // ---- the habitat ring -------------------------------------------------------------------------
  torus(group, metal, RING, 8, [0, 0, 0], [0, 0, 0], 10, 80);
  // Outer armour band and the glazing band between the plates: two thin rings on the outer equator.
  torus(group, dark, RING + 8, 1.6, [0, 0, 0], [0, 0, 0], 6, 80);
  torus(group, glass, RING + 8, 1.0, [0, 0, 0], [0, 0, 0], 5, 72);
  // Inner keel the spokes land on.
  torus(group, copper, RING - 8, 1.2, [0, 0, 0], [0, 0, 0], 6, 72);
  // Sixteen rim modules: workshops, tank farms and airlocks, unevenly patched.
  for (let i = 0; i < 16; i++) {
    const angle = (i / 16) * TAU;
    const x = Math.sin(angle) * RING, y = Math.cos(angle) * RING;
    bevelled(group, i % 4 === 0 ? lightArmor : dark, [15, 18, 14], [x, y, 0], [0, 0, -angle], { radius: 0.9, segments: 1 });
    if (i % 2 === 0) strut(group, metal, [x * 0.88, y * 0.88, 0], [x, y, 0], 0.6, 6);
    if (i % 8 === 1) cylinder_stack(group, x, y, -angle);
  }
  // Spokes: eight radial pairs, cross-braced, landing on the hub collar.
  for (let i = 0; i < 8; i++) {
    const angle = (i / 8) * TAU + Math.PI / 8;
    const dx = Math.sin(angle), dy = Math.cos(angle);
    const inner = 24, outer = RING - 7;
    for (const side of [-3.2, 3.2]) {
      const ox = -dy * side, oy = dx * side;
      strut(group, frame, [dx * inner + ox, dy * inner + oy, 0], [dx * outer + ox, dy * outer + oy, 0], 0.75, 6);
    }
    strut(group, frame, [dx * inner, dy * inner, 0], [dx * (RING - 30), dy * (RING - 30), 0], 0.6, 6);
    strut(group, metal, [dx * (inner + 6) - dy * 3.2, dy * (inner + 6) + dx * 3.2, 0], [dx * (outer - 8) + dy * 3.2, dy * (outer - 8) - dx * 3.2, 0], 0.4, 6);
  }

  // ---- hub, perpendicular to the ring plane -----------------------------------------------------
  lathe(group, armor, [[0, -34], [13, -31], [20, -25], [22, -14], [22, 14], [20, 25], [13, 31], [0, 34]], 24, [0, 0, 0], [Math.PI / 2, 0, 0]);
  dome(group, hullPaint, 22, [0, 0, 34], [Math.PI / 2, 0, 0], 24);
  dome(group, hullPaint, 22, [0, 0, -34], [-Math.PI / 2, 0, 0], 24);
  torus(group, frame, 22.2, 1.2, [0, 0, 0], [Math.PI / 2, 0, 0], 6, 24);
  // Hub collar with the spoke sockets, and the radiator stack on the shadow side.
  tube(group, dark, 24, 24, 4, [0, 0, 0], 20, [Math.PI / 2, 0, 0]);
  for (let i = 0; i < 4; i++) {
    const angle = (i / 4) * TAU + Math.PI / 4;
    const x = Math.sin(angle) * 30, y = Math.cos(angle) * 30;
    bevelled(group, insulation, [10, 10, 26], [x, y, -30], [0, 0, -angle], { radius: 0.6, segments: 1 });
    strut(group, copper, [x, y, -18], [x, y, -42], 0.4, 6);
  }
  // A construction gantry on the lower face: the station was built in pieces.
  for (let i = 0; i < 3; i++) strut(group, frame, [-24 + i * 24, 0, -40], [0, 0, -46], 0.5, 6);
  box(group, deckPlate, [40, 3, 8], [0, 0, -46]);
  // Antenna farm on the upper face.
  for (const at of [[-14, -12], [14, -12], [0, 16]]) {
    strut(group, metal, [at[0], at[1], 34], [at[0], at[1], 48], 0.5, 6);
    dome(group, metal, 2.6, [at[0], at[1], 48], [Math.PI / 2, 0, 0], 10);
  }

  // ---- arms and wings ---------------------------------------------------------------------------
  arm(group, 1);
  arm(group, -1);

  // ---- berths: four on the ring, one at the arm tip ---------------------------------------------
  berth(group, 'A', 0, RING, 0, 0, 'M');
  berth(group, 'B', RING, 0, 0, -Math.PI / 2, 'M');
  berth(group, 'C', 0, -RING, 0, Math.PI, 'M');
  berth(group, 'D', -RING, 0, 0, Math.PI / 2, 'L');
  berth(group, 'E', 147, 0, -12, -Math.PI / 2, 'L');

  // ---- approach lighting: the ring's own position lights, then the arm-tip corridor ---------------
  const ring_lamps: number[][] = [];
  for (let i = 0; i < 16; i++) {
    const angle = (i / 16) * TAU + Math.PI / 16;
    ring_lamps.push([Math.sin(angle) * (RING + 8.6), Math.cos(angle) * (RING + 8.6), 0]);
  }
  lamps(group, '#b7dfdd', ring_lamps, 0.7, 6);
  lamps(group, '#efe4bb', [[0, RING + 8, 10], [0, -RING - 8, 10]], 0.8, 6);

  return group;
}

/** A cluster of tanks and a vent stack on a rim module: what makes a ring read as inhabited. */
function cylinder_stack(root: THREE.Object3D, x: number, y: number, angle: number): void {
  const site = new THREE.Group();
  site.position.set(x, y, 0);
  site.rotation.z = -angle;
  root.add(site);
  for (const side of [-1, 1]) {
    lathe(site, insulation, [[2.4, 0], [2.7, 1.2], [2.7, 10.4], [2.2, 11.6], [0.8, 12.2]], 14, [side * 5.2, 0, 0], [0, 0, side * 0.08]);
  }
  tube(site, metal, 0.9, 0.9, 7.0, [0, 0, 1.0], 8);
  box(site, dark, [12, 2.4, 1.2], [0, 0, -4.4]);
  lamps(site, '#efbf7a', [[0, 0, 8.4]], 0.6, 6);
}

export const maps: Maps = (() => {
  const kit = standard_maps({
    seed: 105, panel: 0.035, rivets: 0.018, seams: 5, wear: 0.3, grime: 0.28, scorch: 3,
    damage: 0.12,
    stencils: [['WF-01', 0.1, 0.14], ['WAYFARER', 0.26, 0.58]],
    hazard: { x: 0.05, y: 0.8, w: 0.9, h: 0.13, angle: Math.PI / 4 },
  });
  return {
    normal: kit.normal,
    roughness: kit.roughness,
    albedo: (ctx, size) => {
      kit.albedo?.(ctx, size);
      each_tile(ctx, size, (tile) => {
        // Ring-module numbering and the berth warnings: a station is a signed place.
        stencil_text(tile, 'RING 4', { x: 0.58, y: 0.16, scale: 0.02 });
        stencil_text(tile, 'BERTH', { x: 0.12, y: 0.42, scale: 0.024, mirror: true });
        hazard_stripes(tile, { x: 0.06, y: 0.44, w: 0.3, h: 0.09 }, { pitch: 0.036, angle: -Math.PI / 4 });
        scorch(tile, { x: 0.72, y: 0.66, radius: 0.13, seed: 12, alpha: 0.6 });
      });
    },
  };
})();

export const meta = { name: 'station', scale: 1, collider: 'auto', untextured: ['glass'] };
