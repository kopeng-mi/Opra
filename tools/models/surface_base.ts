// Surface base (PLAN-03 §7.1, order of work item a, G16): the thing a landed ship refuels at.
// Origin at pad level, deck tops on z = 0, and the whole footprint flat — G13 forces a level span
// under a pad, and this asset has to sit on it without a leg poking through the heightfield.
//
// Layout: two pad decks of different size (the big one takes a freighter, the small one a shuttle),
// a habitat drum with its airlock, a fuel plant with two cryo tanks and a sphere farm, a comms and
// scanner mast on the ridge, blast walls between the pads, and the pipeways and cargo that make a
// base look inhabited rather than dropped.
import * as THREE from 'three';
import {
  dark, deckPlate, frame, hazardPaint, hullPaint, insulation, lamp_material, metal, copper, glass,
  bevelled, box, cylinder, dome, each_tile, hazard_stripes, lamp_material as lampMaterial, lamps,
  lathe, sensor_dish, sphere as ball, standard_maps, stencil_text, strut, truss_box, torus, tube, type Maps,
} from './prims';
import { landing_pad_assembly } from './landing_pad';

function habitat(group: THREE.Object3D, x: number, y: number, rotation: number): void {
  const site = new THREE.Group();
  site.position.set(x, y, 0);
  site.rotation.z = rotation;
  group.add(site);
  // Buried habitat drum: 26 m across, capped, with a ring of windows and a docking lobe.
  lathe(site, insulation, [[13.0, 0], [13.6, 1.4], [13.6, 7.4], [12.4, 9.2], [9.0, 10.4]], 28);
  torus(site, metal, 13.2, 0.7, [0, 2.2, 0], [Math.PI / 2, 0, 0], 8, 32);
  torus(site, metal, 13.0, 0.7, [0, 6.6, 0], [Math.PI / 2, 0, 0], 8, 32);
  dome(site, insulation, 9.0, [0, 10.4, 0], [0, 0, 0], 24);
  for (let i = 0; i < 7; i++) {
    const angle = (i / 7) * Math.PI * 2 + 0.2;
    const at = [Math.cos(angle) * 13.4, 4.6, Math.sin(angle) * 13.4];
    bevelled(site, metal, [2.6, 3.0, 0.8], at, [0, -angle, 0], { radius: 0.3, segments: 1 });
    const pane = new THREE.Mesh(new THREE.BoxGeometry(1.9, 2.2, 0.25), glass);
    pane.position.set(Math.cos(angle) * 13.9, 4.6, Math.sin(angle) * 13.9);
    pane.rotation.y = -angle;
    site.add(pane);
  }
  // Entry lobe with a real airlock: two doors, a ramp and a handrail run.
  lathe(site, hullPaint, [[3.4, 0], [3.4, 4.6], [2.6, 5.6]], 16, [13.6, 0, 0], [0, 0, -Math.PI / 2]);
  box(site, metal, [7.0, 3.0, 0.4], [17.2, 0, 1.6]);
  box(site, dark, [3.2, 0.5, 6.0], [19.6, 0, 0.2]);
  for (const side of [-1, 1]) strut(site, metal, [16.0, side * 2.8, 3.6], [20.0, side * 2.8, 1.2], 0.16, 5);
  box(site, hazardPaint, [1.4, 1.4, 0.3], [19.9, 0, 3.2]);
  // Roof plant: two chillers, a vent stack and a ladder.
  for (const side of [-1, 1]) bevelled(site, metal, [4.0, 4.0, 2.6], [side * 4.6, 0, 13.0], [0, 0, 0], { radius: 0.4, segments: 1 });
  cylinder(site, dark, 1.2, 1.6, 7.0, [0, 0, 15.0], 12);
  for (let rung = 0; rung < 7; rung++) box(site, metal, [0.14, 2.0, 0.14], [12.0, 0, 1.0 + rung * 1.4]);
}

function fuel_plant(group: THREE.Object3D, x: number, y: number): void {
  const site = new THREE.Group();
  site.position.set(x, y, 0);
  group.add(site);
  // Two horizontal cryo tanks on saddles, lagged, with a lagged sphere farm behind them.
  for (const side of [-1, 1]) {
    lathe(site, insulation, [[4.2, 0], [4.6, 1.4], [4.6, 24.0], [4.0, 25.6], [1.6, 26.4]], 24, [side * 7.5, 0, 6.4], [Math.PI / 2, 0, 0]);
    for (const along of [-8, 0, 8]) {
      bevelled(site, dark, [5.0, 3.4, 1.6], [side * 7.5, along, 1.7], [0, 0, 0], { radius: 0.35, segments: 1 });
      strut(site, metal, [side * 7.5 + 3.4, along, 5.6], [side * 7.5 + 3.4, along, 1.4], 0.3, 6);
    }
    torus(site, metal, 4.4, 0.4, [side * 7.5, 8.0, 6.4], [0, 0, 0], 6, 24);
    torus(site, metal, 4.4, 0.4, [side * 7.5, 16.0, 6.4], [0, 0, 0], 6, 24);
  }
  for (const at of [[-4.4, 0, 0], [0, 0, 0], [4.4, 0, 0]]) {
    ball(site, insulation, 4.4, [at[0], at[1] - 12.0, 5.0], 20, 14);
  }
  // Pump house, manifold and the pipeway that runs to the pads.
  bevelled(site, hullPaint, [9.0, 6.0, 4.4], [0, -6.0, 2.2], [0, 0, 0], { radius: 0.5, segments: 1 });
  box(site, dark, [9.4, 1.2, 4.8], [0, -6.0, 4.6]);
  for (const z of [0.8, 1.8, 2.8]) box(site, copper, [16.0, 0.4, 0.4], [0, 3.4, z]);
  for (let i = 0; i < 5; i++) strut(site, metal, [-8 + i * 4, 3.4, 2.2], [-8 + i * 4, 3.4, 0.0], 0.22, 5);
  cylinder(site, metal, 1.0, 1.0, 12.0, [9.6, -2.0, 6.0], 10, [0, 0, 0]);
  lamps(site, '#efbf7a', [[10.6, -2.0, 12.4], [-10.6, -6.0, 5.2]], 0.5, 6);
}

function comms_mast(group: THREE.Object3D, x: number, y: number): void {
  const site = new THREE.Group();
  site.position.set(x, y, 0);
  group.add(site);
  // A 52 m lattice tower with a scanner dish, three service platforms and an aviation lamp.
  truss_box(site, frame, 52.0, 6.0, 6.0, 12, 0.42).position.z = 30.0;
  bevelled(site, hullPaint, [9.0, 9.0, 1.0], [0, 0, 4.0], [0, 0, 0], { radius: 0.5, segments: 1 });
  for (const at of [18.0, 34.0, 50.0]) {
    tube(site, deckPlate, 4.4, 4.4, 0.5, [0, 0, at], 16, [Math.PI / 2, 0, 0]);
    for (let i = 0; i < 6; i++) {
      const angle = (i / 6) * Math.PI * 2;
      strut(site, metal, [Math.cos(angle) * 3.0, Math.sin(angle) * 3.0, at - 1.2], [Math.cos(angle) * 3.0, Math.sin(angle) * 3.0, at + 1.4], 0.14, 4);
    }
  }
  torus(site, frame, 4.0, 0.3, [0, 0, 18.0], [Math.PI / 2, 0, 0], 6, 20);
  torus(site, frame, 4.0, 0.3, [0, 0, 34.0], [Math.PI / 2, 0, 0], 6, 20);
  // Dish on a gimbal at the tower head, plus a whip antenna.
  lathe(site, metal, [[0.6, 0], [0.6, 2.2]], 10, [0, 0, 52.4]);
  const gimbal = new THREE.Group();
  gimbal.position.set(0, 0, 55.4);
  gimbal.rotation.set(Math.PI / 2 - 0.5, 0, 0);
  site.add(gimbal);
  lathe(gimbal, insulation, [[5.4, 0], [5.0, 0.5], [4.2, 1.6]], 20);
  for (const side of [-1, 1]) strut(gimbal, metal, [0, side * 0.8, 1.2], [0, side * 5.0, 0.6], 0.16, 5);
  cylinder(site, metal, 0.16, 0.16, 12.0, [2.4, 0, 58.0], 6);
  lamps(site, '#e6554d', [[0, 0, 52.6]], 0.6, 8);
}

function blast_wall(group: THREE.Object3D, x: number, y: number, length: number, rotation: number): void {
  const site = new THREE.Group();
  site.position.set(x, y, 0);
  site.rotation.z = rotation;
  group.add(site);
  const segments = Math.max(2, Math.round(length / 12));
  for (let i = 0; i < segments; i++) {
    const at = -length / 2 + (i + 0.5) * (length / segments);
    box(site, deckPlate, [length / segments - 0.5, 2.2, 7.0], [0, at, 1.6]);
  }
  for (let i = 0; i <= segments; i++) box(site, dark, [1.0, 1.2, 7.6], [0, -length / 2 + i * (length / segments), 1.6]);
}

export function build(): THREE.Object3D {
  const group = new THREE.Group(); group.name = 'surface_base';
  const apron = new THREE.Group(); group.add(apron);
  // Apron: one flat concrete apron under everything, on the z = 0 datum, with expansion joints.
  bevelled(apron, deckPlate, [196.0, 150.0, 3.2], [-4.0, 0, -1.6], [0, 0, 0], { radius: 1.2, segments: 1 });
  for (let i = -4; i <= 4; i++) box(apron, dark, [0.5, 150.0, 0.3], [-4.0 + i * 20.0, 0, 0.16]);
  for (let j = -3; j <= 3; j++) box(apron, dark, [196.0, 0.5, 0.3], [-4.0, j * 20.0, 0.16]);
  for (const edge of [-1, 1]) {
    box(apron, hazardPaint, [196.0, 1.4, 0.3], [-4.0, edge * 72.0, 0.16]);
    box(apron, hazardPaint, [1.4, 150.0, 0.3], [-4.0 + edge * 95.0, 0, 0.16]);
  }
  // The two decks: equal circular pads, the plan view's own layout, with the berth anchors the
  // sim docks to (`dock.pad.A` / `dock.pad.B`, facing local up).
  const big = new THREE.Group(); group.add(big);
  landing_pad_assembly(big, { id: 'pad.A', radius: 21, clamps: 4, lights: 6 });
  const small = new THREE.Group();
  small.position.set(66.0, 46.0, 0);
  group.add(small);
  landing_pad_assembly(small, { id: 'pad.B', radius: 18, clamps: 4, lights: 4, lamp: '#e8c98a' });
  // Walkway: a covered corridor from the big pad to the shuttle pad and on to the habitat, so
  // the crew never crosses open regolith between the decks.
  box(group, hullPaint, [30.0, 4.0, 3.0], [38.0, 26.0, 1.5], 0.6);
  box(group, hullPaint, [44.0, 4.0, 3.0], [-24.0, 30.0, 1.5], 0.12);
  for (let i = 0; i < 5; i++) box(group, glass, [1.6, 4.2, 1.2], [26.0 + i * 6.0, 30.5 - i * 1.1, 1.5], 0.6);
  habitat(group, -66.0, 34.0, 0.35);
  fuel_plant(group, -62.0, -46.0);
  comms_mast(group, 46.0, -66.0);
  blast_wall(group, 20.0, 66.0, 44.0, 0.0);
  blast_wall(group, -30.0, 62.0, 30.0, 0.15);
  // Pipeway from the fuel plant to the big pad, and a cargo yard beside it.
  for (let i = 0; i < 7; i++) {
    const at = i * 6.0;
    strut(group, metal, [-40.0 + at, -44.0 + at * 0.55, 1.0], [-34.0 + at, -38.0 + at * 0.55, 1.0], 0.5, 6);
    box(group, copper, [8.0, 0.5, 0.5], [-36.0 + at, -40.0 + at * 0.55, 2.4]);
  }
  for (let i = 0; i < 4; i++) {
    bevelled(group, i % 2 ? hullPaint : metal, [7.0, 9.0, 5.0], [46.0 + (i % 2) * 9.0, 18.0 + Math.floor(i / 2) * 11.0, 3.6], [0, 0, 0], { radius: 0.5, segments: 1 });
  }
  // Perimeter lighting and the apron's flood masts.
  const ring: number[][] = [];
  for (let i = 0; i < 18; i++) {
    const angle = (i / 18) * Math.PI * 2;
    ring.push([-4.0 + Math.cos(angle) * 88.0, Math.sin(angle) * 70.0, 0.4]);
  }
  lamps(group, '#b7dfdd', ring, 0.55, 6);
  for (const at of [[-52.0, 58.0], [30.0, 58.0], [-52.0, -14.0], [30.0, -14.0]]) {
    strut(group, metal, [at[0], at[1], 0], [at[0], at[1], 11.0], 0.4, 8);
    bevelled(group, dark, [2.2, 1.6, 1.4], [at[0], at[1], 11.4], [0, 0, 0], { radius: 0.35, segments: 1 });
    const head = new THREE.Mesh(new THREE.SphereGeometry(0.7, 8, 6), lampMaterial('#efe4bb'));
    head.position.set(at[0], at[1], 12.2);
    group.add(head);
  }
  return group;
}

export const maps: Maps = (() => {
  const kit = standard_maps({
    seed: 88, panel: 0.13, rivets: 0.07, seams: 2, wear: 0.34, grime: 0.3,
    hazard: { x: 0.05, y: 0.78, w: 0.9, h: 0.14, angle: Math.PI / 4 },
    stencils: [['BASE 12', 0.1, 0.12], ['FUEL', 0.62, 0.3]],
  });
  return {
    normal: kit.normal,
    roughness: kit.roughness,
    albedo: (ctx, size) => {
      kit.albedo?.(ctx, size);
      each_tile(ctx, size, (tile) => {
        stencil_text(tile, 'NO SMOKING', { x: 0.08, y: 0.5, scale: 0.024 });
        hazard_stripes(tile, { x: 0.06, y: 0.08, w: 0.4, h: 0.12 }, { pitch: 0.045, angle: -Math.PI / 4 });
      });
    },
  };
})();

export const meta = { name: 'surface_base', scale: 1, collider: 'auto' };
