// PDC turret (plan-04 §2.4, ref 10): twin barrels on an elevating cradle over a proud base
// ring with exposed copper plumbing. ~3.0 m across the base ring.
//
// Axes: nose +Y, dorsal +Z, starboard +X, metres. The mount face is the z = 0 plane — the
// ring sits proud on the hull, not recessed — the turret rises +Z and the barrels leave
// toward +Y elevated ~20°, the way the reference draws them. No hardpoints: the mounts are
// hull-side.
//
// `cylinder` and `lathe` revolve about their own +Y, so the turret is authored y-up inside a
// `mast` frame stood on end once: inside, +Y is dorsal-out-of-hull and the nose is -Z.
import * as THREE from 'three';
import {
  copper, dark, lightArmor, metal, ochre, teal,
  box, cylinder, effect_cone, each_tile, standard_maps, stencil_text, strut, torus, tube, type Maps,
} from './prims';

/** Muzzle radius; the flash cones are sized off this so they stay coupled. */
const MUZZLE = 0.14;

export function build(): THREE.Object3D {
  const group = new THREE.Group(); group.name = 'pdc_turret';
  const effects: THREE.Mesh[] = [];
  const mast = new THREE.Group();
  mast.rotation.x = Math.PI / 2;
  group.add(mast);

  // Base ring: the proud mounting flange, 3.0 m across per the scale cube, with a traverse
  // race standing off it.
  cylinder(mast, metal, 1.5, 1.5, 0.22, [0, 0.11, 0], 24);
  torus(mast, dark, 1.32, 0.09, [0, 0.3, 0], [Math.PI / 2, 0, 0], 6, 24);
  cylinder(mast, dark, 1.28, 1.28, 0.14, [0, 0.36, 0], 24);
  // Lower drum: panelled pedestal with access hatches, a louvered vent and the copper plumbing
  // run exposed around its circumference — the character of the sheet.
  cylinder(mast, lightArmor, 1.18, 1.28, 0.62, [0, 0.68, 0], 20);
  for (let i = 0; i < 4; i++) {
    const angle = (i / 4) * Math.PI * 2 + Math.PI / 4;
    const hatch = new THREE.Group();
    hatch.position.set(Math.cos(angle) * 1.2, 0.62, Math.sin(angle) * 1.2);
    hatch.rotation.y = -angle + Math.PI / 2;
    mast.add(hatch);
    box(hatch, dark, [0.5, 0.4, 0.06], [0, 0, 0]);
  }
  // Louvered vent on the aft face (local +Z is the stern).
  box(mast, dark, [0.6, 0.34, 0.1], [0, 0.62, 1.2]);
  for (let i = 0; i < 3; i++) box(mast, metal, [0.56, 0.05, 0.06], [0, 0.52 + i * 0.1, 1.22]);
  // Copper U-bends around the drum: feed lines looping out of one hatch and into the next.
  for (let i = 0; i < 3; i++) {
    const angle = (i / 3) * Math.PI * 2 + 0.5;
    const x0 = Math.cos(angle) * 1.18, z0 = Math.sin(angle) * 1.18;
    const x1 = Math.cos(angle + 0.5) * 1.18, z1 = Math.sin(angle + 0.5) * 1.18;
    strut(mast, copper, [x0, 0.5, z0], [(x0 + x1) / 2 * 1.2, 0.78, (z0 + z1) / 2 * 1.2], 0.06, 6);
    strut(mast, copper, [(x0 + x1) / 2 * 1.2, 0.78, (z0 + z1) / 2 * 1.2], [x1, 0.5, z1], 0.06, 6);
  }
  // Identity band: teal ring with an ochre stripe, the hull marking carried onto the mount.
  tube(mast, teal, 1.24, 1.24, 0.16, [0, 0.92, 0], 20);
  box(mast, ochre, [0.2, 0.18, 0.2], [0, 0.92, -1.22]);

  // Cradle: two cheek mounts either side of the drum top, carrying the elevation pin. Local
  // -Z is the nose, so the gun house pitches nose-up out of this frame.
  for (const side of [-1, 1]) {
    box(mast, metal, [0.3, 0.75, 0.7], [side * 0.62, 1.25, 0.1]);
    tube(mast, dark, 0.16, 0.16, 0.4, [side * 0.62, 1.35, 0.1], 8, [0, 0, Math.PI / 2]);
  }
  // Gun house, elevated ~20° toward the nose: an armoured box with the ejection port (dark
  // chute) on its aft face and the ammo feed box on the starboard cheek.
  const cradle = new THREE.Group();
  cradle.position.set(0, 1.35, -0.15);
  cradle.rotation.x = -(Math.PI / 2 - 0.35);
  mast.add(cradle);
  box(cradle, lightArmor, [1.1, 0.7, 1.0], [0, 0, 0]);
  box(cradle, dark, [0.45, 0.3, 0.18], [0.2, -0.05, -0.55]);
  tube(cradle, copper, 0.08, 0.08, 0.5, [0.45, -0.1, -0.35], 6, [0, 0, 0]);
  box(cradle, metal, [0.4, 0.55, 0.6], [0.78, -0.05, -0.1]);
  // Twin barrels: jacketed breech, long tube, muzzle brake, leaving toward the nose.
  for (const side of [-1, 1]) {
    tube(cradle, dark, 0.2, 0.24, 0.7, [side * 0.32, 0.75, 0.05], 10);
    tube(cradle, metal, MUZZLE, MUZZLE + 0.03, 1.5, [side * 0.32, 1.75, 0.05], 10);
    tube(cradle, dark, MUZZLE + 0.07, MUZZLE + 0.07, 0.28, [side * 0.32, 2.42, 0.05], 10);
    effect_cone(cradle, effects, {
      name: `flash.${side < 0 ? 0 : 1}`,
      radius: MUZZLE * 2.2, length: 1.1, pos: [side * 0.32, 2.6, 0.05], flip: false,
    });
  }
  return group;
}

/** Close panels for a small casting, with the mount's own stencil. */
export const maps: Maps = (() => {
  const kit = standard_maps({
    seed: 41, panel: 0.2, rivets: 0.08, seams: 2, wear: 0.34, grime: 0.26, scorch: 1,
    stencils: [['PDC-2', 0.1, 0.16]],
  });
  return {
    normal: kit.normal,
    roughness: kit.roughness,
    albedo: (ctx, size) => {
      kit.albedo?.(ctx, size);
      each_tile(ctx, size, (tile) => {
        stencil_text(tile, 'NO STEP', { x: 0.55, y: 0.42, scale: 0.024 });
      });
    },
  };
})();

export const meta = { name: 'pdc_turret', scale: 1, collider: 'auto' };
