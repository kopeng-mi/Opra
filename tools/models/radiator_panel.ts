// Deployable radiator panel (plan-04 §2.4, ref 14, deployed state): a 6.0 × 3.0 m wing on a
// hinged root with its actuator. Origin on the hinge axis with the barrel running along +X
// and the panel extending +Y, so stowing and deploying the thing is one rotation of the whole
// asset about its own root edge.
//
// `hp.hinge` sits at the origin with its +Y along the barrel: the fold the editor is expected
// to drive is a rotation about the hinge node's local +Y (equivalently the model's +X axis).
// `hp.tip` marks the outboard end, so a second panel or a tie rod can anchor to the tip.
import * as THREE from 'three';
import {
  ceramic, copper, dark, frame, insulation, metal,
  box, damage_relief, each_tile, hazard_stripes, hp, standard_maps, stencil_text, strut, tube, type Maps,
} from './prims';

/** Deployed panel: 3.0 m of hinge, 6.0 m of wing. */
const SPAN = 3.0;
const CHORD = 6.0;

export function build(): THREE.Object3D {
  const group = new THREE.Group(); group.name = 'radiator_panel';
  const root = CHORD / 2 + 0.45;

  // Hinge barrel, two hull bearings and the arms that carry the panel off it. The barrel is
  // the pivot, so nothing else may straddle the axis at the origin.
  tube(group, metal, 0.24, 0.24, SPAN - 0.4, [0, 0, 0], 12, [0, 0, Math.PI / 2]);
  for (const x of [-SPAN / 2 + 0.2, SPAN / 2 - 0.2]) {
    box(group, dark, [0.5, 0.8, 0.75], [x, 0.25, 0]);
    box(group, frame, [0.5, 0.35, 0.6], [x, -0.4, 0]);
    strut(group, metal, [x * 0.6, 0.1, 0], [x, 1.1, 0], 0.18, 6);
  }
  // Actuator between the hull fitting and the panel root: off the fold axis, so folding
  // changes its length — the whole reason it is there instead of a fixed brace.
  box(group, frame, [0.55, 0.3, 0.45], [-SPAN / 2 + 0.2, -0.25, 0]);
  strut(group, metal, [-SPAN / 2 + 0.2, -0.25, 0], [-SPAN / 2 + 0.55, 1.25, 0], 0.08, 5);
  tube(group, ceramic, 0.14, 0.14, 1.0, [-SPAN / 2 + 0.375, 0.5, 0], 8, [0, 0, -0.26]);
  box(group, frame, [0.5, 0.35, 0.35], [-SPAN / 2 + 0.55, 1.3, 0]);

  // Panel: two faces on a spine with edge rails. Radiator area is the point of the asset, so
  // the faces are wide, flat and unbroken, and the wing stays thin enough to read as a wing.
  box(group, dark, [SPAN - 0.3, 0.7, 0.8], [0, 0.9, 0]);
  box(group, metal, [0.8, CHORD - 0.6, 0.55], [0, root, 0]);
  for (const z of [-0.32, 0.32]) {
    box(group, insulation, [SPAN - 0.4, CHORD - 0.6, 0.14], [0, root, z]);
  }
  // Coolant runs: two per face with a header crossing at each end.
  for (const z of [-0.42, 0.42]) {
    for (const x of [-SPAN / 2 + 0.45, SPAN / 2 - 0.45])
      tube(group, copper, 0.09, 0.09, CHORD - 0.8, [x, root, z], 8);
    for (const y of [root - CHORD / 2 + 0.5, root + CHORD / 2 - 0.5])
      tube(group, copper, 0.11, 0.11, SPAN - 0.5, [0, y, z], 8, [0, 0, Math.PI / 2]);
  }
  // Edge rails, outboard end beam and the latch lugs that hold the wing down while stowed.
  for (const x of [-SPAN / 2 + 0.06, SPAN / 2 - 0.06]) box(group, dark, [0.18, CHORD - 0.5, 0.7], [x, root, 0]);
  box(group, dark, [SPAN - 0.2, 0.2, 0.7], [0, root - CHORD / 2 + 0.35, 0]);
  box(group, frame, [SPAN - 0.1, 0.45, 0.75], [0, root + CHORD / 2 - 0.2, 0]);
  for (const x of [-1.0, 1.0]) box(group, metal, [0.35, 0.3, 0.3], [x, root + CHORD / 2 + 0.05, 0]);

  // Barrel axis: the deploy fold is a rotation about this node's local +Y.
  hp(group, 'hinge', [0, 0, 0], [0, 0, -Math.PI / 2]);
  hp(group, 'tip', [0, root + CHORD / 2 + 0.1, 0]);
  return group;
}

/** Panel lines coarse enough to survive the planar projection, a hinge-end hazard band, and
 *  the stencils a ground crew needs before it vents a hundred square metres of coolant. */
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
        stencil_text(tile, 'HOT', { x: 0.56, y: 0.3, scale: 0.028 });
        stencil_text(tile, 'STAND CLEAR', { x: 0.2, y: 0.56, scale: 0.017 });
        hazard_stripes(tile, { x: 0.06, y: 0.44, w: 0.32, h: 0.09 }, { pitch: 0.04, angle: Math.PI / 4 });
        damage_relief(tile, { amount: 0.05, seed: 19, albedo: true });
      });
    },
  };
})();

export const meta = { name: 'radiator_panel', scale: 1, collider: 'auto' };
