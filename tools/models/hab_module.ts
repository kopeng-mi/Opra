// Habitat module (PLAN-03 §7.1, ship components): a pressurised drum for the modular hull kit — the
// piece a station or a long-haul ship is assembled from. Origin at the module's centre, long axis on
// +Y so it stacks with the ships the way every other asset does, and the docking lobe on the
// starboard flank, which is what makes `dock.side` a side berth rather than another nose port.
//
// The shape is one surface of revolution: barrel with domed end caps, so the pressure shell is
// smooth everywhere it would really be smooth. Everything else is a thing you can touch from
// outside — a window band you can see into, handrails and handholds you can clip a tether to, a
// hatch with a wheel, a supply locker, and the utility runs a module needs to be hooked up.
import * as THREE from 'three';
import {
  black, copper, dark, frame, glass, hullPaint, insulation, metal,
  bevelled, box, dock, each_tile, hazard_stripes, lamps, lathe, standard_maps, stencil_text, strut,
  torus, tube, type Maps,
} from './prims';

export function build(): THREE.Object3D {
  const group = new THREE.Group(); group.name = 'hab_module';

  // ---- pressure shell ---------------------------------------------------------------------------
  // Both caps are domes; the forward one is truncated at radius 3.4 into a flat bulkhead, because
  // that is the face the module stacks against and a dome has nothing to bolt to. The aft cap
  // keeps its full apex, which is what a pressure vessel's far end looks like.
  lathe(group, hullPaint, [
    [0, -12], [3.0, -11.4], [4.8, -10.2], [5.8, -8.6], [6.0, -6.0],
    [6.0, 6.0], [5.8, 8.6], [4.8, 10.2], [3.8, 11.0], [3.4, 11.2], [0, 11.2],
  ], 22);
  // Two stiffening bands, on the barrel's shoulders where a real frame would be: they also break
  // the shell's silhouette so the domes read as domes.
  for (const y of [-7.2, 7.2]) tube(group, dark, 6.15, 6.15, 0.9, [0, y, 0], 16);

  // ---- forward mating interface -----------------------------------------------------------------
  // The bulkhead's ring and four lugs: a 3.2 x 3.8 m pattern (x = +-1.6, z = +-1.9), each lug a
  // 0.7 x 0.5 x 0.7 block with its top 0.35 m proud of the face and a 0.18 m shear pin through it.
  // This is the pattern `cargo_pod`'s -Y mounting face carries, so the two stack on one axis.
  tube(group, dark, 3.4, 3.4, 0.5, [0, 11.35, 0], 16);
  for (const x of [-1.6, 1.6]) for (const z of [-1.9, 1.9]) {
    box(group, metal, [0.7, 0.5, 0.7], [x, 11.3, z]);
    tube(group, frame, 0.18, 0.18, 0.9, [x, 11.35, z], 6, [Math.PI / 2, 0, 0]);
  }

  // ---- window band ------------------------------------------------------------------------------
  // Eight round-rect windows in two rows down the flanks. Each is four bars and a pane: the bars
  // stand 0.25 m proud of the shell and the pane sits behind their face, so the recess is geometry
  // rather than a painted ring. The pane keeps the shared `glass`, which the map binder skips, so
  // no panel line or stencil ever lands across a window.
  for (const theta of [0.44, -0.44, Math.PI - 0.44, Math.PI + 0.44]) {
    for (const y of [-4.0, 4.0]) {
      const window = new THREE.Group();
      window.position.set(Math.cos(theta) * 6.0, y, Math.sin(theta) * 6.0);
      window.rotation.y = -theta;                 // the group's local +X is the outward normal
      group.add(window);
      for (const side of [-1, 1]) {
        box(window, dark, [0.5, 2.9, 0.3], [0, 0, side * 0.9]);
        box(window, dark, [0.5, 0.3, 1.5], [0, side * 1.25, 0]);
      }
      box(window, glass, [0.3, 2.3, 1.6], [0, 0, 0]);
    }
  }

  // ---- docking lobe, starboard ------------------------------------------------------------------
  // Collar, flange and hatch face: `dock.side` sits on the hatch face with its +Y out along +X, so
  // the sidecar records the port's normal as (1, 0) in the flight plane.
  lathe(group, metal, [[2.4, 0], [2.4, 1.8], [2.05, 2.3], [1.95, 2.5]], 16, [4.8, 0, 0], [0, 0, -Math.PI / 2]);
  tube(group, dark, 2.5, 2.5, 0.5, [6.1, 0, 0], 16, [0, 0, Math.PI / 2]);
  lathe(group, dark, [[1.9, 0], [1.9, 0.3], [1.6, 0.5], [0, 0.6]], 14, [7.3, 0, 0], [0, 0, -Math.PI / 2]);
  torus(group, frame, 0.6, 0.11, [7.95, 0, 0], [0, Math.PI / 2, 0], 4, 8);
  // Docking target marks: four blocks spaced round the collar, the aiming reference an approaching
  // pilot lines up on.
  for (const at of [[1.7, 1.7], [1.7, -1.7], [-1.7, 1.7], [-1.7, -1.7]]) {
    box(group, insulation, [0.5, 0.5, 0.5], [6.1, at[0], at[1]]);
  }
  dock(group, 'side', [8.0, 0, 0], -Math.PI / 2, 'M');

  // ---- main hatch, port side --------------------------------------------------------------------
  // Frame ring, dished door, grab bar and two hinges: what makes a flat disc read as a door a crew
  // has to dog down.
  const hatch = new THREE.Group();
  hatch.position.set(-5.9, 0, 0);
  hatch.rotation.y = Math.PI;                     // local +X is the outward (-X) normal
  group.add(hatch);
  torus(hatch, frame, 1.5, 0.28, [0.1, 0, 0], [0, Math.PI / 2, 0], 4, 10);
  lathe(hatch, dark, [[1.35, 0], [1.35, 0.25], [1.05, 0.42], [0, 0.52]], 14, [0, 0, 0], [0, 0, -Math.PI / 2]);
  box(hatch, metal, [0.25, 1.3, 0.2], [0.62, 0, 0]);
  for (const side of [-1, 1]) box(hatch, metal, [0.35, 0.22, 0.8], [0.1, side * 1.5, 0.55]);

  // ---- supply locker, dorsal --------------------------------------------------------------------
  const locker = new THREE.Group();
  locker.position.set(0, 7.4, 5.75);
  locker.rotation.y = -Math.PI / 2;               // local +X is the outward (+Z) normal
  group.add(locker);
  bevelled(locker, hullPaint, [1.5, 2.8, 3.4], [0, 0, 0], [0, 0, 0], { radius: 0.3, segments: 1 });
  box(locker, dark, [0.3, 2.4, 3.0], [0.82, 0, 0]);
  for (const side of [-1, 1]) box(locker, metal, [0.16, 0.3, 0.7], [0.85, side * 1.1, 1.45]);
  box(locker, metal, [0.2, 0.5, 0.2], [0.95, 0, -1.2]);
  box(locker, black, [0.2, 0.6, 1.6], [0.95, -1.5, 0]);

  // ---- handrails and handholds ------------------------------------------------------------------
  // Two rails run the barrel's length just off the dorsal shoulder, held by stanchions: the run a
  // crew crosses the hull on. Short grabs cluster round the hatch and the locker where a suit has
  // to steady itself to turn a dog handle. Each grab is its own little frame — a standoff and a
  // tangential bar — because a bar drawn in model axes would sit at an angle to the shell.
  for (const theta of [1.2, 1.94]) {
    const x = Math.cos(theta), z = Math.sin(theta);
    tube(group, metal, 0.13, 0.13, 14.4, [x * 6.65, 0, z * 6.65], 8);
    for (let i = 0; i < 4; i++) {
      const y = -6.6 + i * 4.4;
      strut(group, metal, [x * 6.0, y, z * 6.0], [x * 6.6, y, z * 6.6], 0.12, 5);
    }
  }
  for (const at of [[Math.PI - 0.18, -2.6], [Math.PI + 0.18, -2.6], [Math.PI - 0.18, 2.6], [Math.PI + 0.18, 2.6], [1.05, 6.0]]) {
    const theta = at[0], y = at[1];
    const grab = new THREE.Group();
    grab.position.set(Math.cos(theta) * 6.0, y, Math.sin(theta) * 6.0);
    grab.rotation.y = -theta;                   // local +X out of the shell, local Z tangential
    group.add(grab);
    strut(grab, frame, [0, 0, 0], [0.6, 0, 0], 0.1, 5);
    box(grab, frame, [0.16, 0.16, 0.9], [0.64, 0, 0]);
  }

  // ---- utility runs -----------------------------------------------------------------------------
  // Coolant and feed lines along the ventral, clamped to the shell: a module without them is a
  // balloon, not a piece of a ship.
  for (const theta of [-Math.PI / 2 - 0.3, -Math.PI / 2 + 0.3]) {
    const x = Math.cos(theta), z = Math.sin(theta);
    tube(group, copper, 0.16, 0.16, 12.4, [x * 6.35, 0, z * 6.35], 8);
    for (const y of [-6.2, -0.5, 5.2]) {
      box(group, frame, [0.3, 0.3, 0.7], [x * 6.2, y, z * 6.2]);
    }
  }

  // Marker lamps: two on the lobe collar and two on the aft shoulder, so the module reads as an
  // approachable object from either end. Each sits proud of the surface it is on.
  lamps(group, '#b7dfdd', [[6.1, 2.7, 0], [6.1, -2.7, 0], [4.2, -8.4, -4.2], [-4.2, -8.4, -4.2]], 0.42, 6);
  return group;
}

/** Panels, rivets and the module's own stencils: the drum number, the airlock warning and a hazard
 *  band at the docking end. */
export const maps: Maps = (() => {
  const kit = standard_maps({
    seed: 53, panel: 0.17, rivets: 0.085, seams: 3, wear: 0.3, grime: 0.24, scorch: 1,
    stencils: [['HAB-2', 0.09, 0.16], ['AIRLOCK', 0.3, 0.6]],
    hazard: { x: 0.05, y: 0.78, w: 0.9, h: 0.15, angle: Math.PI / 6 },
  });
  return {
    normal: kit.normal,
    roughness: kit.roughness,
    albedo: (ctx, size) => {
      kit.albedo?.(ctx, size);
      each_tile(ctx, size, (tile) => {
        // Docking and EVA handling marks: what a suit reads while it is outside.
        stencil_text(tile, 'DOCK', { x: 0.6, y: 0.24, scale: 0.024, mirror: true });
        stencil_text(tile, 'EVA', { x: 0.12, y: 0.44, scale: 0.026 });
        hazard_stripes(tile, { x: 0.06, y: 0.42, w: 0.36, h: 0.1 }, { pitch: 0.042, angle: Math.PI / 4 });
      });
    },
  };
})();

/** The panes keep the shared `glass`; naming it here states the intent where the loader reads it. */
export const meta = { name: 'hab_module', scale: 1, collider: 'auto', untextured: ['glass'] };
