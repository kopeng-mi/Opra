// Fusion drive, main engine (PLAN-03 §7.1): the bell, the gimbal ring and the actuators that steer
// it, the injector plumbing that feeds it, and the flange a hull bolts it to. Authored standalone
// with the bell mouth at y = 0 and the mounting flange toward +Y, so a ship only has to place the
// flange: the flame already hangs off the mouth, and no per-hull offset is baked in here.
//
// Axes as always: nose +Y, dorsal +Z, starboard +X, metres. Exit diameter 10 m, so the bell reads
// as one component of a ~60 m hull rather than as the whole stern.
import * as THREE from 'three';
import {
  black, copper, dark, frame, metal,
  box, cylinder, each_tile, effect_cone, hazard_stripes, hp, lathe, scorch, standard_maps,
  stencil_text, strut, torus, tube, type Maps,
} from './prims';

/** Injector face: unlit, so the throat still reads as lit when the bell is in its own shadow, and
 *  named because `meta.untextured` can only spare a material the maps can identify. */
const ignition = new THREE.MeshBasicMaterial({ color: '#a3e9ff', toneMapped: false });
ignition.name = 'ignition';

export function build(): THREE.Object3D {
  const group = new THREE.Group(); group.name = 'drive_main';
  const flames: THREE.Mesh[] = [];

  // Bell: a single surface of revolution from the mouth up, exit radius 5.0 m and a throat at 0.55
  // of that. The profile carries the flare, not a linear cone, because the changing radius of
  // curvature is what makes a bell read as a bell instead of a funnel.
  lathe(group, metal, [
    [5.00, 0.00], [4.62, 0.72], [3.98, 1.72], [3.44, 2.78], [3.06, 3.72],
    [2.84, 4.60], [2.66, 5.30], [2.66, 6.80], [3.10, 7.25],
  ], 18);
  // Interior, authored top-down: reversing the profile turns the surface normals inward, so looking
  // into the mouth shows a lit throat instead of the flare's culled backfaces. It runs all the way
  // up to the injector face because the chamber is a surface of revolution seen only from outside:
  // without this lining, a sight line past the throat would look straight through the casing.
  lathe(group, dark, [
    [2.58, 6.90], [2.58, 5.20], [2.62, 4.70], [3.28, 2.85], [4.28, 1.15], [4.80, 0.25],
  ], 12);
  // Rim band: the casting's edge thickness, proud of the flare so the mouth has a lip to catch light.
  tube(group, dark, 5.08, 5.08, 0.55, [0, 0.28, 0], 14);
  // Film-cooling manifold just inside the lip: the spud ring is where the boundary layer is fed, and
  // it gives the flare one hard horizontal line to break its silhouette. Machined, not copper, so
  // the feed lines that leave it stay the only warm-coloured thing on this face.
  tube(group, metal, 4.32, 4.32, 0.45, [0, 1.45, 0], 14);
  // Injector run: three feed lines leave the cooling manifold and climb standing clear of the flare
  // to a valve ring under the gimbal, where the flare falls away and the gap to the pipe becomes a
  // visible silhouette break. They stop at the ring instead of crossing it, because the ring is the
  // joint the bell swings on and nothing gets bolted across it; the chamber feed above is internal.
  for (let i = 0; i < 3; i++) {
    const angle = (i / 3) * Math.PI * 2 + Math.PI / 6;
    const foot = [Math.cos(angle) * 4.36, 1.45, Math.sin(angle) * 4.36];
    box(group, copper, [0.6, 0.5, 0.6], foot);
    strut(group, copper, foot, [Math.cos(angle) * 3.55, 4.25, Math.sin(angle) * 3.55], 0.17, 6);
  }
  tube(group, metal, 3.55, 3.55, 0.42, [0, 4.25, 0], 12);
  for (let i = 0; i < 3; i++) {
    const angle = (i / 3) * Math.PI * 2 + Math.PI / 6;
    cylinder(group, copper, 0.36, 0.36, 0.6, [Math.cos(angle) * 3.55, 4.25, Math.sin(angle) * 3.55], 6);
  }
  // Gimbal ring on the throat, half a metre clear of it, with trunnions into the chamber wall: the
  // bell pivots about this, so everything that steers the ship reacts through the ring.
  torus(group, frame, 3.30, 0.42, [0, 5.05, 0], [Math.PI / 2, 0, 0], 5, 16);
  for (const side of [-1, 1]) strut(group, metal, [side * 3.30, 5.05, 0], [side * 2.78, 5.05, 0], 0.26, 6);
  // Three actuators 120 degrees apart, flange down to ring: three is the fewest that can hold a
  // thrust axis against roll, which is why the real ones come in threes.
  for (let i = 0; i < 3; i++) {
    const angle = (i / 3) * Math.PI * 2 + Math.PI / 3;
    const top = [Math.cos(angle) * 3.78, 7.10, Math.sin(angle) * 3.78];
    const seat = [Math.cos(angle) * 3.36, 5.35, Math.sin(angle) * 3.36];
    strut(group, frame, top, seat, 0.22, 6);
    cylinder(group, metal, 0.42, 0.42, 1.5, [(top[0] + seat[0]) / 2, (top[1] + seat[1]) / 2, (top[2] + seat[2]) / 2], 6);
  }
  // Mounting flange: a plate wide enough to spread the thrust into a hull's frame, a collar above,
  // and six bolts on the pattern the ship-side bracket is drilled for.
  tube(group, frame, 4.15, 4.15, 0.50, [0, 7.55, 0], 14);
  tube(group, metal, 3.30, 3.30, 0.60, [0, 7.90, 0], 14);
  for (let i = 0; i < 6; i++) {
    const angle = (i / 6) * Math.PI * 2 + 0.26;
    cylinder(group, metal, 0.22, 0.22, 0.34, [Math.cos(angle) * 3.75, 7.86, Math.sin(angle) * 3.75], 6);
  }
  // Gussets under the flange: this is where the flange load actually lands on the chamber casing.
  for (let i = 0; i < 4; i++) {
    const angle = (i / 4) * Math.PI * 2 + Math.PI / 4;
    box(group, frame, [0.6, 0.9, 0.6], [Math.cos(angle) * 3.45, 7.05, Math.sin(angle) * 3.45]);
  }
  // Injector face and its glow: a disc across the chamber, so the throat ends in a surface rather
  // than in a hole, with the unlit disc that stands in for the plasma when the drive is lit.
  tube(group, black, 2.62, 2.62, 0.34, [0, 6.85, 0], 10);
  tube(group, ignition, 2.18, 2.18, 0.12, [0, 6.62, 0], 10);

  // Anchors: a hull bolts to `mount`, and `thrust` is the bell mouth axis, where the force and the
  // plume are applied.
  hp(group, 'mount', [0, 8.20, 0]);
  hp(group, 'thrust', [0, 0, 0]);
  // The plume is an effect: hidden until the sim lights it, and left out of the static collider.
  effect_cone(group, flames, { name: 'flame', radius: 4.10, length: 26.0, pos: [0, 0, 0] });
  return group;
}

/** Large panels and few seams: a drive is a casting with plumbing bolted on, not a riveted hull. The
 *  atlas is projected from model space with v = 0 at the model's top, so the hazard band lands on
 *  the mounting flange and the soot at the mouth end, which is where a bell that has been run is
 *  actually dirty. */
export const maps: Maps = (() => {
  const kit = standard_maps({
    seed: 29, panel: 0.2, rivets: 0.1, seams: 2, wear: 0.3, grime: 0.26,
    stencils: [['DRV-09', 0.12, 0.2]],
  });
  return {
    normal: kit.normal,
    roughness: kit.roughness,
    albedo: (ctx, size) => {
      kit.albedo?.(ctx, size);
      each_tile(ctx, size, (tile) => {
        hazard_stripes(tile, { x: 0.06, y: 0.05, w: 0.88, h: 0.12 }, { pitch: 0.05, angle: Math.PI / 6 });
        stencil_text(tile, 'FUSION', { x: 0.58, y: 0.3, scale: 0.024 });
        scorch(tile, { x: 0.5, y: 0.9, radius: 0.2, seed: 43, alpha: 0.7 });
      });
    },
  };
})();

export const meta = { name: 'drive_main', scale: 1, collider: 'auto', untextured: ['glass', 'ignition'] };
