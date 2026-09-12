// Landing leg (PLAN-03 §7.1, ship components): one three-segment gear — hip bracket, upper arm,
// knee, lower arm and footpad — with the hydraulic ram and drag brace that carry the load.
//
// It is authored standing on its pad with +Y up, not lying along the ship's ventral: a leg is
// mounted by its hip pivot, and the hip pivot is the origin. That choice is what makes `hp.contact`
// honest — the node sits on the pad's underside with its own +Y straight out of the pad, so the
// ground point and the ground normal are one node instead of a position plus a caveat. An editor
// hanging this on a hull rotates the whole asset at the hip until -Y is ventral.
import * as THREE from 'three';
import {
  ceramic, dark, frame, metal,
  bevelled, box, damage_relief, each_tile, hazard_stripes, hp, lathe, standard_maps, stencil_text,
  strut, tube, type Maps,
} from './prims';

export function build(): THREE.Object3D {
  const group = new THREE.Group(); group.name = 'landing_leg';

  const knee = [1.5, -4.3, 0];
  const foot = [-0.6, -8.0, 0];

  // ---- hip --------------------------------------------------------------------------------------
  // The casting bolts to the hull and carries the pivot pin: everything below hangs off this one
  // joint, so it is the heaviest piece of the leg.
  bevelled(group, dark, [2.4, 3.0, 3.0], [0, 0.7, 0], [0, 0, 0], { radius: 0.4, segments: 1 });
  tube(group, metal, 0.45, 0.45, 3.6, [0, 0, 0], 10, [Math.PI / 2, 0, 0]);
  for (const side of [-1, 1]) box(group, metal, [0.3, 1.6, 1.6], [0, 0, side * 1.8]);

  // ---- upper arm --------------------------------------------------------------------------------
  // Two plates either side of the pivot rather than one bar: it is the fork a real gear uses, and it
  // keeps the knee's clevis from having to be invented separately.
  for (const side of [-1, 1]) strut(group, metal, [0, -0.5, side * 0.85], [knee[0], knee[1] + 0.3, side * 0.85], 0.42, 8);

  // ---- knee -------------------------------------------------------------------------------------
  tube(group, metal, 0.62, 0.62, 2.6, knee, 12, [Math.PI / 2, 0, 0]);
  for (const side of [-1, 1]) box(group, dark, [1.0, 1.6, 0.3], [knee[0], knee[1], side * 1.35], 0.3);

  // ---- lower arm and pad ------------------------------------------------------------------------
  for (const side of [-1, 1]) strut(group, metal, [knee[0], knee[1] - 0.2, side * 0.6], [foot[0], foot[1], side * 0.6], 0.36, 8);
  box(group, dark, [1.6, 1.4, 1.4], [foot[0], foot[1] + 0.5, 0]);
  // Pad: a dished bell with a flat underside, so the contact point is a face and not a rim.
  lathe(group, dark, [
    [0.9, 0.55], [2.0, 0.25], [2.45, -0.15], [2.35, -0.5], [1.8, -0.55], [0, -0.55],
  ], 14, [foot[0], foot[1], 0]);
  tube(group, metal, 2.5, 2.5, 0.35, [foot[0], foot[1] - 0.05, 0], 12);
  // The point that touches the ground: the middle of the pad's underside, +Y up out of it.
  hp(group, 'contact', [foot[0], foot[1] - 0.55, 0]);

  // ---- hydraulic ram ----------------------------------------------------------------------------
  // Body, polished rod and a clevis at each end: the ram is what takes the landing load, so it runs
  // from the hip casting to the knee rather than anywhere convenient.
  box(group, frame, [0.8, 0.7, 1.0], [-1.35, -0.45, 0]);
  strut(group, metal, [-1.25, -0.6, 0], [-0.2, -2.5, 0], 0.42, 10);
  strut(group, ceramic, [-0.2, -2.5, 0], [knee[0] - 0.6, knee[1] + 0.4, 0], 0.2, 8);
  box(group, frame, [0.7, 0.7, 0.8], [knee[0] - 0.6, knee[1] + 0.4, 0]);

  // ---- drag brace and torque link ---------------------------------------------------------------
  // The brace triangulates the upper arm so the knee cannot fold past its stop; the link keeps the
  // lower arm's angle off the ram, so a blown seal still lands the ship.
  strut(group, frame, [-0.5, -1.1, 1.2], [knee[0] - 0.3, knee[1] + 0.7, 1.2], 0.16, 6);
  box(group, frame, [0.7, 0.6, 0.4], [-0.5, -1.1, 1.4]);
  box(group, frame, [0.7, 0.6, 0.4], [knee[0] - 0.3, knee[1] + 0.7, 1.4]);
  strut(group, frame, [knee[0] + 0.4, knee[1] - 0.6, 0.9], [foot[0] + 0.7, foot[1] + 1.5, 0.9], 0.14, 6);
  return group;
}

/** Coarse panel lines, a worn gear's worth of scuffs, and the jacking and stand-clear stencils a
 *  ground crew works to. */
export const maps: Maps = (() => {
  const kit = standard_maps({
    seed: 31, panel: 0.1, rivets: 0.055, seams: 3, wear: 0.44, grime: 0.34, scorch: 1,
    stencils: [['LEG-3', 0.1, 0.17]],
    hazard: { x: 0.04, y: 0.76, w: 0.92, h: 0.16, angle: Math.PI / 3 },
  });
  return {
    normal: kit.normal,
    roughness: kit.roughness,
    albedo: (ctx, size) => {
      kit.albedo?.(ctx, size);
      each_tile(ctx, size, (tile) => {
        // Gear wears against the ground, not against the sky: grit, a jacking mark and the paint the
        // hydraulics have already taken off.
        stencil_text(tile, 'JACK', { x: 0.58, y: 0.34, scale: 0.026, mirror: true });
        stencil_text(tile, 'NO STEP', { x: 0.16, y: 0.6, scale: 0.02 });
        hazard_stripes(tile, { x: 0.34, y: 0.08, w: 0.6, h: 0.1 }, { pitch: 0.04, angle: -Math.PI / 4 });
        damage_relief(tile, { amount: 0.12, seed: 43, albedo: true });
      });
    },
  };
})();

export const meta = { name: 'landing_leg', scale: 1, collider: 'auto' };
