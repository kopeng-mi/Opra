// Tessera L4: the Lagrange base (plan-04 §2.6, ref 21). A truss backbone along X with the
// habitat ring at 1/3, a processing block on one end and an unfinished gantry on the other —
// built as-drawn, the incompleteness is the character. Four ports along the truss.
//
// The previous 104 m ring-station is gone: the sheet draws a construction site, not a wheel.
// Axes: nose +Y, dorsal +Z, starboard +X, metres. The ring lies in the gameplay plane (XY).
import * as THREE from 'three';
import {
  armor, black, copper, dark, deckPlate, frame, glass, hazardPaint, hullPaint, insulation,
  lightArmor, metal, ochre, solarCell, teal,
  bevelled, box, dock, dome, each_tile, hazard_stripes, hp, lamps, lathe,
  scorch, sphere, standard_maps, stencil_text, strut, torus, truss_box, tube, type Maps,
} from './prims';

const TAU = Math.PI * 2;
/** Backbone extent and the ring station, 1/3 from the processing end. */
const HALF = 150;
const RING_X = -50;
const RING = 45;
const RING_TUBE = 8;

/** One side berth off the truss: collar, face, lamps and the `dock.<id>` anchor. */
function berth(root: THREE.Object3D, id: string, x: number, side: number, cls: string): void {
  const site = new THREE.Group();
  site.position.set(x, side * 8, 0);
  site.rotation.z = side > 0 ? 0 : Math.PI;
  root.add(site);
  strut(root, frame, [x, side * 5, 0], [x, side * 9, 0], 0.5, 6);
  tube(site, frame, 3.6, 4.0, 2.0, [0, 2.0, 0], 14);
  torus(site, copper, 3.5, 0.3, [0, 3.6, 0], [Math.PI / 2, 0, 0], 5, 14);
  tube(site, dark, 2.7, 2.7, 0.8, [0, 3.2, 0], 14);
  box(site, deckPlate, [7.0, 0.25, 0.25], [0, 5.4, 0]);
  dock(site, id, [0, 3.6, 0], 0, cls);
  lamps(site, '#b7dfdd', [[-2.4, 5.6, 0], [2.4, 5.6, 0]], 0.4, 6);
}

/** The processing end: tank farm, ochre-clad works block and radiator wings. */
function processing_end(root: THREE.Object3D): void {
  const end = new THREE.Group();
  end.position.set(-HALF, 0, 0);
  root.add(end);
  // Works block with its ochre cladding, flanked by the tank farm.
  bevelled(end, hullPaint, [26.0, 18.0, 16.0], [8, 0, 0], [0, 0, 0], { radius: 1.0, segments: 1 });
  for (const z of [-9.5, 9.5]) box(end, ochre, [20.0, 14.0, 1.0], [8, 0, z]);
  box(end, dark, [6.0, 8.0, 6.0], [22, 0, 0]);
  for (const z of [-12, 0, 12]) {
    tube(end, insulation, 5.0, 5.0, 14.0, [-6, z * 0.9, z * 0.4], 14);
    dome(end, insulation, 5.0, [-6, z * 0.9, z * 0.4 + 7.0], [0, 0, 0], 14);
  }
  // Scaffolding over the block and the vent stacks that make it read as a plant.
  for (const x of [-2, 18]) for (const y of [-10, 10])
    strut(end, frame, [x, y, 8], [x, y, 16], 0.3, 5);
  for (const x of [-2, 18]) strut(end, frame, [x, -10, 16], [x, 10, 16], 0.25, 5);
  for (const y of [-6, 6]) tube(end, dark, 0.8, 1.0, 9.0, [14, y, 12], 8);
  lamps(end, '#e8c98a', [[-4, -11, 4], [20, 11, 4], [8, 0, 17]], 0.5, 6);
}

/** The unfinished end, as-drawn: open gantry frame, a crane arm slewed outboard, loose
 *  girders staged alongside and cables hanging off the last complete bay. */
function gantry_end(root: THREE.Object3D): void {
  const end = new THREE.Group();
  end.position.set(HALF - 18, 0, 0);
  root.add(end);
  // Last complete bays, then the open frame: longerons that stop mid-air with cut ends.
  truss_box(end, frame, 36, 10, 10, 3, 0.6);
  for (const sx of [-1, 1]) for (const sz of [-1, 1]) {
    strut(end, dark, [sx * 5, 18, sz * 5], [sx * 5, 34 + (sx * sz > 0 ? 6 : 0), sz * 5], 0.45, 6);
    box(end, hazardPaint, [1.2, 1.0, 1.2], [sx * 5, 18.5, sz * 5]);
  }
  // Crane: pedestal, slewed jib, hook block on a cable — the work in progress, frozen.
  tube(end, metal, 2.2, 2.6, 5.0, [0, -12, 8], 10);
  strut(end, frame, [0, -12, 10], [22, -12, 16], 0.5, 6);
  strut(end, dark, [22, -12, 16], [22, -12, 2], 0.08, 4);
  box(end, metal, [1.6, 1.6, 2.0], [22, -12, 1.0]);
  hp(end, 'crane', [22, -12, 2]);
  // Staged girders and hanging cables: material waiting for the next shift.
  for (let i = 0; i < 3; i++)
    strut(end, metal, [-8 + i * 3, 14, -8], [10 + i * 3, 14, -8], 0.3, 5);
  for (const x of [-14, -6, 4]) {
    strut(end, dark, [x, 16, 4], [x + 1.5, 8, 4.5], 0.06, 3);
    box(end, copper, [0.5, 0.7, 0.5], [x + 1.5, 7.6, 4.5]);
  }
  lamps(end, '#e6554d', [[-5, 20, 6], [5, 20, 6]], 0.45, 6);
}

export function build(): THREE.Object3D {
  const group = new THREE.Group(); group.name = 'station_lagrange';

  // Backbone: the original unpressurised truss everything else hangs off.
  const spine = new THREE.Group();
  spine.rotation.z = Math.PI / 2;
  group.add(spine);
  truss_box(spine, frame, HALF * 2 - 20, 10, 10, 18, 0.8);
  // Bolted-on modules along the truss: labs, stores and the drum farm, each layer keeping its
  // own material so the decades read at a glance.
  for (const [x, w] of [[-110, 14], [-78, 10], [-20, 12], [14, 10], [52, 14], [96, 10]] as [number, number][]) {
    box(group, x < -60 ? armor : hullPaint, [w, 12.0, 9.0], [x, 0, 8.5]);
    box(group, dark, [w * 0.7, 2.0, 2.0], [x, 0, -6.0]);
  }
  for (const x of [-88, 30, 70]) {
    tube(group, insulation, 3.4, 3.4, 9.0, [x, -8.5, 0], 12, [0, 0, Math.PI / 2]);
  }

  // Habitat ring at 1/3: pressure torus in the gameplay plane on four in-plane spokes, turning
  // on a bearing around the truss midpoint of its station.
  torus(group, hullPaint, RING, RING_TUBE, [RING_X, 0, 0], [0, 0, 0], 12, 64);
  tube(group, dark, 12.0, 12.0, 16.0, [RING_X, 0, 0], 16, [Math.PI / 2, 0, 0]);
  torus(group, metal, 12.0, 1.2, [RING_X, 0, 8.5], [0, 0, 0], 6, 20);
  for (let i = 0; i < 4; i++) {
    const angle = (i / 4) * TAU + TAU / 8;
    const spoke = new THREE.Group();
    spoke.position.set(RING_X + Math.cos(angle) * RING / 2, Math.sin(angle) * RING / 2, 0);
    spoke.rotation.z = angle - Math.PI / 2;
    group.add(spoke);
    truss_box(spoke, frame, RING - 22, 4.0, 4.0, 5, 0.45);
  }
  for (let i = 0; i < 6; i++) {
    const angle = (i / 6) * TAU + TAU / 12;
    bevelled(group, lightArmor, [5.0, 4.5, 3.5],
      [RING_X + Math.cos(angle) * RING, Math.sin(angle) * RING, RING_TUBE + 0.8], [0, 0, angle],
      { radius: 0.4, segments: 1 });
  }
  for (const angle of [0.5, 0.72, 2.4]) {
    box(group, teal, [2.8, 1.4, 0.4],
      [RING_X + Math.cos(angle) * RING, Math.sin(angle) * RING, RING_TUBE - 0.2], angle + Math.PI / 2);
  }

  // Solar wings: a cross-boom pair with arrays outboard on each side, one amidships and one
  // past the ring toward the gantry.
  for (const x of [-16, 78]) {
    box(group, frame, [3.0, 44.0, 3.0], [x, 0, -12]);
    for (const side of [-1, 1]) {
      box(group, solarCell, [10.0, 18.0, 0.4], [x, side * 16, -12]);
      box(group, frame, [10.6, 18.6, 0.3], [x, side * 16, -12.4]);
    }
  }

  processing_end(group);
  gantry_end(group);

  // Four berths along the truss, alternating sides.
  berth(group, 'A', -104, 1, 'L');
  berth(group, 'B', -38, -1, 'L');
  berth(group, 'C', 34, 1, 'L');
  berth(group, 'D', 108, -1, 'L');

  // Truss section lighting: a lamp run down the whole backbone.
  const run: number[][] = [];
  for (let i = -7; i <= 7; i++) run.push([i * 18, 6.5, 0]);
  lamps(group, '#b7dfdd', run, 0.5, 6);
  return group;
}

/** Panels, welds, wear and the markings a base accumulates: section numbers on the truss. */
export const maps: Maps = (() => {
  const kit = standard_maps({
    seed: 9, panel: 0.18, rivets: 0.08, seams: 3, wear: 0.34, grime: 0.3, scorch: 1,
    stencils: [['L4-SEC', 0.08, 0.12]],
  });
  return {
    normal: kit.normal,
    roughness: kit.roughness,
    albedo: (ctx, size) => {
      kit.albedo?.(ctx, size);
      each_tile(ctx, size, (tile) => {
        hazard_stripes(tile, { x: 0.06, y: 0.78, w: 0.88, h: 0.12 }, { pitch: 0.04, angle: Math.PI / 3 });
        stencil_text(tile, 'L4', { x: 0.6, y: 0.3, scale: 0.03 });
      });
    },
  };
})();

export const meta = { name: 'station_lagrange', scale: 1, collider: 'auto', untextured: ['glass', 'solarCell'] };
