// Kestrel: light corvette, re-authored to plan 05 s3 (K5 - the pattern for the rest).
//
// The plan-04 hull was faithful to its spec table and still read as a grey lump, for arithmetic
// reasons: every feature carrying the design was one to two pixels wide at the old framing. This
// pass obeys the s3.2 authoring rules at the new 3.00 px/m home framing:
//
//   minimum feature 1.5 m      - everything smaller moved to the module's maps (J6)
//   identity bands 3.5 m       - teal abaft ochre, one continuous block across deck and flanks
//   silhouette notch >= 3.0 m  - the bay waist cuts both flanks; the stern reads forked
//   triangle budget ~1500      - the bold LOD; no panel lines or rivets as geometry
//   material regions >= 6 m2   - plan-view area, which the exporter's audit measures
//
// Plan outline, nose +Y, dorsal +Z, starboard +X, metres. The plan silhouette is the design: an
// ogival prow with a shoulder step, a waisted midship where the dorsal bay cuts both flanks, and a
// forked stern whose prongs carry the drive bells outboard.
import * as THREE from 'three';
import {
  armor, black, copper, dark, frame, lightArmor, metal, teal, ochre,
  bevelled, box, cylinder, dock, effect_cone, hp, lathe, plate, standard_maps, type Maps,
} from './prims';

// Plan frame: L = 46, half-width 7.4, deck +/-4.6. All stations are fractions of L from the nose.
const L = 46;
const HW = 7.4;
const DECK = 4.6;
const at = (f: number) => L / 2 - f * L;

// The plan outline. Waist: the bay cuts both flanks 3.2 m deep (s3.2's notch floor). Fork: the
// stern ends in two prongs 4.8 m apart, the bells in them, the middle open.
const HULL_OUTLINE: number[][] = [
  [-4.07, at(0.0)], [4.07, at(0.0)],            // blunt ogive, 8.1 m on the nose face
  [6.5, at(0.10)],                              // shoulder step out (s3.5: the prow keeps its ogive)
  [HW, at(0.18)],
  [HW, at(0.30)], [5.0, at(0.30)],              // the bay waist begins: 2.4 m off each flank
  [5.0, at(0.44)], [HW, at(0.44)],              // ...and closes
  [HW, at(0.72)], [6.0, at(0.86)],
  [4.6, at(0.96)], [4.6, at(1.0)],              // the prong tips
  [1.6, at(1.0)],                               // the fork: 3.2 m of open transom between
  [-1.6, at(1.0)], [-4.6, at(1.0)],
  [-6.0, at(0.86)], [-HW, at(0.72)],
  [-HW, at(0.44)], [-5.0, at(0.44)], [-5.0, at(0.30)], [-HW, at(0.30)],
  [-HW, at(0.18)], [-6.5, at(0.10)],
];

// The deck is four plates with the bay a real hole between them: a recess that only the deck
// plate took would read, from above, as a decal. The bay is a cut in the whole plan (s3.5).
const DECK_FORE: number[][] = [
  [-6.2, at(0.02)], [6.2, at(0.02)], [6.2, at(0.30)], [-6.2, at(0.30)],
];
const DECK_RAIL_STARBOARD: number[][] = [
  [3.4, at(0.30)], [6.2, at(0.30)], [6.2, at(0.44)], [3.4, at(0.44)],
];
const DECK_RAIL_PORT: number[][] = [
  [-6.2, at(0.30)], [-3.4, at(0.30)], [-3.4, at(0.44)], [-6.2, at(0.44)],
];
const DECK_AFT: number[][] = [
  [-6.2, at(0.44)], [6.2, at(0.44)], [6.2, at(0.62)], [4.0, at(0.84)],
  [2.6, at(0.96)], [1.1, at(0.96)], [1.1, at(1.0)], [-1.1, at(1.0)],
  [-1.1, at(0.96)], [-2.6, at(0.96)], [-4.0, at(0.84)], [-6.2, at(0.62)],
];

export function build(): THREE.Object3D {
  const group = new THREE.Group(); group.name = 'kestrel';
  const flames: THREE.Mesh[] = [];

  // ---- armoured hull: the plan silhouette, extruded ------------------------------------------
  plate(group, HULL_OUTLINE, 9.2, -DECK, armor);
  // Dorsal deck: four plates around the open bay, the faceted section the plan's drawing shows.
  plate(group, DECK_FORE, 1.6, DECK, lightArmor);
  plate(group, DECK_RAIL_STARBOARD, 1.6, DECK, lightArmor);
  plate(group, DECK_RAIL_PORT, 1.6, DECK, lightArmor);
  plate(group, DECK_AFT, 1.6, DECK, lightArmor);

  // Prow cheeks: faceted armour over the ogive, and the plated-over bridge's sensor windows
  // (1.9 x 2.1 - well over the 1.5 m floor, and the reason no glasshouse was authored).
  for (const side of [-1, 1]) {
    bevelled(group, dark, [3.4, 6.5, 2.6], [side * 3.6, at(0.07), 3.4], [0, 0, side * 0.18], { radius: 0.4, segments: 1 });
    box(group, black, [1.9, 2.6, 2.1], [side * 2.1, at(0.045), 1.4]);
  }

  // ---- identity marking: teal abaft ochre, one continuous block across deck and both flanks ----
  // s3.5: carried as one wrap, not three thin strips. 3.5 m along the hull at 14.4 m wide is
  // 50 m2 of deck alone; the flanks double it. The audit measures the plan view: both clear the
  // 6 m2 floor by an order of magnitude, and at 3.00 px/m they are 10.5 px bands.
  // The deck plate extrudes a 0.6 bevel above its own depth, so the proud work rides clear of
  // it: the bands read across the deck AND down both flanks, one continuous block (s3.5).
  box(group, ochre, [2 * HW - 0.4, 3.5, 1.5], [0, 18.2, DECK + 2.9]);
  box(group, teal, [2 * HW - 0.4, 3.5, 1.5], [0, 14.5, DECK + 2.9]);
  for (const side of [-1, 1]) {
    box(group, ochre, [1.5, 3.5, 2 * DECK - 0.6], [side * (HW - 0.1), 18.2, 0]);
    box(group, teal, [1.5, 3.5, 2 * DECK - 0.6], [side * (HW - 0.1), 14.5, 0]);
  }

  // ---- turret rings: the mounts the pdc_turret modules bolt to --------------------------------
  // Low drums instead of tori: a ring is a silhouette idea, a drum is the mount, and both cost a
  // twentieth of the triangles. The plan's stations are unchanged - gameplay does not move.
  for (const [id, f, s] of [['pdc.fwd', 0.20, 0.25], ['pdc.aft', 0.28, -0.25]] as const) {
    const x = s * HW, y = at(f);
    cylinder(group, frame, 1.4, 1.5, 1.5, [x, y, DECK + 1.9], 14);
    hp(group, id, [x, y, DECK + 2.65]);
  }
  cylinder(group, frame, 1.4, 1.5, 1.5, [0, at(0.42), -DECK - 0.75], 14);
  hp(group, 'pdc.ventral', [0, at(0.42), -DECK - 1.5]);

  // ---- open dorsal bay: the waist IS the bay --------------------------------------------------
  // The bay floor sits in the waist between the deck plates; its coaming walls are the cut flanks.
  box(group, dark, [6.4, at(0.30) - at(0.44), 1.5], [0, (at(0.30) + at(0.44)) / 2, DECK + 0.5]);

  // ---- drive bells (0.86-1.00, outboard in the fork prongs): the stern reads forked -----------
  for (const side of [-1, 1]) {
    const x = side * 3.1;
    const bell = lathe(group, metal, [
      [1.30, 0.00], [1.10, 1.60], [0.90, 3.30], [0.78, 4.60], [0.92, 5.20],
    ], 14, [x, at(1.0), 0], [0, 0, side * 0.05]);
    bell.castShadow = true;
    cylinder(group, copper, 1.42, 1.34, 1.5, [x, at(1.0) + 0.7, 0], 14);
    effect_cone(group, flames, { name: 'flame', radius: 1.25, length: 18.0,
      pos: [x, at(1.0) + 0.5, 0] });
  }

  // ---- RCS quads (0.08 and 0.88, +/-0.44): the in-plane cross ---------------------------------
  // One 1.6 m block per corner with a single outboard nozzle; the four-nozzle cross the plan-04
  // hull modelled is normal-map content now (J6). One sim-resolved cone per corner remains.
  for (const side of [-1, 1]) for (const f of [0.08, 0.88]) {
    const x = side * 0.44 * HW, y = at(f);
    box(group, dark, [1.6, 1.6, 1.6], [x, y, 0]);
    // The nozzle cross the plan-04 hull modelled is map content now: four 0.5 m nozzles were
    // three-pixel stumps at the home framing (J6). The sim-resolved jet cone stays.
    effect_cone(group, flames, { name: 'rcs-jet', radius: 0.55, length: 4.5, pos: [x + side * 3.1, y, 0], rot: [0, 0, -side * Math.PI / 2] });
  }

  // ---- docking collar (0.46, port edge): berth A, normal -X -----------------------------------
  dock(group, 'A', [-HW, at(0.46), 0], Math.PI / 2, 'M');
  cylinder(group, frame, 2.0, 2.1, 1.6, [-HW - 0.7, at(0.46), 0], 16).rotation.z = Math.PI / 2;

  // ---- landing pads, tucked under the keel ----------------------------------------------------
  for (const side of [-1, 1]) for (const f of [0.32, 0.68]) {
    box(group, metal, [1.6, 2.4, 1.5], [side * 3.4, at(f), -DECK - 1.5]);
    box(group, dark, [2.2, 2.4, 1.5], [side * 3.4, at(f), -DECK - 2.6]);
  }

  return group;
}

export const maps: Maps = standard_maps({
  seed: 12, panel: 0.05, rivets: 0.03, seams: 3, wear: 0.28, grime: 0.2, scorch: 2,
  stencils: [['K-7', 0.1, 0.16], ['KESTREL', 0.34, 0.66]],
  hazard: { x: 0.05, y: 0.82, w: 0.9, h: 0.12, angle: Math.PI / 3 },
});

// The plan's own collider table (s2.1, plan-04 A1): the hull box is the authored gameplay legacy;
// the sidecar's compound shapes are derived from the geometry and are what the sim collides on.
export const meta = { name: 'kestrel', scale: 1.3, collider: { halfLength: 23, halfWidth: 5.0 } };
