// drive_twin_torch: Variant A propulsion module, 4 m axial, drive_fusion.
//
// Two torches side by side in one thrust block, not stacked and not a single bell: a corvette that
// loses one drive still steers. Local +Y is outboard from the flange, so the bells flare toward +Y
// and the module mounts aft-facing - the plume then leaves at ship -Y (PLAN-07 s3.4).
import * as THREE from 'three';
import {
  black, copper, dark, exhaust, glow, lightArmor, metal, ochre,
  box, effect_cone, flange, standard_maps, strut, tube, type Maps,
} from '../../../tools/models/prims';

/** Bell centres, port and starboard. */
const BELL_X = 0.72;

export function build(): THREE.Object3D {
  const root = new THREE.Group();
  root.name = 'drive_twin_torch';
  const effects: THREE.Mesh[] = [];

  flange(root, metal, [0, 0, 0], [0, -1, 0]).name = 'flange';

  // Thrust-frame skirt: the 3.1 m hull opens out to the 3.4 m block here, and the skirt is the
  // load path, so it is hull plate rather than block casting.
  box(root, lightArmor, [3.0, 1.54, 2.1], [0, 0.83, 0]);
  // Thrust block: the heaviest single casting on the ship, taking 2 x 1200 kN into the frame.
  box(root, dark, [3.05, 1.7, 2.0], [0, 2.05, 0]);
  box(root, ochre, [0.95, 0.55, 0.14], [0, 2.3, 1.06]);    // reactor-side access panel

  for (const sx of [-1, 1]) {
    // Bell in two stages, 8-sided: welded from rolled plate, not spun. The throat half is bare
    // structural metal, the exit half carries the ablative thermal coat - DESIGN-DIRECTION-v2's
    // metal-to-dark gradient, built as two cones rather than painted on.
    tube(root, metal, 0.78, 0.6, 1.5, [sx * BELL_X, 2.55, 0], 8);
    tube(root, dark, 0.88, 0.78, 0.86, [sx * BELL_X, 3.67, 0], 8);
    // Exit lip: the rolled ring that stops the plate mouth tearing.
    tube(root, black, 0.96, 0.96, 0.2, [sx * BELL_X, 4.0, 0], 8);
    // Throat glow, seen down the bore from astern and nowhere else. 0.9 m across against a 4 m
    // module: an indicator, not a light source (TASTE-PROFILE s3).
    tube(root, glow, 0.34, 0.34, 0.14, [sx * BELL_X, 4.14, 0], 6);
    // Propellant feed, thrust block to throat, one line each side of the bell.
    strut(root, copper, [sx * 1.32, 0.9, 0.75], [sx * 0.9, 2.1, 0.5], 0.12, 4);
    strut(root, copper, [sx * 1.32, 0.9, -0.75], [sx * 0.9, 2.1, -0.5], 0.12, 4);
    // Block-corner RCS quads: the aft pair of the ship's attitude set.
    box(root, metal, [0.56, 0.56, 0.56], [sx * 1.28, 1.15, 1.0]);
    // Plume. flip:false puts the cone's mouth at the bell and the taper outboard at +Y; the default
    // (flip:true) runs it back into the block, which is the wrong way round for a drive.
    effect_cone(root, effects, {
      name: 'flame', radius: 0.84, length: 6.0, pos: [sx * BELL_X, 4.12, 0],
      material: exhaust, flip: false,
    });
  }

  // Turbopump housing on the block crown, one unit feeding both bells.
  box(root, metal, [1.5, 1.4, 0.5], [0, 2.55, 1.23]);

  return root;
}

export const maps: Maps = standard_maps({
  seed: 705, panel: 0.2, rivets: 0.05, seams: 1, wear: 0.08, grime: 0.06, scorch: 3,
  stencils: [['FUS-TORCH', 0.1, 0.18]],
});

// A's drive vents through the hull; no cooling on the drive part itself.
export const meta = {
  name: 'drive_twin_torch', kind: 'drive_fusion', span: 1, axial: true, modular: true,
  flange: { pos: [0, 0, 0], normal: [0, -1, 0] },
  mass: 11.5, thrust: 1600, propellant: 0, cooling: 0, heat_capacity: 6,
  rcs_jets: 2, rcs_authority: 0.01534,
  scale: 1, collider: 'auto', untextured: ['glow'],
};
