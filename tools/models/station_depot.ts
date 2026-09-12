// Tessera L1 depot (PLAN-03 §7.1): the small one. A tank farm on a truss spine with a berth at each
// end and nothing aboard anyone lives in - no drum, no windows, no pressurised ring. Its whole
// budget went into structure you can bolt tanks, radiators and a mast to, which is what makes it
// read as cheap next to the Wayfarer ring.
//
// Both berths open along the spine's own axis. A port's local +Y is its outward normal, so the two
// mouths face +Y and -Y and both normals land in the flight plane the sim flies in; a dorsal mouth
// would project to a zero-length normal and no run could use it.
//
// Axes as always: nose +Y, dorsal +Z, starboard +X, metres. Envelope ~142 m along the spine.
import * as THREE from 'three';
import {
  box, bevelled, copper, dark, deckPlate, dock, each_tile, frame, hazardPaint, hp,
  hullPaint, insulation, lamps, lathe, lightArmor, metal, noise_wash, solarCell, standard_maps,
  stencil_text, strut, truss_box, tube, type Maps,
} from './prims';

/**
 * One berth: a flared throat carrying the loads back into the truss, the hardened ring a tanker
 * lands on, four guide arms and the square mouth frame the approach lamps are fixed to.
 *
 * Authored facing local +Y so both ends are one piece of code. The aft copy is that mouth rotated
 * a half turn about X, which is a rotation and not a mirror, so the lathe normals still face out.
 */
function berth(root: THREE.Object3D, id: string, y: number, sign: number): void {
  const mouth = new THREE.Group();
  mouth.name = `berth.${id}`;
  mouth.position.set(0, y, 0);
  mouth.rotation.x = sign > 0 ? 0 : Math.PI;
  root.add(mouth);

  lathe(mouth, metal, [[3.0, -8], [5.6, -3], [5.6, 7.6], [4.4, 10.4]], 16);
  tube(mouth, metal, 6.1, 6.1, 1.8, [0, 8.8, 0], 12);
  // The face itself is painted: a ring a crew works around is the one place hazard stripes earn
  // their cost, and the collar is what the tanker's own seal lands against.
  tube(mouth, hazardPaint, 4.7, 4.7, 1.2, [0, 10.6, 0], 14);
  // Inner lip: the taper that centres the probe before the seal touches, so a bad approach scuffs
  // the lip instead of the ring.
  lathe(mouth, lightArmor, [[3.4, 10.8], [3.4, 12.2], [2.6, 13.2]], 12);
  for (const x of [-1, 1]) for (const z of [-1, 1]) {
    strut(mouth, metal, [x * 5.6, 7.0, z * 5.6], [x * 7.8, 11.2, z * 7.8], 0.42, 6);
  }
  for (const side of [-1, 1]) {
    strut(mouth, frame, [-7.8, 11.2, side * 7.8], [7.8, 11.2, side * 7.8], 0.36, 6);
    strut(mouth, frame, [side * 7.8, 11.2, -7.8], [side * 7.8, 11.2, 7.8], 0.36, 6);
    // The outboard lamps get a post each: a marker floating clear of the frame reads as debris.
    strut(mouth, metal, [side * 7.8, 11.2, 0], [side * 10.6, 11.2, 0], 0.3, 6);
  }
  // Back stays to the last truss bay: without them the throat is a 20 m cantilever.
  for (const side of [-1, 1]) strut(mouth, frame, [side * 5.4, -4.0, side * 5.4], [side * 3.6, -18.0, side * 3.6], 0.3, 6);
  // Approach lighting: the four mouth-frame corners plus two on the outboard posts, so a pilot
  // reads the roll of the berth before the ring is close enough to fill the view.
  lamps(mouth, '#efe4bb', [
    [-7.8, 11.8, -7.8], [7.8, 11.8, -7.8], [-7.8, 11.8, 7.8], [7.8, 11.8, 7.8],
    [-10.6, 11.8, 0], [10.6, 11.8, 0],
  ], 0.55, 5);
  dock(mouth, id, [0, 13.8, 0], 0, 'M');
}

/**
 * One outrigger tank cluster: a lagged CH4 main and the smaller LOX sump strapped above it, both on
 * one saddle so the outrigger carries a single load path. The manifold header runs between them with
 * the block valves that let each tank be isolated without a crew aboard.
 */
function cluster(root: THREE.Object3D, sx: number, sy: number): void {
  const x = sx * 44, y = sy * 34;
  // Outrigger: two beams and a diagonal. The tanks hang off the spine rather than sitting on it,
  // which keeps the truss light and the volume either side of each berth approach clear.
  for (const dy of [-5, 5]) strut(root, frame, [sx * 7, y + dy, -5], [sx * 41, y + dy, -5], 0.5, 6);
  strut(root, frame, [sx * 8, y - 16, -5], [sx * 40, y - 5, -5], 0.38, 6);
  box(root, frame, [9, 3.4, 8], [x, y - 5, -5]);

  // Main tank: 28 m of lagged methane, ringed at the quarter points where the shell is stiffest.
  lathe(root, insulation, [[0, -14], [4.2, -12.6], [6.2, -10], [6.2, 10], [4.2, 12.6], [0, 14]], 16, [x, y, -5]);
  for (const dy of [-7.5, 7.5]) tube(root, metal, 6.4, 6.4, 0.9, [x, y + dy, -5], 10);
  // Sump tank: the oxidiser run, smaller because a depot sells the methane and only carries the
  // LOX its own pumps need.
  box(root, frame, [6, 2.6, 9.4], [x, y - 4.6, 2.0]);
  lathe(root, insulation, [[0, -6], [3.6, -3.6], [3.6, 3.6], [0, 6]], 10, [x, y, 8.6]);
  tube(root, metal, 3.8, 3.8, 0.8, [x, y, 8.6], 10);
  strut(root, frame, [x - 3.4, y, 8.6], [x + 3.4, y, 8.6], 0.3, 6);

  // Manifold: the header every cluster taps, with the valve that isolates each tank from it.
  tube(root, copper, 0.7, 0.7, 31, [sx * 25.5, y, 2.6], 10, [0, 0, Math.PI / 2]);
  box(root, copper, [1.6, 2.0, 1.6], [x - sx * 6, y, 2.6]);
  for (const dy of [-6, 6]) strut(root, copper, [x, y + dy, 1.4], [x, y + dy, 2.6], 0.3, 6);
  strut(root, copper, [x, y, 5.4], [x, y, 2.6], 0.28, 6);
  // Feeds the spine's cryo main, so the manifold and the trunk are one circuit, not two.
  strut(root, copper, [sx * 10, y, 2.6], [sx * 5.0, y, 3.6], 0.3, 6);
  // Vent stack on the outboard end of the main: the one place a cryo tank is allowed to blow off.
  box(root, metal, [2.0, 2.6, 2.0], [x, y + 15.2, -5]);
  tube(root, metal, 0.6, 0.6, 3.2, [x, y + 17.2, -5], 6);
}

/** Unpressurised machinery deck hung under the ventral face: pumps, valve actuators and spares. */
function machinery_deck(root: THREE.Object3D): void {
  bevelled(root, deckPlate, [26, 26, 1.2], [0, 12, -13.6], [0, 0, 0], { radius: 0.3, segments: 1 });
  for (let i = 0; i < 3; i++) {
    strut(root, frame, [-13, 2 + i * 10, -13.6], [13, 2 + i * 10, -13.6], 0.34, 6);
  }
  for (const side of [-1, 1]) for (const y of [-0.5, 24.5]) {
    strut(root, frame, [side * 11, y, -13.6], [side * 5, y, -5.4], 0.3, 6);
  }
  box(root, hullPaint, [5.4, 6.6, 4.4], [-7.5, 17, -10.6]);
  box(root, hullPaint, [5.4, 6.6, 4.4], [7.5, 17, -10.6]);
  box(root, dark, [3.6, 3.6, 3.6], [0, 21.5, -10.8]);
  tube(root, copper, 0.9, 0.9, 22, [-9.5, 10, -11.6], 10);
  // Rail and posts along the open edges: a bare plate with nothing at its rim reads as a slab, not
  // as somewhere a crew works in a suit.
  for (const side of [-1, 1]) {
    strut(root, frame, [side * 12.2, -0.5, -9.6], [side * 12.2, 24.5, -9.6], 0.22, 5);
    for (const y of [-0.5, 24]) strut(root, deckPlate, [side * 12.2, y, -13.0], [side * 12.2, y, -9.8], 0.22, 5);
  }
  hp(root, 'deck', [0, 12, -12.9]);
}

/** Power mast: solar wings on a short cross arm, offset aft of the tankage so nothing shades it. */
function power_mast(root: THREE.Object3D): void {
  tube(root, metal, 3.6, 3.6, 1.6, [0, -38, 7.4], 10);
  // The mast itself is a lathe about local +Y, turned dorsal: one surface from base to head.
  lathe(root, lightArmor, [[2.6, 1.6], [1.8, 8], [1.8, 22], [1.1, 26]], 12, [0, -38, 0], [Math.PI / 2, 0, 0]);
  tube(root, metal, 2.2, 2.2, 1.4, [0, -38, 26.4], 8);
  bevelled(root, lightArmor, [22, 1.0, 1.0], [0, -38, 24.5], [0, 0, 0], { radius: 0.4, segments: 1 });
  for (const side of [-1, 1]) {
    box(root, solarCell, [13, 5.4, 0.9], [side * 17, -38, 24.5]);
    strut(root, frame, [side * 10.5, -38, 24.5], [side * 23.5, -38, 24.5], 0.28, 6);
  }
  hp(root, 'mast', [0, -38, 26.4]);
}

/** Radiator bank: three blades spread along the spine on a dorsal header. Stacked across X they
 *  would read as one slab from every angle in the flight plane, which is where the player looks. */
function radiator_bank(root: THREE.Object3D): void {
  tube(root, metal, 1.5, 1.5, 30, [0, 30, 11.4], 12);
  for (const y of [17, 30, 43]) {
    box(root, dark, [16, 8.5, 0.6], [0, y, 11.8]);
    strut(root, copper, [-8.2, y - 3.9, 12.2], [8.2, y - 3.9, 12.2], 0.24, 6);
  }
  for (const side of [-1, 1]) strut(root, frame, [side * 5.6, 24, 7.0], [side * 1.6, 30, 11.4], 0.3, 6);
}

/**
 * Propellant transfer boom: the depot's reason to exist. It reaches outboard so a hull that does not
 * want to berth can hold station and take on fuel through a hose without ever matching rolls.
 */
function transfer_boom(root: THREE.Object3D): void {
  for (const dz of [-3, 3]) strut(root, frame, [6, 10, dz], [30, 10, dz], 0.5, 6);
  strut(root, frame, [8, 16, 0], [28, 10, 0], 0.34, 6);
  tube(root, metal, 1.6, 1.6, 18, [21, 10, 0], 12, [0, 0, Math.PI / 2]);
  bevelled(root, metal, [3.2, 3.2, 3.2], [31.5, 10, 0], [0, 0, 0], { radius: 0.5, segments: 1 });
  tube(root, hazardPaint, 2.2, 2.2, 1.0, [33.6, 10, 0], 10, [0, 0, Math.PI / 2]);
  for (const dz of [-2.2, 2.2]) box(root, copper, [1.6, 1.6, 1.6], [26, 10, dz]);
  hp(root, 'transfer', [34.4, 10, 0]);
}

export function build(): THREE.Object3D {
  const group = new THREE.Group(); group.name = 'station_depot';

  // Spine: three lattice bays berth to berth, with the fuel main and the cryo trunk running inside
  // it. The three ring frames below land between the truss battens, so the spine still reads as
  // subdivided; nothing else on the depot is pressurised, so it can be as light as it wants.
  truss_box(group, frame, 116, 12, 12, 3, 0.5);
  tube(group, dark, 2.6, 2.6, 132, [0, 0, 0], 14);
  for (const y of [-44, 0, 44]) tube(group, metal, 7.4, 7.4, 1.4, [0, y, 0], 10);
  for (const side of [-1, 1]) tube(group, copper, 0.85, 0.85, 104, [side * 4.6, 0, 3.6], 10);

  berth(group, 'A', 58, 1);
  berth(group, 'B', -58, -1);
  for (const sx of [-1, 1]) for (const sy of [-1, 1]) cluster(group, sx, sy);
  machinery_deck(group);
  power_mast(group);
  radiator_bank(group);
  transfer_boom(group);

  return group;
}

/** Panels, rivets and a hazard band, with the fuel stencils a tanker crew reads on the way in. */
export const maps: Maps = (() => {
  const kit = standard_maps({
    // The atlas tile spans the model's largest dimension, so a 142 m station wants a finer pitch
    // than a 60 m ship or its panel lines come out 14 m wide.
    seed: 118, panel: 0.035, rivets: 0.02, seams: 3, wear: 0.42, grime: 0.28, scorch: 1, damage: 0.08,
    stencils: [['FUEL', 0.12, 0.3]],
    hazard: { x: 0.05, y: 0.62, w: 0.9, h: 0.1, angle: Math.PI / 4 },
  });
  return {
    normal: kit.normal,
    roughness: kit.roughness,
    albedo: (ctx, size) => {
      kit.albedo?.(ctx, size);
      each_tile(ctx, size, (tile) => {
        // Three stencils, kept apart: the tiles are shared, so anything else sprayed in here lands
        // on the tankage too. The grime sits under the band, where the berth collars project.
        noise_wash(tile, { color: '#8a8578', alpha: 0.32, scale: 0.06, seed: 14, rect: { x: 0.05, y: 0.7, w: 0.9, h: 0.22 } });
        stencil_text(tile, 'TESSERA L1', { x: 0.1, y: 0.44, scale: 0.02 });
        stencil_text(tile, 'CH4/LOX', { x: 0.34, y: 0.06, scale: 0.022 });
      });
    },
  };
})();

// No pressurised volume anywhere on the depot, so no glass; the solar cells stay clean because a
// panel line sprayed across a cell is a dead cell.
export const meta = { name: 'station_depot', scale: 1, collider: 'auto', untextured: ['glass', 'solarCell'] };
