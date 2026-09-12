// Mule: heavy salvage tug, re-authored to the plan-04 spec table (s2.2, A1).
// Hull 58 m, proportions as drawn 2.45 : 1 : 0.70, so 58 x 23.7 x 16.6. The image wins: the
// V-open three-segment grapple arms, the towing yoke between them, the glass-segmented cab, the
// open rail chassis with structure visible through it, six clamped freight pods, deployed
// radiators abaft midships, the four-bell square cluster, and the dorsal turret forward of the
// radiators. The collider below reproduces the plan's own table - a single box would span the
// open chassis and register hits in gaps a Needle flies through, which is exactly why the
// sidecar's compound shapes are what the sim collides on.
//
// Axes as always: nose +Y, dorsal +Z, starboard +X, metres.
import * as THREE from 'three';
import {
  armor, black, copper, dark, frame, glass, lightArmor, metal, teal, ochre,
  bevelled, box, cylinder, dock, effect_cone, each_tile, hazard_stripes, hp, lathe, panel_relief,
  plate, scorch, standard_maps, stencil_text, strut, torus, tube, type Maps,
} from './prims';

// Plan frame: L = 58, half-width 11.85, deck +/-8.3.
const L = 58;
const HW = 11.85;
const DECK = 8.3;
// Along: fraction of length from the nose -> model y (nose +29, stern -29).
const at = (f: number) => L / 2 - f * L;

/** One grapple arm: three folding segments, open in the reference, ending in a claw pair. */
function grapple_arm(root: THREE.Group, side: number): void {
  const shoulder = [side * 5.5, at(0.05), 0];
  const elbow = [side * 10.5, at(0.10), 0];
  const wrist = [side * 12.4, at(0.15), 0];
  const tip = [side * 10.2, at(0.185), 0];
  cylinder(root, copper, 1.1, 1.1, 1.6, shoulder, 12);
  strut(root, frame, shoulder, elbow, 0.85, 10);
  cylinder(root, copper, 0.9, 0.9, 1.4, elbow, 10);
  strut(root, frame, elbow, wrist, 0.7, 10);
  cylinder(root, copper, 0.7, 0.7, 1.1, wrist, 10);
  // Claw pair, open: two dark talons splayed from the wrist knuckle.
  strut(root, dark, wrist, [tip[0] - side * 1.4, tip[1], 0.6], 0.5, 8);
  strut(root, dark, wrist, [tip[0] - side * 1.4, tip[1], -0.6], 0.5, 8);
  strut(root, dark, wrist, [tip[0] + side * 0.6, tip[1] - 0.4, 0], 0.5, 8);
}

export function build(): THREE.Object3D {
  const group = new THREE.Group(); group.name = 'mule';
  const flames: THREE.Mesh[] = [];

  // ---- towing end: grapple arms and the yoke between them (0.00-0.16) -----------------------------
  for (const side of [-1, 1]) grapple_arm(group, side);
  // Yoke: centre bar with a tow roller and hook, gibbeted between the arm roots.
  box(group, dark, [2 * 0.30 * HW, 2.2, 2.2], [0, at(0.10), 0]);
  cylinder(group, metal, 1.0, 1.0, 5.6, [0, at(0.10), 0], 12);
  box(group, copper, [1.1, 1.6, 1.1], [0, at(0.135), 0]);

  // ---- cab (0.16-0.26): faceted command block with segmented glass ----------------------------------
  plate(group, [
    [-3.6, at(0.16)], [3.6, at(0.16)], [5.33, at(0.21)], [5.33, at(0.26)],
    [-5.33, at(0.26)], [-5.33, at(0.21)],
  ], 7.5, -1.2, armor);
  // Segmented canopy: three glass panes in a frame, facing forward-down like the sheet.
  box(group, glass, [7.6, 0.4, 3.0], [0, at(0.175), 2.6]);
  for (const x of [-2.6, 0, 2.6]) box(group, frame, [0.5, 0.5, 3.2], [x, at(0.175), 2.6]);
  box(group, teal, [10.9, 0.7, 0.3], [0, at(0.24), 5.6]);
  box(group, ochre, [10.9, 0.55, 0.3], [0, at(0.225), 5.6]);

  // ---- open rail chassis (0.26-0.80): two rails, structure visible through it -------------------------
  const railFore = at(0.26), railAft = at(0.80), railX = 0.35 * HW;
  for (const side of [-1, 1]) {
    box(group, dark, [1.5, railFore - railAft, 2.2], [side * railX, (railFore + railAft) / 2, 0]);
  }
  for (let i = 0; i < 8; i++) {
    const y = railFore - (i + 0.5) * ((railFore - railAft) / 8);
    box(group, frame, [2 * railX, 0.8, 1.4], [0, y, 0]);
  }
  // Keel tank and pipe run between the rails: the structure you see through the chassis.
  tube(group, metal, 1.5, 1.5, railFore - railAft - 6, [0, (railFore + railAft) / 2, -2.4], 12);
  tube(group, copper, 0.5, 0.5, railFore - railAft - 6, [1.9, (railFore + railAft) / 2, -2.4], 8);

  // ---- cargo pods, three per side (0.32, 0.48, 0.64 at +/-0.80) --------------------------------------
  for (const side of [-1, 1]) for (const f of [0.32, 0.48, 0.64]) {
    const x = side * 0.80 * HW, y = at(f);
    bevelled(group, lightArmor, [4.4, 6.6, 5.2], [x, y, 0], [0, 0, 0], { radius: 0.4, segments: 1 });
    // Clamp arms to the rails: the pod hangs off the chassis, not through it.
    strut(group, frame, [x - side * 2.2, y + 2.2, 0], [side * railX, y + 2.2, 0], 0.5, 8);
    strut(group, frame, [x - side * 2.2, y - 2.2, 0], [side * railX, y - 2.2, 0], 0.5, 8);
    // Identity stripes: teal and ochre bands like the sheet's pods.
    box(group, teal, [0.3, 5.4, 4.4], [x + side * 2.2, y, 0]);
    box(group, ochre, [0.3, 5.4, 0.9], [x + side * 2.2, y + 1.6, 0]);
    box(group, dark, [3.6, 0.5, 4.2], [x, y, 2.8]);
  }

  // ---- dorsal turret (0.30, centreline) ------------------------------------------------------------------
  torus(group, frame, 1.05, 0.3, [0, at(0.30), DECK + 0.4], [Math.PI / 2, 0, 0], 8, 20);
  cylinder(group, dark, 0.95, 1.05, 0.7, [0, at(0.30), DECK + 0.1], 20);
  hp(group, 'pdc.dorsal', [0, at(0.30), DECK + 0.5]);

  // ---- radiator panels (0.62-0.80, +/-0.55), deployed ------------------------------------------------------
  for (const side of [-1, 1]) {
    const x = side * 0.55 * HW;
    // Hinge root and actuator at the deck.
    cylinder(group, copper, 0.55, 0.55, 2.2, [x, at(0.66), DECK - 0.4], 10);
    box(group, dark, [6.0, 3.0, 0.4], [x + side * 1.3, at(0.71), DECK + 2.6]);
    for (let i = -1; i <= 1; i++) box(group, frame, [0.5, 3.0, 0.55], [x + side * 1.3 + i * 1.9, at(0.71), DECK + 2.6]);
  }

  // ---- drive bells, 2x2 square cluster (0.80-1.00): asset 11's proportions, scaled ---------------------
  for (const sx of [-1, 1]) for (const sz of [-1, 1]) {
    const x = sx * 0.20 * HW, z = sz * 2.5;
    lathe(group, metal, [
      [1.30, 0.00], [1.20, 0.65], [1.02, 1.72], [0.90, 2.80], [0.80, 3.70],
      [0.74, 4.60], [0.72, 5.20], [0.86, 5.50],
    ], 18, [x, at(1.0), z]);
    torus(group, copper, 1.32, 0.14, [x, at(1.0) + 0.06, z], [Math.PI / 2, 0, 0], 8, 20);
    effect_cone(group, flames, { name: 'flame', radius: 1.25, length: 14.0,
      pos: [x, at(1.0) - 7.6, z] });
  }
  // Stern frame the cluster bolts to.
  box(group, frame, [2 * 0.20 * HW + 3.4, 2.0, 7.4], [0, at(0.815), 0]);

  // ---- RCS corners: cab shoulders forward, drive frame abaft ------------------------------------------------
  for (const side of [-1, 1]) for (const [y, z] of [[at(0.20), 2.0], [at(0.86), 0.0]]) {
    const x = side * (y > 0 ? 8.2 : 5.2);
    box(group, dark, [1.3, 1.3, 1.3], [x, y, z]);
    effect_cone(group, flames, { name: 'rcs-jet', radius: 0.55, length: 4.5, pos: [x + side * 3.1, y, z], rot: [0, 0, -side * Math.PI / 2] });
  }

  // ---- berth: the tug docks stern-first through the drive frame ------------------------------------------------
  dock(group, 'A', [0, at(1.0), 0], Math.PI, 'M');

  // ---- landing legs under the rails --------------------------------------------------------------------------
  for (const side of [-1, 1]) for (const f of [0.40, 0.60]) {
    strut(group, metal, [side * railX, at(f), -1.4], [side * (railX + 1.8), at(f) - 0.9, -DECK - 2.6], 0.4, 8);
    box(group, dark, [2.4, 1.7, 0.5], [side * (railX + 1.8), at(f) - 0.9, -DECK - 2.8]);
  }

  // ---- panel relief on the pods' outboard faces ------------------------------------------------------------------
  for (const side of [-1, 1]) for (const f of [0.32, 0.48, 0.64]) {
    panel_relief(group, armor, [4.2, 6.2, 0.4], [side * (0.80 * HW + 2.3), at(f), 0], [0, 0, 0], { cols: 2, rows: 3, thickness: 0.5, depth: 0.4 });
  }
  return group;
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

// The plan's own collider table (s2.2): the spine box is the authored reference; the sidecar's
// compound shapes are derived from the geometry and are what the sim collides on.
export const meta = { name: 'mule', scale: 1.3, collider: { halfLength: 29, halfWidth: 11.85 } };
