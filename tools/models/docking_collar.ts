// Docking collar (PLAN-03 §7.1): the ring a berth is made of, not the ship that comes to it. Fleet
// axes: nose +Y, dorsal +Z, starboard +X, metres. The collar axis is local +Z — a collar caps a
// hull's dorsal face — so the mating face looks straight up, and `dock.A` sits on that face with
// its +Y along the axis: the exporter then records normal3 [0, 0, 1] (the planar normal is rightly
// zero), which puts the berth on the face a pilot flies to rather than at the ring's centre.
//
// Parts, out from the hull: a bolted mounting flange, the structural torus, the bore the probe
// enters, an elastomer sealing ring in the bore mouth, the flat mating annulus, a guide cone that
// leads a mis-aligned collar onto that annulus, and six capture latches clamping over the rim.
import * as THREE from 'three';
import {
  box, dark, deckPlate, dock, each_tile, hazard_stripes, hazardPaint, hullPaint, lathe, metal,
  scorch, standard_maps, stencil_text, torus, tube, type Maps,
} from './prims';

/** Elastomer sealing ring: a clean rubber surface, so it is named and kept out of the maps. */
const seal = new THREE.MeshStandardMaterial({ color: '#23282c', roughness: 0.94, metalness: 0.04 });
seal.name = 'seal';

/** Capture latches around the rim: six jaws, hinged blocks outboard of the ring. */
const LATCHES = 6;

export function build(): THREE.Object3D {
  const group = new THREE.Group(); group.name = 'docking_collar';
  // Mounting flange, then the structural torus it carries.
  tube(group, metal, 3.3, 3.6, 0.6, [0, 0, -0.7], 14, [Math.PI / 2, 0, 0]);
  torus(group, metal, 2.7, 0.5, [0, 0, 0.1], [0, 0, 0], 5, 14);
  // The bore: a straight throat a probe slides into, with the seal in its mouth.
  tube(group, metal, 1.75, 1.75, 1.5, [0, 0, 0.2], 14, [Math.PI / 2, 0, 0]);
  torus(group, seal, 1.55, 0.18, [0, 0, 0.88], [0, 0, 0], 5, 8);
  // Mating face: the flat annulus both collars meet on, with the guide cone shouldering out of it.
  lathe(group, deckPlate, [[1.75, 0.95], [2.35, 0.95], [2.35, 0.78]], 14, [0, 0, 0], [Math.PI / 2, 0, 0]);
  lathe(group, hullPaint, [[2.35, 0.95], [3.05, 0.42]], 14, [0, 0, 0], [Math.PI / 2, 0, 0]);
  // Keying lug on the rim, so a berth can only be taken one way round.
  box(group, hazardPaint, [0.55, 1.3, 0.34], [0, 3.2, 0.5]);
  // Latches: a hinge block welded to the torus' outer wall, a jaw reaching in over the face. The
  // ring starts half a bay off the top so no latch lands on the keying lug.
  for (let i = 0; i < LATCHES; i++) {
    const angle = ((i + 0.5) / LATCHES) * Math.PI * 2 + Math.PI / 2;
    const cos = Math.cos(angle), sin = Math.sin(angle);
    box(group, metal, [0.5, 1.0, 0.9], [cos * 3.1, sin * 3.1, -0.05], angle);
    box(group, dark, [1.1, 0.6, 0.7], [cos * 2.75, sin * 2.75, 0.5], angle);
  }
  // Berth on the face plane, facing the collar's axis.
  dock(group, 'A', [0, 0, 0.95], 0, 'M', Math.PI / 2);
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

export const meta = { name: 'docking_collar', scale: 1, collider: 'auto', untextured: ['seal'] };
