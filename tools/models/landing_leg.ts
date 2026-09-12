// Landing leg (plan-04 §2.4, ref 15): one gear — hip bracket, upper arm, knee, lower arm and
// footpad — with the hydraulic ram and drag brace that carry the load. 3.5 m extended.
//
// It is authored standing on its pad with +Y up, not lying along the ship's ventral: a leg is
// mounted by its hip pivot, and the hip pivot is the origin. `hp.contact` sits on the pad's
// underside with its own +Y straight out of the pad, so the ground point and the ground
// normal are one node instead of a position plus a caveat.
import * as THREE from 'three';
import {
  ceramic, dark, frame, metal,
  bevelled, box, damage_relief, each_tile, effect_cone, hazard_stripes, hp, lathe,
  standard_maps, stencil_text, strut, tube, type Maps,
} from './prims';

/** Hip pivot to pad underside, extended. */
const DROP = 3.5;

export function build(): THREE.Object3D {
  const group = new THREE.Group(); group.name = 'landing_leg';
  const effects: THREE.Mesh[] = [];

  const knee = [0.65, -1.9, 0];
  const foot = [-0.25, -DROP + 0.3, 0];

  // Hip: the casting that bolts to the hull and carries the pivot pin — the heaviest piece,
  // because everything below hangs off this one joint.
  bevelled(group, dark, [1.1, 1.3, 1.3], [0, 0.35, 0], [0, 0, 0], { radius: 0.18, segments: 1 });
  tube(group, metal, 0.2, 0.2, 1.6, [0, 0, 0], 10, [Math.PI / 2, 0, 0]);
  for (const side of [-1, 1]) box(group, metal, [0.14, 0.7, 0.7], [0, 0, side * 0.8]);

  // Upper arm: a fork of two plates either side of the pivot, the way a real gear is built.
  for (const side of [-1, 1])
    strut(group, metal, [0, -0.2, side * 0.38], [knee[0], knee[1] + 0.15, side * 0.38], 0.18, 8);

  // Knee: pin housing with cheek plates, and the ram clevis it drives against.
  tube(group, metal, 0.27, 0.27, 1.15, knee, 12, [Math.PI / 2, 0, 0]);
  for (const side of [-1, 1]) box(group, dark, [0.45, 0.7, 0.14], [knee[0], knee[1], side * 0.6], 0.25);

  // Lower arm and pad: twin struts into a dished pad with a flat underside, so the contact
  // point is a face and not a rim.
  for (const side of [-1, 1])
    strut(group, metal, [knee[0], knee[1] - 0.1, side * 0.27], [foot[0], foot[1], side * 0.27], 0.15, 8);
  box(group, dark, [0.7, 0.6, 0.6], [foot[0], foot[1] + 0.25, 0]);
  lathe(group, dark, [
    [0.4, 0.28], [0.9, 0.12], [1.1, -0.08], [1.05, -0.24], [0.8, -0.27], [0, -0.27],
  ], 14, [foot[0], foot[1], 0]);
  tube(group, metal, 1.12, 1.12, 0.16, [foot[0], foot[1] - 0.02, 0], 12);
  // The point that touches the ground: the middle of the pad's underside, +Y up out of it.
  hp(group, 'contact', [foot[0], foot[1] - 0.27, 0]);

  // Hydraulic ram: body, polished rod and a clevis at each end, running hip to knee — the ram
  // takes the landing load, so it triangulates the joint rather than following the arm.
  box(group, frame, [0.36, 0.32, 0.44], [-0.6, -0.2, 0]);
  strut(group, metal, [-0.55, -0.28, 0], [-0.1, -1.1, 0], 0.18, 10);
  strut(group, ceramic, [-0.1, -1.1, 0], [knee[0] - 0.28, knee[1] + 0.2, 0], 0.09, 8);
  box(group, frame, [0.32, 0.32, 0.36], [knee[0] - 0.28, knee[1] + 0.2, 0]);

  // Drag brace: hip to upper arm, so the knee cannot fold past its stop.
  strut(group, frame, [-0.25, -0.5, 0.53], [knee[0] - 0.15, knee[1] + 0.35, 0.53], 0.07, 6);
  box(group, frame, [0.32, 0.28, 0.18], [-0.25, -0.5, 0.62]);
  box(group, frame, [0.32, 0.28, 0.18], [knee[0] - 0.15, knee[1] + 0.35, 0.62]);

  // Contact dust: the plume the sim lights on touchdown, hanging under the pad.
  effect_cone(group, effects, {
    name: 'dust', radius: 1.0, length: 1.2, pos: [foot[0], foot[1] - 0.3, 0], flip: false,
  });
  return group;
}

/** A worn gear's worth of scuffs, with the jacking and stand-clear stencils a ground crew
 *  works to. */
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
        stencil_text(tile, 'JACK', { x: 0.58, y: 0.34, scale: 0.026, mirror: true });
        stencil_text(tile, 'NO STEP', { x: 0.16, y: 0.6, scale: 0.02 });
        hazard_stripes(tile, { x: 0.34, y: 0.08, w: 0.6, h: 0.1 }, { pitch: 0.04, angle: -Math.PI / 4 });
        damage_relief(tile, { amount: 0.12, seed: 43, albedo: true });
      });
    },
  };
})();

export const meta = { name: 'landing_leg', scale: 1, collider: 'auto' };
