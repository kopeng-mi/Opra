// Torpedo tube (plan-04 §2.4, ref 18): a 5.0 m launch tube with a flush muzzle block, side
// loading tray and breech flange. The tube axis is +Y: the fish leaves through `hp.muzzle`
// and the loader feeds `hp.breech`, which carries the shared stack flange.
import * as THREE from 'three';
import {
  copper, dark, lightArmor, metal,
  box, cylinder, effect_cone, each_tile, flange, hp, standard_maps, stencil_text, strut,
  tube, type Maps,
} from './prims';

/** Tube length overall, muzzle face to breech flange. */
const LENGTH = 5.0;
const RADIUS = 0.6;

export function build(): THREE.Object3D {
  const group = new THREE.Group(); group.name = 'torpedo_tube';
  const effects: THREE.Mesh[] = [];
  const half = LENGTH / 2;

  // Pressure tube: breech flange behind, muzzle block flush ahead. The shell is one straight
  // run — a torpedo tube is a gun barrel, not a pressure vessel, so no domes.
  tube(group, metal, RADIUS, RADIUS, LENGTH, [0, 0, 0], 20);
  // Stiffener bands and the longitudinal rails the inner tray hangs off.
  for (const y of [-1.7, -0.4, 0.9]) tube(group, dark, RADIUS + 0.08, RADIUS + 0.08, 0.22, [0, y, 0], 20);
  for (const side of [-1, 1]) box(group, dark, [0.16, LENGTH - 0.6, 0.16], [side * (RADIUS + 0.04), 0, RADIUS - 0.1]);
  // Muzzle block: a flush face with a dark aperture the fish leaves through and a hazard rim.
  // Flush, per the sheet — nothing protrudes past the face.
  cylinder(group, dark, RADIUS + 0.02, RADIUS + 0.02, 0.18, [0, half - 0.09, 0], 20);
  cylinder(group, lightArmor, 0.42, 0.42, 0.2, [0, half - 0.08, 0], 16);
  tube(group, dark, 0.34, 0.34, 0.24, [0, half - 0.06, 0], 16);
  // Loading tray: full-length rails on the dorsal side with three hanger arms into the tube,
  // and the copper impulse lines feeding the breech.
  for (const x of [-0.35, 0.35]) box(group, metal, [0.14, LENGTH - 0.8, 0.14], [x, 0, RADIUS + 0.22]);
  for (const y of [-1.6, 0, 1.6]) {
    box(group, metal, [0.9, 0.14, 0.3], [0, y, RADIUS + 0.2]);
    strut(group, metal, [0, y, RADIUS + 0.2], [0, y, RADIUS], 0.07, 5);
  }
  strut(group, copper, [0.4, -half + 0.4, -0.4], [0.4, -half + 1.6, -0.5], 0.07, 6);
  strut(group, copper, [-0.4, -half + 0.4, -0.4], [-0.4, -half + 1.6, -0.5], 0.07, 6);
  box(group, metal, [0.5, 0.4, 0.4], [0, -half + 0.35, -0.55]);
  // Breech flange: the shared stack interface, facing aft.
  flange(group, metal, [0, -half, 0], [0, -1, 0]);

  hp(group, 'breech', [0, -half, 0]);
  hp(group, 'muzzle', [0, half, 0]);
  effect_cone(group, effects, {
    name: 'flash', radius: 0.55, length: 2.2, pos: [0, half + 0.1, 0], flip: false,
  });
  return group;
}

/** Tube panels lengthwise, with the tube number and a muzzle warning. */
export const maps: Maps = (() => {
  const kit = standard_maps({
    seed: 83, panel: 0.16, rivets: 0.07, seams: 2, wear: 0.3, grime: 0.24, scorch: 1,
    stencils: [['T-1', 0.1, 0.16]],
  });
  return {
    normal: kit.normal,
    roughness: kit.roughness,
    albedo: (ctx, size) => {
      kit.albedo?.(ctx, size);
      each_tile(ctx, size, (tile) => {
        stencil_text(tile, 'STAND CLEAR', { x: 0.2, y: 0.5, scale: 0.02 });
      });
    },
  };
})();

export const meta = { name: 'torpedo_tube', scale: 1, collider: 'auto' };
