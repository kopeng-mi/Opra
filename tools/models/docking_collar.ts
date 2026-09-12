// Docking collar (plan-04 §2.4, ref 13): a 4.0 m ring with a dark seal face, 4 latching lugs
// at 90°, 4 guide petals on the diagonals and an umbilical block with a window. The collar
// axis is local +Z — a collar caps a hull face — so the mating face looks straight up, and
// `dock.A` sits on that face with its +Y along the axis.
import * as THREE from 'three';
import {
  box, copper, dark, deckPlate, dock, each_tile, effect_cone, glass, hazard_stripes,
  hazardPaint, hullPaint, lathe, metal, scorch, standard_maps, stencil_text, torus, tube,
  type Maps,
} from './prims';

/** Elastomer sealing ring: a clean rubber surface, so it is named and kept out of the maps. */
const seal = new THREE.MeshStandardMaterial({ color: '#23282c', roughness: 0.94, metalness: 0.04 });
seal.name = 'seal';

/** Umbilical window: lit optics, named so the maps leave it alone. */
const umbilicalGlass = new THREE.MeshStandardMaterial({ color: '#9fc0c8', roughness: 0.2, metalness: 0.3, emissive: '#2c5f6d', emissiveIntensity: 0.5 });
umbilicalGlass.name = 'umbilicalGlass';

/** Outer ring radius: 4.0 m across the face. */
const RING = 2.0;

export function build(): THREE.Object3D {
  const group = new THREE.Group(); group.name = 'docking_collar';
  const effects: THREE.Mesh[] = [];
  // Mounting flange, then the structural torus it carries.
  tube(group, metal, RING - 0.15, RING, 0.35, [0, 0, -0.42], 20, [Math.PI / 2, 0, 0]);
  torus(group, metal, RING - 0.32, 0.28, [0, 0, -0.05], [0, 0, 0], 6, 20);
  // The bore: a straight throat a probe slides into, with the dark seal in its mouth.
  tube(group, metal, 1.05, 1.05, 0.9, [0, 0, 0.05], 20, [Math.PI / 2, 0, 0]);
  torus(group, seal, 0.95, 0.12, [0, 0, 0.5], [0, 0, 0], 5, 16);
  // Mating face: the flat annulus both collars meet on, with the guide cone shouldering out
  // of it to lead a mis-aligned collar home.
  lathe(group, deckPlate, [[1.05, 0.55], [1.62, 0.55], [1.62, 0.4]], 20, [0, 0, 0], [Math.PI / 2, 0, 0]);
  lathe(group, hullPaint, [[1.62, 0.55], [RING - 0.05, 0.15]], 20, [0, 0, 0], [Math.PI / 2, 0, 0]);
  // Four latching lugs at 90°: a hinge block welded outboard, a jaw reaching in over the face.
  for (let i = 0; i < 4; i++) {
    const angle = (i / 4) * Math.PI * 2 + Math.PI / 4;
    const cos = Math.cos(angle), sin = Math.sin(angle);
    box(group, metal, [0.34, 0.6, 0.5], [cos * (RING - 0.1), sin * (RING - 0.1), -0.1], angle);
    box(group, dark, [0.62, 0.34, 0.4], [cos * (RING - 0.45), sin * (RING - 0.45), 0.28], angle);
    box(group, copper, [0.2, 0.2, 0.5], [cos * (RING - 0.1), sin * (RING - 0.1), 0.15], angle);
  }
  // Four guide petals on the diagonals: canted plates forming the lead-in funnel a probe sees.
  for (let i = 0; i < 4; i++) {
    const angle = (i / 4) * Math.PI * 2;
    const petal = new THREE.Group();
    petal.position.set(Math.cos(angle) * (RING - 0.25), Math.sin(angle) * (RING - 0.25), 0.75);
    petal.rotation.z = angle;
    petal.rotation.y = -0.5;
    group.add(petal);
    box(petal, hazardPaint, [0.5, 0.7, 0.08], [0, 0, 0]);
  }
  // Umbilical block on the rim with its window: power and data cross here once captured.
  box(group, metal, [0.6, 0.6, 0.7], [0, RING - 0.1, -0.2]);
  box(group, umbilicalGlass, [0.36, 0.3, 0.1], [0, RING - 0.1, 0.2]);
  tube(group, copper, 0.07, 0.07, 0.6, [0.4, RING - 0.35, -0.3], 6);
  // Berth on the face plane, facing the collar's axis.
  dock(group, 'A', [0, 0, 0.55], 0, 'M', Math.PI / 2);
  // Capture pulse: the ring flash the sim lights when the latches drive home.
  effect_cone(group, effects, {
    name: 'capture', radius: 1.5, length: 0.8, pos: [0, 0, 0.6], flip: false,
  });
  return group;
}

/** Broad panels for a large ring, with an alignment stencil and a hazard band on the flange. */
export const maps: Maps = (() => {
  const kit = standard_maps({
    seed: 97, panel: 0.34, rivets: 0.12, seams: 2, wear: 0.26, grime: 0.22, scorch: 1,
    stencils: [['DOCK 3', 0.12, 0.16]],
  });
  return {
    normal: kit.normal,
    roughness: kit.roughness,
    albedo: (ctx, size) => {
      kit.albedo?.(ctx, size);
      each_tile(ctx, size, (tile) => {
        hazard_stripes(tile, { x: 0.08, y: 0.7, w: 0.84, h: 0.16 }, { pitch: 0.05, angle: Math.PI / 4 });
        stencil_text(tile, 'ALIGN', { x: 0.42, y: 0.4, scale: 0.026 });
      });
    },
  };
})();

export const meta = { name: 'docking_collar', scale: 1, collider: 'auto', untextured: ['seal', 'umbilicalGlass'] };
