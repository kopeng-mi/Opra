// Deployable radiator panel (PLAN-03 §7.1, ship components): the hinged wing a ship dumps heat
// through. Origin on the hinge axis with the barrel running along +X and the panel extending +Y, so
// stowing and deploying the thing is one rotation of the whole asset about its own root edge.
//
// `hp.hinge` sits at the origin with its +Y along the barrel: the fold the editor is expected to
// drive is a rotation about the hinge node's local +Y (equivalently the model's +X axis), which
// swings the panel from lying in the hull's plane to standing off it like a fin. `hp.tip` marks the
// outboard end, so a second panel or a tie rod can be anchored to the tip without re-measuring.
import * as THREE from 'three';
import {
  ceramic, copper, dark, frame, insulation, metal,
  box, damage_relief, each_tile, hazard_stripes, hp, standard_maps, stencil_text, strut, tube, type Maps,
} from './prims';

export function build(): THREE.Object3D {
  const group = new THREE.Group(); group.name = 'radiator_panel';

  // ---- hinge ------------------------------------------------------------------------------------
  // Barrel, two hull bearings and the arms that carry the panel off it. The barrel is the pivot, so
  // nothing else may straddle the axis at the origin: the bearings sit outboard of its ends.
  tube(group, metal, 0.32, 0.32, 3.4, [0, 0, 0], 12, [0, 0, Math.PI / 2]);
  for (const x of [-1.5, 1.5]) {
    box(group, dark, [0.7, 1.1, 1.0], [x, 0.35, 0]);
    box(group, frame, [0.7, 0.5, 0.8], [x, -0.5, 0]);
    strut(group, metal, [x * 0.6, 0.1, 0], [x, 1.45, 0], 0.26, 6);
  }
  // Actuator between the hull fitting and the panel root: off the fold axis, so folding changes its
  // length, which is the whole reason it is there instead of a fixed brace.
  box(group, frame, [0.8, 0.4, 0.6], [-1.3, -0.3, 0]);
  strut(group, metal, [-1.3, -0.3, 0], [-0.9, 1.6, 0], 0.11, 5);
  tube(group, ceramic, 0.2, 0.2, 1.4, [-1.156, 0.386, 0], 8, [0, 0, -0.2075]);
  box(group, frame, [0.7, 0.5, 0.5], [-0.9, 1.7, 0]);

  // ---- panel ------------------------------------------------------------------------------------
  // Two faces on a spine and a rim of edge rails: radiator area is the point of the asset, so the
  // faces are wide, flat and unbroken, and the panel stays thin enough to read as a wing.
  box(group, dark, [3.6, 0.9, 1.0], [0, 1.2, 0]);
  box(group, metal, [1.0, 5.6, 0.7], [0, 4.9, 0]);
  for (const z of [-0.4, 0.4]) {
    box(group, insulation, [3.4, 5.6, 0.2], [0, 4.9, z]);
  }
  // Coolant tube runs: two per face, with a header crossing at each end.
  for (const z of [-0.52, 0.52]) {
    for (const x of [-1.1, 1.1]) tube(group, copper, 0.13, 0.13, 5.4, [x, 4.9, z], 8);
    for (const y of [2.1, 7.7]) tube(group, copper, 0.16, 0.16, 3.4, [0, y, z], 8, [0, 0, Math.PI / 2]);
  }
  // Edge rails and the outboard end beam.
  for (const x of [-1.78, 1.78]) box(group, dark, [0.25, 5.7, 0.9], [x, 4.9, 0]);
  box(group, dark, [3.8, 0.25, 0.9], [0, 2.05, 0]);
  box(group, frame, [3.9, 0.6, 1.0], [0, 7.9, 0]);
  // Latch lugs on the end beam: what holds the wing down while it is stowed.
  for (const x of [-1.4, 1.4]) box(group, metal, [0.5, 0.4, 0.4], [x, 8.3, 0]);

  // ---- anchors ----------------------------------------------------------------------------------
  // Barrel axis: the deploy fold is a rotation about this node's local +Y.
  hp(group, 'hinge', [0, 0, 0], [0, 0, -Math.PI / 2]);
  hp(group, 'tip', [0, 8.35, 0]);
  return group;
}

/** Panel lines coarse enough to survive the planar projection, a hinge-end hazard band, and the
 *  stencils a ground crew needs before it vents a hundred square metres of coolant. */
export const maps: Maps = (() => {
  const kit = standard_maps({
    seed: 67, panel: 0.055, rivets: 0.03, seams: 2, wear: 0.26, grime: 0.16,
    stencils: [['RAD-1', 0.11, 0.18]],
    hazard: { x: 0.04, y: 0.84, w: 0.92, h: 0.1, angle: -Math.PI / 4 },
  });
  return {
    normal: kit.normal,
    roughness: kit.roughness,
    albedo: (ctx, size) => {
      kit.albedo?.(ctx, size);
      each_tile(ctx, size, (tile) => {
        // The one warning that matters on a radiator, plus the crew stand-off marks at the hinge.
        stencil_text(tile, 'HOT', { x: 0.56, y: 0.3, scale: 0.028 });
        stencil_text(tile, 'STAND CLEAR', { x: 0.2, y: 0.56, scale: 0.017 });
        hazard_stripes(tile, { x: 0.06, y: 0.44, w: 0.32, h: 0.09 }, { pitch: 0.04, angle: Math.PI / 4 });
        damage_relief(tile, { amount: 0.05, seed: 19, albedo: true });
      });
    },
  };
})();

export const meta = { name: 'radiator_panel', scale: 1, collider: 'auto' };
