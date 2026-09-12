// Sensor mast (PLAN-03 §7.1): a bolted mast with a two-axis gimbal and a parabolic scanner. Fleet
// axes: nose +Y, dorsal +Z, starboard +X, metres. The mast rises along +Z because this is hull
// furniture, the gimbal yaws about that same +Z and pitches about +X, and the beam leaves the dish
// along the dish group's own +Y.
//
// `hp.scan` sits at the dish vertex on that boresight, so the anchor the sidecar carries is exactly
// where the beam starts and points where the dish looks. The dish is tilted 17 degrees forward of
// the vertical: a scanner that only ever looks straight up has no reason to carry a gimbal.
import * as THREE from 'three';
import {
  bevelled, box, copper, dark, dish, each_tile, hazard_stripes, hp, insulation, lathe, lightArmor,
  metal, scorch, standard_maps, stencil_text, strut, tube, type Maps,
} from './prims';

/** The feed aperture: a clean radome disc, so it is named and kept out of the maps. */
const feedWindow = new THREE.MeshStandardMaterial({ color: '#9fc0c8', roughness: 0.18, metalness: 0.3, emissive: '#2c5f6d', emissiveIntensity: 0.45 });
feedWindow.name = 'feedWindow';

const DISH_RADIUS = 2.7;
const DISH_DEPTH = 0.9;
/** Elevation axis height up the mast, and how far the bowl's vertex sits above it. */
const TRUNNION = 10.8;
const VERTEX_LIFT = 0.32;

export function build(): THREE.Object3D {
  const group = new THREE.Group(); group.name = 'sensor_mast';
  // The mast is authored with +Y up it and then stood on end. `lathe` and `tube` revolve about
  // their own +Y, so one rotation up front beats rotating every part: inside this frame +Y is
  // dorsal, +X is still starboard, and the nose is -Z (which is why the dish tilts through a
  // negative angle below).
  const mast = new THREE.Group();
  mast.rotation.x = Math.PI / 2;
  group.add(mast);
  // Foot: a bevelled plate with four bolt heads, so the mast reads as bolted to a hull rather than
  // dropped on it, and two gussets taking the bending load out of the mast root.
  bevelled(mast, metal, [4.6, 0.7, 4.6], [0, 0.35, 0], [0, 0, 0], { radius: 0.3, segments: 1 });
  for (const sx of [-1, 1]) for (const sy of [-1, 1]) box(mast, lightArmor, [0.5, 0.3, 0.5], [sx * 1.8, 0.82, sy * 1.8]);
  lathe(mast, lightArmor, [[1.45, 0.55], [1.1, 1.2], [1.0, 6.5], [0.85, 8.5]], 10);
  for (const sx of [-1, 1]) strut(mast, metal, [sx * 1.5, 0.7, 0], [sx * 0.95, 2.6, 0], 0.18, 4);
  // Cable run: junction box at the root, conduit clipped down the mast into the azimuth drum.
  box(mast, dark, [1.2, 1.0, 0.9], [1.25, 1.6, 0]);
  strut(mast, dark, [1.25, 2.1, 0], [1.0, 6.4, 0], 0.16, 4);
  strut(mast, dark, [1.0, 6.4, 0], [1.1, 8.6, 0], 0.14, 4);
  // Azimuth: the drum the head yaws on, its bearing lip proud of the mast head.
  lathe(mast, dark, [[1.05, 8.4], [1.5, 8.75], [1.45, 9.5]], 10);
  // Elevation: two yoke arms up to the trunnion the dish pitches on, about +X.
  for (const sx of [-1, 1]) strut(mast, metal, [sx * 0.85, 9.4, 0], [sx * 1.25, TRUNNION, 0], 0.18, 4);
  tube(mast, metal, 0.26, 0.26, 2.9, [0, TRUNNION, 0], 8, [0, 0, Math.PI / 2]);
  // Dish on the trunnion: vertex just clear of the shaft, boresight tilted forward of vertical.
  const dishGroup = new THREE.Group();
  dishGroup.position.set(0, TRUNNION + VERTEX_LIFT, 0);
  dishGroup.rotation.x = -0.3;
  mast.add(dishGroup);
  dish(dishGroup, insulation, DISH_RADIUS, DISH_DEPTH, [0, 0, 0], [0, 0, 0], 10);
  lathe(dishGroup, metal, [[DISH_RADIUS - 0.15, DISH_DEPTH], [DISH_RADIUS + 0.02, DISH_DEPTH - 0.15], [DISH_RADIUS - 0.15, DISH_DEPTH - 0.3]], 10);
  // Feed horn on the axis, held by three struts off the rim, capped with the radome disc.
  const focus = DISH_DEPTH + DISH_RADIUS * 0.55;
  for (let i = 0; i < 3; i++) {
    const angle = (i / 3) * Math.PI * 2 + 0.4;
    strut(dishGroup, metal, [Math.cos(angle) * DISH_RADIUS * 0.94, DISH_DEPTH, Math.sin(angle) * DISH_RADIUS * 0.94], [0, focus, 0], 0.07, 3);
  }
  lathe(dishGroup, copper, [[0.2, focus - 0.9], [0.2, focus]], 8);
  lathe(dishGroup, feedWindow, [[0.4, focus], [0.34, focus + 0.28], [0.18, focus + 0.42]], 8);
  hp(dishGroup, 'scan', [0, 0, 0], [0, 0, 0]);
  return group;
}

/** Fine panels for a small assembly, with the mast's duty stencil and a hazard band at the foot. */
export const maps: Maps = (() => {
  const kit = standard_maps({
    seed: 55, panel: 0.22, rivets: 0.09, seams: 1, wear: 0.3, grime: 0.24, scorch: 1,
    stencils: [['SCAN', 0.1, 0.16]],
  });
  return {
    normal: kit.normal,
    roughness: kit.roughness,
    albedo: (ctx, size) => {
      kit.albedo?.(ctx, size);
      each_tile(ctx, size, (tile) => {
        hazard_stripes(tile, { x: 0.06, y: 0.76, w: 0.88, h: 0.12 }, { pitch: 0.04, angle: 0 });
        stencil_text(tile, 'M-12', { x: 0.6, y: 0.44, scale: 0.03 });
      });
    },
  };
})();

export const meta = { name: 'sensor_mast', scale: 1, collider: 'auto', untextured: ['feedWindow'] };
