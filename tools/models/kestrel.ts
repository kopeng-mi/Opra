// Kestrel: light corvette, re-authored to the plan-04 spec table (s2.1, A1).
// Hull 46 m, plan-view proportions as drawn 3.1 : 1 : 0.62, so 46 x 14.8 x 9.2. The image wins
// everywhere a number below had to be chosen: the blunt ogival prow, the teal band abaft the
// ochre stripe, the two turret blisters flanking the open dorsal bay, the recessed radiator panel
// in the bay floor, the greebled machinery spine, the twin bells splayed outboard with RCS quads
// at all four hull corners, and no glass canopy - the bridge is plated over with sensor windows.
// Feature stations are fractions of length from the nose (0.0) to the stern (1.0); the collider
// below reproduces the plan's own table, and the sidecar's compound shapes are what the sim
// collides on. The legacy scalar box stays as the gameplay-tuned reference value.
//
// Axes as always: nose +Y, dorsal +Z, starboard +X, metres.
import * as THREE from 'three';
import {
  armor, black, copper, dark, frame, lightArmor, metal, teal, ochre,
  bevelled, box, cylinder, dock, effect_cone, each_tile, hazard_stripes, hp, lathe, panel_relief,
  plate, scorch, standard_maps, stencil_text, strut, torus, tube, type Maps,
} from './prims';

// Plan frame: L = 46, half-width 7.4, deck +/-4.6. All stations below are fractions of L.
const L = 46;
const HW = 7.4;
const DECK = 4.6;
// Along: fraction of length from the nose -> model y (nose +23, stern -23).
const at = (f: number) => L / 2 - f * L;

export function build(): THREE.Object3D {
  const group = new THREE.Group(); group.name = 'kestrel';
  const flames: THREE.Mesh[] = [];

  // ---- armoured hull: the plan silhouette extruded -------------------------------------------
  // Blunt ogival prow (0.00-0.18): a blunt +/-4.07 nose widening to full beam, then a slab-sided
  // midship, shoulders in to the drive bays. Ogival in plan, faceted in section via the deck.
  plate(group, [
    [-4.07, at(0.0)], [4.07, at(0.0)], [HW, at(0.18)], [HW, at(0.66)],
    [6.0, at(0.85)], [4.6, at(1.0)], [-4.6, at(1.0)], [-6.0, at(0.85)],
    [-HW, at(0.66)], [-HW, at(0.18)],
  ], 9.2, -DECK, armor);
  // Dorsal deck step: the armour plate's own top face gets a raised deck, which is also what
  // makes the section read faceted instead of slabbed.
  plate(group, [
    [-3.2, at(0.02)], [3.2, at(0.02)], [6.4, at(0.2)], [6.4, at(0.62)],
    [4.2, at(0.84)], [2.6, at(0.96)], [-2.6, at(0.96)], [-4.2, at(0.84)],
    [-6.4, at(0.62)], [-6.4, at(0.2)],
  ], 1.3, DECK, lightArmor);
  // Prow cheeks: faceted armour over the ogive, and the two sensor windows the reference shows
  // on the nose face (a plated-over bridge, no glasshouse).
  for (const side of [-1, 1]) {
    bevelled(group, dark, [3.4, 6.5, 2.6], [side * 3.6, at(0.07), 3.4], [0, 0, side * 0.18], { radius: 0.4, segments: 1 });
    box(group, black, [1.9, 0.5, 2.1], [side * 2.1, at(0.045), 1.4]);
  }

  // ---- identity marking: teal abaft ochre, full width -----------------------------------------
  // Ochre forward, teal aft of it, wrapped across the deck and down both flanks.
  box(group, ochre, [2 * HW - 0.4, 0.85, 0.35], [0, at(0.115), DECK + 0.65]);
  box(group, teal, [2 * HW - 0.4, 1.15, 0.35], [0, at(0.145), DECK + 0.65]);
  for (const side of [-1, 1]) {
    box(group, ochre, [0.35, 0.85, 2 * DECK - 0.6], [side * (HW - 0.1), at(0.115), 0]);
    box(group, teal, [0.35, 1.15, 2 * DECK - 0.6], [side * (HW - 0.1), at(0.145), 0]);
  }

  // ---- turret rings: the mounts the pdc_turret modules bolt to ----------------------------------
  // fwd (0.20, +0.25), aft (0.28, -0.25): staggered off the centreline so neither sits over the
  // bay. The ring sits proud on the hull, not recessed (s2.4), and the hardpoint is its centre.
  for (const [id, f, s] of [['pdc.fwd', 0.20, 0.25], ['pdc.aft', 0.28, -0.25]] as const) {
    const x = s * HW, y = at(f);
    torus(group, frame, 1.05, 0.3, [x, y, DECK + 0.9], [Math.PI / 2, 0, 0], 8, 20);
    cylinder(group, dark, 0.95, 1.05, 0.7, [x, y, DECK + 0.6], 20);
    hp(group, id, [x, y, DECK + 1.0]);
  }
  // Ventral ring at (0.42, 0.0): the keel blister the reference shows below midships.
  torus(group, frame, 1.05, 0.3, [0, at(0.42), -DECK - 0.9], [Math.PI / 2, 0, 0], 8, 20);
  cylinder(group, dark, 0.95, 1.05, 0.7, [0, at(0.42), -DECK - 0.6], 20);
  hp(group, 'pdc.ventral', [0, at(0.42), -DECK - 1.0]);

  // ---- open dorsal bay (0.30-0.62, +/-0.62) ------------------------------------------------------
  // Coaming walls, eight transverse rails, and the flat radiator panel recessed in the floor -
  // the reference's dark floor with its bright edge guides.
  const bayFore = at(0.30), bayAft = at(0.62), bayHalf = 0.62 * HW;
  for (const side of [-1, 1]) {
    box(group, frame, [0.55, bayFore - bayAft, 1.35], [side * bayHalf, (bayFore + bayAft) / 2, DECK + 0.6]);
  }
  box(group, dark, [2 * bayHalf - 0.6, bayFore - bayAft, 0.35], [0, (bayFore + bayAft) / 2, DECK - 0.6]);
  for (let i = 0; i < 8; i++) {
    const y = bayFore - (i + 0.5) * ((bayFore - bayAft) / 8);
    box(group, copper, [2 * bayHalf - 0.6, 0.22, 0.3], [0, y, DECK - 0.3]);
  }

  // ---- machinery spine (0.62-0.82, +/-0.55): exposed plumbing, greebled ------------------------------
  const spineFore = at(0.62), spineAft = at(0.82), spineHalf = 0.55 * HW;
  box(group, dark, [2 * spineHalf, spineFore - spineAft, 0.8], [0, (spineFore + spineAft) / 2, DECK + 0.25]);
  for (const x of [-2.9, -1.5, 0, 1.5, 2.9]) {
    tube(group, metal, 0.34, 0.34, spineFore - spineAft - 1.0, [x, (spineFore + spineAft) / 2, DECK + 0.75], 8);
  }
  for (const y of [spineFore - 3.4, spineFore - 7.6]) {
    box(group, metal, [2 * spineHalf - 0.6, 1.1, 0.9], [0, y, DECK + 0.8]);
    box(group, copper, [1.2, 0.7, 0.7], [2.2, y, DECK + 1.3]);
  }

  // ---- drive bells (0.82-1.00, +/-0.22): asset 11's proportions, scaled -----------------------------
  // 2.6 m mouth on a 5.5 m body, splayed 4 degrees outboard. The flame already hangs off the
  // mouth, so the bay keeps the stern length the plan gives it.
  for (const side of [-1, 1]) {
    const x = side * 0.22 * HW;
    const bell = lathe(group, metal, [
      [1.30, 0.00], [1.20, 0.65], [1.02, 1.72], [0.90, 2.80], [0.80, 3.70],
      [0.74, 4.60], [0.72, 5.20], [0.86, 5.50],
    ], 20, [x, at(1.0), 0], [0, 0, side * 0.07]);
    bell.castShadow = true;
    torus(group, copper, 1.32, 0.14, [x - side * 0.06, at(1.0) + 0.06, 0], [Math.PI / 2, 0, side * 0.07], 8, 24);
    torus(group, frame, 0.95, 0.22, [x, at(0.86), 0], [Math.PI / 2, 0, 0], 8, 16);
    for (const link of [-1, 1]) strut(group, copper, [x + link * 1.1, at(0.90), 0], [x + link * 0.7, at(0.80), 0], 0.16, 6);
    effect_cone(group, flames, { name: 'flame', radius: 1.25, length: 18.0,
      // The fade profile in the shader is written for a long cone: a short one never leaves the
      // dim middle of the curve, so the flames stay hull-proportional (0.39 of the 46 m hull, as
      // the old 36 m cones were of the old hull) and the bloom chain still finds them. The cone
      // origin sits at the bell mouth; the splay only walks the tip outboard.
      pos: [x, at(1.0) + 0.5, 0] });
  }

  // ---- RCS quads (0.08 and 0.88, +/-0.44): the in-plane cross ---------------------------------------
  // 1.3 m corner blocks with a four-nozzle cross in the gameplay plane (s2.4). One sim-resolved
  // cone per corner, named for the loader's position rule; the nozzles themselves are geometry.
  for (const side of [-1, 1]) for (const f of [0.08, 0.88]) {
    const x = side * 0.44 * HW, y = at(f);
    box(group, dark, [1.3, 1.3, 1.3], [x, y, 0]);
    const dirs = [[1, 0], [-1, 0], [0, 1], [0, -1]];
    for (const [dx, dy] of dirs) {
      const nozzle = cylinder(group, metal, 0.16, 0.26, 0.5,
        [x + dx * 0.75, y + dy * 0.75, 0], 8);
      nozzle.rotation.z = Math.atan2(dy, dx) - Math.PI / 2;
    }
    effect_cone(group, flames, { name: 'rcs-jet', radius: 0.55, length: 4.5, pos: [x + side * 3.1, y, 0], rot: [0, 0, -side * Math.PI / 2] });
  }

  // ---- docking collar (0.46, port edge): berth A, normal -X ----------------------------------------
  dock(group, 'A', [-HW, at(0.46), 0], Math.PI / 2, 'M');
  torus(group, frame, 1.9, 0.32, [-HW - 0.1, at(0.46), 0], [0, Math.PI / 2, 0], 8, 20);

  // ---- landing legs, tucked under the keel ------------------------------------------------------
  for (const side of [-1, 1]) for (const f of [0.32, 0.68]) {
    strut(group, metal, [side * 3.0, at(f), -DECK + 0.4], [side * 4.4, at(f) - 0.8, -DECK - 2.0], 0.34, 8);
    box(group, dark, [2.2, 1.6, 0.5], [side * 4.4, at(f) - 0.8, -DECK - 2.2]);
  }

  // ---- panel relief: ribs across the deck, relief on the flanks ----------------------------------
  for (const f of [0.24, 0.36, 0.56, 0.76]) {
    box(group, frame, [2 * HW - 3.4, 0.5, 0.35], [0, at(f), DECK + 0.65]);
  }
  for (const side of [-1, 1]) {
    panel_relief(group, armor, [10, 30, 0.5], [side * (HW - 0.6), at(0.5), 0], [0, 0, 0], { cols: 2, rows: 3, thickness: 0.6, depth: 0.5 });
  }
  return group;
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
        // Polished fields: the deck plates kept waxed. Dark in the green channel is smooth, so
        // this pulls those panels well under the hull's base roughness while the wear speckle the
        // kit drew stays bright and rough at the panel edges.
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

// The plan's own collider table (s2.1): the hull box is the authored gameplay legacy; the
// sidecar's compound shapes are derived from the geometry and are what the sim collides on.
export const meta = { name: 'kestrel', scale: 1.3, collider: { halfLength: 23, halfWidth: 5.0 } };
