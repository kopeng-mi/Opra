// RCS quad block (PLAN-03 §7.1): the four-nozzle reaction-control casting a hull bolts on. Fleet
// axes as always: nose +Y, dorsal +Z, starboard +X, metres. The pads on the -Z face are the
// mounting face; the four bells fire out of the corner chamfers, one per quadrant, so one block
// gives the simulation pitch, yaw and roll authority instead of a pod per axis.
//
// The bells are `jet_nozzle`'s, each with the effect cone it owns: those cones are invisible and
// flagged `userData.effect`, so the exporter carries them to the sidecar as jet.0..jet.3 for the
// simulation to light while the thruster fires, and merges nothing but the bells and the casting.
import * as THREE from 'three';
import {
  bevelled, box, dark, each_tile, hazard_stripes, jet_nozzle, lightArmor, metal, scorch,
  standard_maps, stencil_text, tube, type Maps,
} from './prims';

/** The four quadrants, starboard-dorsal first, so jet.0..jet.3 walk the block instead of jumping. */
const QUADRANTS: number[][] = [[1, 1], [-1, 1], [-1, -1], [1, -1]];

export function build(): THREE.Object3D {
  const group = new THREE.Group(); group.name = 'rcs_block';
  const effects: THREE.Mesh[] = [];
  // One housing casting, bevelled: a raw box edge catches a highlight like a prop, and on a fighter
  // this block sits close enough to camera that the edge is what a pilot actually sees.
  bevelled(group, metal, [3.6, 3.6, 2.4], [0, 0, 0], [0, 0, 0], { radius: 0.5, segments: 1 });
  // Mounting face: a gasket plate and four pads, the only surfaces that touch the hull.
  box(group, dark, [3.0, 3.0, 0.18], [0, 0, -1.28]);
  for (const [qx, qy] of QUADRANTS) box(group, lightArmor, [0.95, 0.95, 0.5], [qx * 1.02, qy * 1.02, -1.5]);
  // Propellant inlet on the dorsal face with a harness clamp over it: a block that reads connected.
  tube(group, metal, 0.42, 0.42, 1.2, [0, 0, 1.5], 8, [Math.PI / 2, 0, 0]);
  box(group, dark, [1.6, 0.5, 0.5], [0, 1.4, 1.05]);
  // Bells on the corner chamfers, one per quadrant, each with its own hidden jet. `jet_nozzle`'s
  // bell flares toward -direction and its plume leaves the same side, so the outward vector goes in
  // negated: pass it as-is and the bell opens back into the ship with the plume buried in it.
  QUADRANTS.forEach(([qx, qy], index) => {
    jet_nozzle(group, effects, {
      name: `jet.${index}`,
      radius: 0.42,
      pos: [qx * 1.82, qy * 1.82, 0],
      direction: [-qx, -qy, 0],
      length: 0.9,
    });
  });
  return group;
}

/** Close-pitch panels and rivets, a hazard band under the mounting face and a quad stencil. */
export const maps: Maps = (() => {
  const kit = standard_maps({
    seed: 61, panel: 0.3, rivets: 0.1, seams: 1, wear: 0.3, grime: 0.26, scorch: 1,
    stencils: [['RCS 04', 0.09, 0.14]],
  });
  return {
    normal: kit.normal,
    roughness: kit.roughness,
    albedo: (ctx, size) => {
      kit.albedo?.(ctx, size);
      each_tile(ctx, size, (tile) => {
        hazard_stripes(tile, { x: 0.06, y: 0.74, w: 0.88, h: 0.14 }, { pitch: 0.04, angle: 0 });
        stencil_text(tile, 'NO STEP', { x: 0.5, y: 0.42, scale: 0.024 });
      });
    },
  };
})();

export const meta = { name: 'rcs_block', scale: 1, collider: 'auto' };
