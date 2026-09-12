// Habitat module (plan-04 §2.5, ref 17): the hab variant of the 4.0 m / 2.5 m flange family.
// Two round ports, one rectangular hatch, the teal/ochre identity band — and the shared flange
// at BOTH ends, identical part, identical bolt pattern.
//
// Axis +Y, origin at the module centre. The previous 24 m drum with its side dock is gone: a
// hull module stacks nose to tail, and a side berth on a stackable section was a second
// interface doing the flange's job.
import * as THREE from 'three';
import {
  black, copper, dark, frame, glass, metal, ochre, teal,
  bevelled, box, each_tile, flange, hazard_stripes, lamps, lathe, standard_maps,
  stencil_text, strut, torus, tube, type Maps,
} from './prims';

/** Family envelope: 4.0 m flange to flange, 2.5 m across. */
const HALF = 2.0;
const RADIUS = 1.25;

export function build(): THREE.Object3D {
  const group = new THREE.Group(); group.name = 'hab_module';

  // Pressure shell: barrel with shallow domes tucked just inside the flange faces.
  lathe(group, metal, [
    [0, -1.95], [0.7, -1.9], [1.05, -1.72], [RADIUS, -1.45],
    [RADIUS, 1.45], [1.05, 1.72], [0.7, 1.9], [0, 1.95],
  ], 20);
  // Stiffening bands on the barrel's shoulders: where a real frame would be, and the break
  // that keeps the shell from reading as a plain tube.
  for (const y of [-1.35, 1.35]) tube(group, dark, RADIUS + 0.06, RADIUS + 0.06, 0.22, [0, y, 0], 20);
  // Identity band: teal aft of ochre, the marking every hull in the fleet carries.
  tube(group, teal, RADIUS + 0.03, RADIUS + 0.03, 0.34, [0, 0.75, 0], 20);
  tube(group, ochre, RADIUS + 0.03, RADIUS + 0.03, 0.14, [0, 0.5, 0], 20);

  // Two round ports on opposite flanks: a frame ring standing proud, the pane recessed behind
  // it so the recess is geometry rather than a painted ring. `glass` stays out of the maps.
  for (const theta of [0.6, 0.6 + Math.PI]) {
    const port = new THREE.Group();
    port.position.set(Math.cos(theta) * RADIUS, -0.5, Math.sin(theta) * RADIUS);
    port.rotation.y = -theta;
    group.add(port);
    torus(port, frame, 0.42, 0.09, [0, 0, 0], [0, Math.PI / 2, 0], 5, 12);
    lathe(port, dark, [[0.42, 0], [0.42, 0.08], [0.36, 0.12], [0, 0.14]], 12, [0, 0, 0], [0, 0, -Math.PI / 2]);
    box(port, glass, [0.08, 0.62, 0.62], [-0.02, 0, 0]);
  }
  // One rectangular hatch on the dorsal shoulder: frame, dished door, grab bar, hinges — what
  // makes a flat plate read as a door a crew has to dog down.
  const hatch = new THREE.Group();
  hatch.position.set(0, 0.2, RADIUS);
  group.add(hatch);
  bevelled(hatch, frame, [1.1, 1.5, 0.16], [0, 0, 0.02], [0, 0, 0], { radius: 0.06, segments: 1 });
  box(hatch, dark, [0.85, 1.2, 0.1], [0, 0, 0.1]);
  box(hatch, metal, [0.5, 0.08, 0.08], [0, 0.2, 0.18]);
  for (const side of [-1, 1]) box(hatch, metal, [0.12, 0.2, 0.12], [side * 0.42, 0, 0.12]);
  // Utility run along the ventral: coolant and feed lines clamped to the shell, without which
  // a module is a balloon rather than a piece of a ship.
  tube(group, copper, 0.07, 0.07, 2.9, [0, 0, -RADIUS - 0.08], 8);
  for (const y of [-1.2, 0, 1.2]) box(group, frame, [0.14, 0.14, 0.2], [0, y, -RADIUS - 0.06]);
  // Handholds round the hatch, where a suit steadies itself to turn a dog handle.
  for (const x of [-0.8, 0.8]) {
    strut(group, frame, [x, 1.0, RADIUS - 0.05], [x, 1.0, RADIUS + 0.25], 0.045, 4);
    box(group, frame, [0.07, 0.07, 0.4], [x, 1.0, RADIUS + 0.27]);
  }
  // Marker lamps fore and aft on the ventral, so the module reads as approachable either end.
  lamps(group, '#b7dfdd', [[0.8, -1.6, -0.9], [-0.8, -1.6, -0.9], [0.8, 1.6, -0.9], [-0.8, 1.6, -0.9]], 0.12, 6);

  // The interface, both ends, one part.
  flange(group, metal, [0, -HALF, 0], [0, -1, 0]);
  flange(group, metal, [0, HALF, 0], [0, 1, 0]);
  return group;
}

/** Panels, rivets and the module's own stencils: the drum number and an EVA mark. */
export const maps: Maps = (() => {
  const kit = standard_maps({
    seed: 53, panel: 0.17, rivets: 0.085, seams: 3, wear: 0.3, grime: 0.24, scorch: 1,
    stencils: [['HAB-2', 0.09, 0.16]],
    hazard: { x: 0.05, y: 0.78, w: 0.9, h: 0.15, angle: Math.PI / 6 },
  });
  return {
    normal: kit.normal,
    roughness: kit.roughness,
    albedo: (ctx, size) => {
      kit.albedo?.(ctx, size);
      each_tile(ctx, size, (tile) => {
        stencil_text(tile, 'EVA', { x: 0.12, y: 0.44, scale: 0.026 });
        hazard_stripes(tile, { x: 0.06, y: 0.42, w: 0.36, h: 0.1 }, { pitch: 0.042, angle: Math.PI / 4 });
      });
    },
  };
})();

/** The panes keep the shared `glass`; naming it here states the intent where the loader reads it. */
export const meta = { name: 'hab_module', scale: 1, collider: 'auto', untextured: ['glass'] };
