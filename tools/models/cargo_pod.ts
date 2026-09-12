// Intermodal cargo pod (PLAN-03 §7.1): a bevelled pressure shell that latches onto a hab_module's
// mounts, with the corner rails a container is lifted by, a dogged door, and its number and hazard
// band painted into the albedo.
//
// The mate is the whole point of the asset, so it is stated here rather than left implicit: the
// hab_module's +Y bulkhead carries four lugs at x = ±1.6, z = ±1.9, each 0.7 x 0.5 x 0.7 with its
// top 0.35 m proud of that face and a 0.18 m shear pin through it along Z. This pod's mounting face
// is -Y at y = 0 and it carries the matching four latches on the same rectangle: a fork that drops
// over each lug (two jaws 0.70 m apart in Z, a cap that lands on the lug's top), pinned through jaw,
// lug and jaw on the same Z axis. The fork hangs below the face, so placing the pod on a hab means
// putting this origin 0.64 m above that bulkhead - and nothing else has to be measured by hand.
import * as THREE from 'three';
import {
  black, dark, frame, hazardPaint, hullPaint, lightArmor, metal,
  bevelled, box, each_tile, hp, standard_maps, stencil_text, strut, tube, type Maps, type Tile,
} from './prims';

const HALF = 2.8;          // shell half-width, so the frame rails sit at 2.55 m
const STATIONS = [[1.6, 1.9], [1.6, -1.9], [-1.6, 1.9], [-1.6, -1.9]];

/** One latch: jaws that straddle a hab lug, a cap that lands on it, and the pin that ties them. */
function latch(root: THREE.Object3D, x: number, z: number): void {
  box(root, frame, [0.94, 0.5, 0.13], [x, -0.25, z - 0.41]);
  box(root, frame, [0.94, 0.5, 0.13], [x, -0.25, z + 0.41]);
  box(root, frame, [0.94, 0.14, 0.96], [x, -0.57, z]);
  tube(root, metal, 0.09, 0.09, 1.0, [x, -0.25, z], 6, [Math.PI / 2, 0, 0]);
}

export function build(): THREE.Object3D {
  const group = new THREE.Group(); group.name = 'cargo_pod';

  // Shell: one bevelled box, because a pressure vessel with corners is exactly what an intermodal
  // pod is - the bevel is the shell's corner radius, and the frame under it is what takes the load.
  // One bevel segment, not two: at this size a chamfer reads as a rolled corner, and the triangles
  // are better spent on the frame and the latches.
  bevelled(group, hullPaint, [HALF * 2, 6.0, HALF * 2], [0, 3.2, 0], [0, 0, 0], { radius: 0.7, segments: 1 });
  box(group, frame, [HALF * 2 + 0.3, 0.4, HALF * 2 + 0.3], [0, 0.2, 0]);
  box(group, frame, [HALF * 2 + 0.3, 0.4, HALF * 2 + 0.3], [0, 6.2, 0]);

  // Corner posts and top rails: the ISO frame, the part that gets lifted, stacked and lashed. The
  // bottom of the frame is the load plate at y = 0.2, which is also the mating face's own ring.
  for (const sx of [-2.55, 2.55]) for (const sz of [-2.55, 2.55]) box(group, frame, [0.5, 5.6, 0.5], [sx, 3.2, sz]);
  for (const sz of [-2.55, 2.55]) box(group, frame, [5.2, 0.35, 0.4], [0, 5.95, sz]);
  for (const sx of [-2.55, 2.55]) box(group, frame, [0.4, 0.35, 5.2], [sx, 5.95, 0]);

  // Mounting face: four latches on the rectangle above, hanging under y = 0.
  for (const [x, z] of STATIONS) latch(group, x, z);

  // Door: recessed surround, the leaf, two hinges, two dogged lock rods and the lever that turns
  // them. The leaf is hull paint, not `dark`: the albedo's number and hazard band are multiplied by
  // the material colour, and a dark door would swallow both.
  box(group, dark, [4.6, 5.0, 0.3], [0, 3.2, 2.75]);
  bevelled(group, lightArmor, [4.2, 4.6, 0.36], [0, 3.2, 2.98], [0, 0, 0], { radius: 0.18, segments: 1 });
  for (const y of [1.75, 4.65]) tube(group, metal, 0.22, 0.22, 0.55, [2.25, y, 3.15], 6);
  for (const x of [-1.6, 0.4]) strut(group, metal, [x, 1.1, 3.18], [x, 5.3, 3.18], 0.15, 6);
  strut(group, metal, [-1.6, 3.2, 3.2], [-2.7, 3.2, 3.95], 0.16, 6);

  // Shell fittings: the vent and its relief head on the roof, the hazard plate and the data plate.
  tube(group, metal, 0.32, 0.32, 0.7, [1.4, 6.45, 1.6], 8);
  box(group, metal, [0.9, 0.5, 0.9], [1.4, 6.7, 1.6]);
  box(group, hazardPaint, [1.5, 1.0, 0.22], [1.95, 1.35, 2.94]);
  box(group, black, [1.0, 0.7, 0.14], [-1.9, 5.3, 2.76]);

  // Where a crane hook goes, and nothing else: a pod is lifted by its frame, so there is no lug to
  // draw, but a future editor wants the point.
  hp(group, 'lift', [0, 6.4, 0]);
  return group;
}

/**
 * A hazard band confined to its rect. `hazard_stripes` sizes its bars by the rect's diagonal - for a
 * wide, short band that floods the whole tile - and a pod's door band has to stay on the door, so
 * the bars are laid out here instead.
 */
function danger_band(tile: Tile, rect: { x: number; y: number; w: number; h: number }): void {
  const { ctx } = tile;
  const bars = Math.max(4, Math.round(rect.w * 20));
  const pitch = (rect.w * tile.w) / bars;
  for (let i = 0; i < bars; i++) {
    ctx.fillStyle = i % 2 ? '#20232a' : '#c8a33c';
    ctx.fillRect(rect.x * tile.w + i * pitch, rect.y * tile.h, pitch * 0.55, rect.h * tile.h);
  }
}

export const maps: Maps = (() => {
  const kit = standard_maps({ seed: 51, panel: 0.18, rivets: 0.1, seams: 1, wear: 0.32, grime: 0.24 });
  // Cell 0 is the ±Z-facing tile, which on this pod is the door face. The atlas spans max(aabb) =
  // 7.59 m and a tile coordinate is (model - box.min)/span, so model x = 0 is tile x 0.389, the
  // door's upper panel (model y 4.6) is tile y 0.310 and the door sill (model y 1.2) is tile y
  // 0.758. The number, the class stencil and the door's own hazard band sit inside those bounds.
  return {
    normal: kit.normal,
    roughness: kit.roughness,
    albedo: (ctx, size) => {
      kit.albedo?.(ctx, size);
      each_tile(ctx, size, (tile) => {
        danger_band(tile, { x: 0.06, y: 0.10, w: 0.88, h: 0.035 });      // the lashing band
        if (tile.cell !== 0) return;
        stencil_text(tile, 'CP-211', { x: 0.13 * tile.w, y: 0.245 * tile.h, scale: 0.018 });
        stencil_text(tile, 'CARGO', { x: 0.135 * tile.w, y: 0.310 * tile.h, scale: 0.011, color: '#4a5057' });
        danger_band(tile, { x: 0.112, y: 0.730, w: 0.553, h: 0.048 });   // the door sill band
      });
    },
  };
})();

export const meta = { name: 'cargo_pod', scale: 1, collider: 'auto' };
