// Fusion drive, vernier casting (PLAN-03 §7.1): the same anatomy as the main engine at 0.45 scale —
// a 4 m bell, a strap ring with two actuators, one plumbing run, a mounting flange — plus the two
// small vernier nozzles that give a hull its last few degrees of attitude authority. The bell is
// steeper and shorter than the main drive's and the ring is a bolted strap rather than a trunnion
// gimbal, so a vernier cluster reads as its own mass-produced casting, not as a scaled copy.
//
// Axes as always: nose +Y, dorsal +Z, starboard +X, metres.
import * as THREE from 'three';
import {
  black, copper, dark, frame, jet_nozzle, metal,
  box, cylinder, each_tile, effect_cone, hazard_stripes, hp, lathe, scorch, standard_maps,
  stencil_text, strut, tube, type Maps,
} from './prims';

/** Injector face: unlit, so the throat still reads as lit when the bell is in its own shadow, and
 *  named because `meta.untextured` can only spare a material the maps can identify. */
const ignition = new THREE.MeshBasicMaterial({ color: '#a3e9ff', toneMapped: false });
ignition.name = 'ignition';

export function build(): THREE.Object3D {
  const group = new THREE.Group(); group.name = 'drive_aux';
  const flames: THREE.Mesh[] = [];
  const jets: THREE.Mesh[] = [];

  // Bell and chamber in one casting: 2.0 m exit radius and a 1.12 m throat, much steeper than the
  // main drive because a vernier trades expansion ratio for a short installed length.
  lathe(group, metal, [
    [2.00, 0.00], [1.82, 0.40], [1.52, 0.95], [1.32, 1.50], [1.18, 2.00],
    [1.12, 2.40], [1.12, 3.00], [1.28, 3.20],
  ], 18);
  // Interior, authored top-down so the normals face the axis and a viewer looking into the mouth
  // sees a lit throat rather than the flare's culled backfaces. It runs up to the injector face
  // because the chamber is only ever seen from outside: unlined, a sight line past the throat would
  // look straight through the casing.
  lathe(group, dark, [[1.02, 2.85], [1.02, 2.35], [1.30, 1.45], [1.68, 0.45], [1.86, 0.18]], 14);
  // Rim band, proud of the flare: the casting's edge thickness, which is most of what an eye reads
  // at the small end of a vernier.
  tube(group, dark, 2.02, 2.02, 0.35, [0, 0.18, 0], 14);
  // Strap ring on the throat with four clamps: a bolted band, not the main drive's trunnion ring, and
  // the two actuators above it pull against the flange.
  tube(group, frame, 1.42, 1.42, 0.34, [0, 2.25, 0], 14);
  for (let i = 0; i < 4; i++) {
    const angle = (i / 4) * Math.PI * 2 + Math.PI / 4;
    box(group, frame, [0.42, 0.4, 0.42], [Math.cos(angle) * 1.42, 2.25, Math.sin(angle) * 1.42]);
  }
  for (const side of [-1, 1]) {
    strut(group, metal, [side * 1.46, 3.02, 0], [side * 1.40, 2.42, 0], 0.16, 6);
    box(group, frame, [0.4, 0.5, 0.4], [side * 1.46, 3.02, 0]);
  }
  // Feed run, mirrored port and starboard: a vernier casting gets one line and one valve per side,
  // where the main drive gets a full injector manifold. The run ends at the valve ring under the
  // strap ring, so nothing is bolted across the joint the clamp has to hold.
  for (const side of [-1, 1]) {
    const foot = [side * 1.23, 0.78, side * 1.23];
    box(group, copper, [0.42, 0.4, 0.42], foot);
    strut(group, copper, foot, [side * 1.06, 1.85, side * 1.06], 0.12, 6);
    cylinder(group, copper, 0.20, 0.20, 0.44, [side * 1.06, 1.62, side * 1.06], 6);
  }
  tube(group, metal, 1.50, 1.50, 0.28, [0, 1.85, 0], 12);
  // Mounting flange: an octagonal plate revolved at eight segments, so the silhouette alone says
  // this is a different casting from the main drive's round flange. Five bolts clear of the feeds.
  lathe(group, frame, [[0.95, 3.05], [1.55, 3.05], [1.55, 3.38], [0.95, 3.38], [0.95, 3.05]], 8);
  tube(group, metal, 1.16, 1.16, 0.36, [0, 3.42, 0], 12);
  for (let i = 0; i < 5; i++) {
    const angle = (i / 5) * Math.PI * 2 + 0.4;
    cylinder(group, metal, 0.14, 0.14, 0.24, [Math.cos(angle) * 1.34, 3.38, Math.sin(angle) * 1.34], 6);
  }
  // Injector face and glow: the throat ends in a surface, and the unlit disc stands in for the
  // plasma while the drive is lit.
  tube(group, black, 1.10, 1.10, 0.28, [0, 2.95, 0], 10);
  tube(group, ignition, 0.92, 0.92, 0.10, [0, 2.78, 0], 8);
  // Vernier nozzles: two small bells canted outboard and down off the chamber wall, which is where
  // a hull wants attitude thrust. `jet_nozzle` flares its bell toward -direction and its plume
  // leaves the same side, so the mounting vector goes in negated: pass the outward vector as-is and
  // the bell opens back into the ship with the plume buried in it. Their plumes are the effects the
  // sim fires for fine control, one per side.
  for (const side of [-1, 1]) {
    strut(group, metal, [side * 1.10, 2.70, 0], [side * 1.62, 2.35, 0], 0.22, 8);
    jet_nozzle(group, jets, {
      name: side > 0 ? 'vernier_stbd' : 'vernier_port',
      radius: 0.42, pos: [side * 1.62, 2.35, 0], direction: [-side * 0.55, 1, 0], length: 0.9,
    });
  }

  // Anchors: a hull bolts to `mount`; `thrust` is the bell mouth axis, where the force and the main
  // plume are applied.
  hp(group, 'mount', [0, 3.60, 0]);
  hp(group, 'thrust', [0, 0, 0]);
  // The main plume is an effect, hidden until the sim lights it and left out of the static collider.
  effect_cone(group, flames, { name: 'flame', radius: 1.75, length: 10.0, pos: [0, 0, 0] });
  return group;
}

/** Finer panels and denser rivets than the main drive: this casing is bolted plate over a small
 *  casting. v = 0 of the atlas is the model's top, so the hazard band lands on the flange and the
 *  soot at the mouth end, where a nozzle that has been run actually burns. */
export const maps: Maps = (() => {
  const kit = standard_maps({
    seed: 53, panel: 0.13, rivets: 0.06, seams: 2, wear: 0.34, grime: 0.28,
    stencils: [['VRN-3', 0.12, 0.2]],
  });
  return {
    normal: kit.normal,
    roughness: kit.roughness,
    albedo: (ctx, size) => {
      kit.albedo?.(ctx, size);
      each_tile(ctx, size, (tile) => {
        hazard_stripes(tile, { x: 0.06, y: 0.06, w: 0.88, h: 0.11 }, { pitch: 0.04, angle: -Math.PI / 6 });
        stencil_text(tile, 'VERNIER', { x: 0.5, y: 0.32, scale: 0.02 });
        scorch(tile, { x: 0.5, y: 0.88, radius: 0.22, seed: 67, alpha: 0.72 });
      });
    },
  };
})();

export const meta = { name: 'drive_aux', scale: 1, collider: 'auto', untextured: ['glass', 'ignition'] };
