// Fusion drive, main engine (plan-04 §2.4, ref 11): 5.5 m tall, 2.6 m bell — the observed
// object-to-cube ratio, which wins over the prompt (A1). Regeneratively cooled bell with the
// straw-to-blue heat gradient in the maps, gimbal ring, turbopump plumbing, and the shared
// stack flange on top.
//
// Authored standalone with the bell mouth at y = 0 and the flange toward +Y, so a ship only
// has to place the flange: the flame already hangs off the mouth. Axes: nose +Y, dorsal +Z,
// starboard +X, metres.
import * as THREE from 'three';
import {
  black, copper, dark, frame, lightArmor, metal,
  box, cylinder, each_tile, effect_cone, flange, hp, lathe, scorch, standard_maps,
  stencil_text, strut, torus, tube, type Maps,
} from './prims';

/** Injector face: unlit, so the throat still reads as lit when the bell is in its own shadow. */
const ignition = new THREE.MeshBasicMaterial({ color: '#a3e9ff', toneMapped: false });
ignition.name = 'ignition';

/** Bell exit radius (2.6 m bell) and stack height (5.5 m). Everything is measured from these. */
const EXIT = 1.3;
const HEIGHT = 5.5;

export function build(): THREE.Object3D {
  const group = new THREE.Group(); group.name = 'drive_main';
  const flames: THREE.Mesh[] = [];

  // Bell: one surface of revolution from the mouth up, throat at ~0.55 of exit. The profile
  // carries the flare, not a linear cone — the changing radius of curvature is what makes a
  // bell read as a bell instead of a funnel.
  lathe(group, metal, [
    [EXIT, 0], [1.24, 0.25], [1.1, 0.7], [0.94, 1.25], [0.8, 1.8], [0.72, 2.3], [0.72, 2.6],
  ], 20);
  // Interior lining, authored top-down so the normals face inward: looking into the mouth shows
  // a lit throat instead of the flare's culled backfaces.
  lathe(group, dark, [
    [0.68, 3.3], [0.68, 2.6], [0.72, 2.2], [0.86, 1.4], [1.06, 0.6], [1.2, 0.12],
  ], 14);
  // Rim band and film-cooling manifold: the lip that catches light, and the hard line that
  // breaks the flare's silhouette.
  tube(group, dark, EXIT + 0.05, EXIT + 0.05, 0.14, [0, 0.07, 0], 20);
  tube(group, metal, 1.08, 1.08, 0.12, [0, 0.72, 0], 20);
  // Ribbed panels: twelve strakes down the flare with two stiffener rings over them, the way
  // the sheet draws the bell — structure you can see from across the map.
  for (let i = 0; i < 12; i++) {
    const angle = (i / 12) * Math.PI * 2;
    strut(group, metal, [Math.cos(angle) * 0.78, 2.2, Math.sin(angle) * 0.78],
      [Math.cos(angle) * 1.2, 0.3, Math.sin(angle) * 1.2], 0.035, 4);
  }
  for (const [y, r] of [[0.55, 1.15], [1.5, 0.88]]) tube(group, frame, r, r, 0.09, [0, y, 0], 20);

  // Powerhead: valve block, twin white L-ducts and the copper line loops standing clear of
  // the flare, with an avionics box on the starboard side.
  tube(group, metal, 0.62, 0.62, 0.7, [0, 3.35, 0], 14);
  for (const side of [-1, 1]) {
    box(group, lightArmor, [0.42, 1.1, 0.42], [side * 0.85, 3.6, 0]);
    box(group, lightArmor, [0.42, 0.42, 0.9], [side * 0.62, 4.2, 0]);
    strut(group, copper, [side * 0.5, 2.9, 0.45], [side * 0.85, 3.7, 0.3], 0.07, 6);
    strut(group, copper, [side * 0.5, 2.9, -0.45], [side * 0.85, 3.7, -0.3], 0.07, 6);
  }
  box(group, dark, [0.5, 0.7, 0.35], [1.0, 3.3, -0.5]);
  // Feed lines down from the cooling manifold to the valve ring: they stop at the gimbal
  torus(group, frame, 0.92, 0.12, [0, 2.55, 0], [Math.PI / 2, 0, 0], 6, 18);
  torus(group, copper, 0.78, 0.045, [0, 2.68, 0], [Math.PI / 2, 0, 0], 5, 18);
  for (let i = 0; i < 3; i++) {
    const angle = (i / 3) * Math.PI * 2 + 0.5;
    strut(group, copper, [Math.cos(angle) * 1.05, 0.85, Math.sin(angle) * 1.05],
      [Math.cos(angle) * 0.66, 2.75, Math.sin(angle) * 0.66], 0.06, 6);
  }
  tube(group, metal, 0.78, 0.78, 0.14, [0, 2.82, 0], 14);
  // Gimbal ring on the throat with the gold band the sheet draws at the joint, trunnions into
  // the chamber wall, three actuators up to the mount: three is the fewest that can hold a
  // thrust axis against roll.
  torus(group, frame, 0.92, 0.12, [0, 2.55, 0], [Math.PI / 2, 0, 0], 6, 18);
  torus(group, copper, 0.78, 0.045, [0, 2.68, 0], [0, 0, 0], 5, 18);
  for (const side of [-1, 1]) strut(group, metal, [side * 0.92, 2.55, 0], [side * 0.72, 2.55, 0], 0.08, 6);
  for (let i = 0; i < 3; i++) {
    const angle = (i / 3) * Math.PI * 2;
    const bx = Math.cos(angle) * 0.92, bz = Math.sin(angle) * 0.92;
    const tx = Math.cos(angle) * 1.0, tz = Math.sin(angle) * 1.0;
    box(group, frame, [0.2, 0.3, 0.2], [bx, 2.75, bz], angle);
    strut(group, metal, [bx, 2.9, bz], [tx, 4.35, tz], 0.09, 6);
    box(group, frame, [0.22, 0.25, 0.22], [tx, 4.45, tz], angle);
  }
  // Mounting flange: the shared stack interface, on struts above the powerhead so the joint
  // reads as a bolted interface rather than a weld.
  for (const side of [-1, 1]) strut(group, metal, [side * 0.7, 4.3, 0], [side * 1.05, HEIGHT, 0], 0.1, 6);
  cylinder(group, metal, 0.85, 0.85, 0.5, [0, 4.6, 0], 14);
  flange(group, metal, [0, HEIGHT, 0], [0, 1, 0]);
  // Injector face and its glow: a disc across the chamber, so the throat ends in a surface
  // rather than in a hole.
  tube(group, black, 0.62, 0.62, 0.14, [0, 3.2, 0], 12);
  tube(group, ignition, 0.5, 0.5, 0.06, [0, 3.12, 0], 12);

  // Anchors: a hull bolts to `mount`, and `thrust` is the bell mouth axis.
  hp(group, 'mount', [0, HEIGHT + 0.1, 0]);
  hp(group, 'thrust', [0, 0, 0]);
  // The plume is an effect: hidden until the sim lights it, and left out of the collider.
  effect_cone(group, flames, { name: 'flame', radius: 1.05, length: 7.0, pos: [0, 0, 0] });
  return group;
}

/** A drive is a casting with plumbing bolted on: large panels, few seams, soot at the mouth
 *  end and the heat tint bands a bell that has been run actually carries. */
export const maps: Maps = (() => {
  const kit = standard_maps({
    seed: 11, panel: 0.3, rivets: 0.1, seams: 2, wear: 0.3, grime: 0.3, scorch: 2,
    stencils: [['PWR-1', 0.12, 0.2]],
  });
  return {
    normal: kit.normal,
    roughness: kit.roughness,
    albedo: (ctx, size) => {
      kit.albedo?.(ctx, size);
      each_tile(ctx, size, (tile) => {
        stencil_text(tile, 'NO STEP', { x: 0.5, y: 0.4, scale: 0.024 });
        scorch(tile, { x: 0.5, y: 0.9, radius: 0.28, seed: 7 });
      });
    },
  };
})();

export const meta = { name: 'drive_main', scale: 1, collider: 'auto', untextured: ['glass', 'ignition'] };
