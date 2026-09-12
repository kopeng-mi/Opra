// Cargo crate, variant 0 (PLAN-03 §7.1, re-authored at F8): a real intermodal crate rather than the
// plan-01 stack of boxes. A bevelled shell, eight corner castings standing proud of it, corrugated
// flanks, deck rails with lashing recesses, a two-leaf door with locking bars and hinges, a hazard
// placard. The envelope and the group rotation are the plan-01 crate's and are load bearing: the
// scene drops cargo at a tuned size, so the local box (13.6 x 17.4 x 11.6 m to the castings) and the
// 0.4 rad roll stay where they were.
import * as THREE from 'three';
import {
  black, copper, dark, frame, glass, hazardPaint, hullPaint, metal,
  bevelled, box, each_tile, standard_maps, stencil_text, tube, type Maps, type Tile,
} from './prims';

/**
 * A hazard band drawn inside one rect. `hazard_stripes` lays its stripes across the whole tile —
 * they are not clipped, which is fine on a beacon's 26 m skirt but would cover every face of a 20 m
 * crate — so the placard paints its own band, bounded, with the same yellow-and-black reading.
 */
function placard(tile: Tile, rect: { x: number; y: number; w: number; h: number }, pitch = 0.03): void {
  const bands = [[200, 163, 60], [32, 35, 42]];
  tile.ctx.pixels(rect.x * tile.w, rect.y * tile.h, rect.w * tile.w, rect.h * tile.h, (px, py) =>
    bands[Math.floor((px + py) / (pitch * tile.w)) & 1]);
}

export function build(): THREE.Object3D {
  const group = new THREE.Group(); group.name = 'cargo';
  // Shell: painted steel, crowned by a real bevel so the crate catches light along its edges instead
  // of reading as a flat box. It is deliberately smaller than the castings that sit on its corners.
  bevelled(group, hullPaint, [12.8, 16.4, 10.8], [0, 0, 0], [0, 0, 0], { radius: 0.6, segments: 2 });
  // Corner castings: the eight handling fittings, 0.4 m proud. They are what makes the silhouette
  // read as intermodal at range, and they are the crate's outer envelope.
  for (const x of [-5.6, 5.6]) for (const y of [-7.5, 7.5]) for (const z of [-4.6, 4.6]) {
    box(group, metal, [2.4, 2.4, 2.4], [x, y, z]);
  }
  // Corner posts and corrugation: five vertical ribs a side, which is the relief a container's skin
  // actually has. Without it the flanks are flat panels and the maps are the only detail on them.
  for (const x of [-6.2, 6.2]) for (const z of [-5.2, 5.2]) box(group, frame, [0.5, 16.8, 0.5], [x, 0, z]);
  for (const x of [-6.45, 6.45]) for (const y of [-5.5, -2.75, 0, 2.75, 5.5]) {
    box(group, hullPaint, [0.35, 0.4, 10.4], [x, y, 0]);
  }
  // Deck: two longitudinal rails with a head, and the lashing recesses between them — the tie-down
  // pattern a flatrack carries, so the crate reads as something that was strapped down, not just shut.
  for (const x of [-4.3, 4.3]) {
    box(group, metal, [1.5, 16.6, 0.35], [x, 0, 5.5]);
    box(group, frame, [0.7, 16.6, 0.3], [x, 0, 5.65]);
  }
  for (let i = 0; i < 6; i++) box(group, black, [8.8, 0.5, 0.14], [0, -5.6 + i * 2.24, 5.42]);
  // Door face, aft: a frame, two leaves, six vertical locking bars standing proud of them with their
  // cam keepers, and eight hinges along the outer edges.
  box(group, dark, [12.4, 0.4, 10.4], [0, -8.22, 0]);
  for (const x of [-2.95, 2.95]) box(group, hullPaint, [5.8, 0.5, 9.8], [x, -8.3, 0]);
  for (const x of [-5.3, -3.1, -0.9, 0.9, 3.1, 5.3]) {
    tube(group, copper, 0.22, 0.22, 9.2, [x, -8.47, 0], 6, [Math.PI / 2, 0, 0]);
  }
  for (const x of [-5.8, 5.8]) for (const z of [-3.3, -1.1, 1.1, 3.3]) {
    box(group, metal, [0.8, 0.6, 0.7], [x, -8.3, z]);
  }
  // Door status lens: the one clean surface on the crate, so `glass` stays out of the map binding.
  tube(group, glass, 0.45, 0.45, 0.3, [0, -8.45, 4.2], 8);
  // Placard: a hazard panel on the starboard flank, proud of the corrugation at the height a loader
  // reads it. The band and the class text are repeated in the albedo, so the panel reads from any angle.
  box(group, hazardPaint, [0.4, 5.4, 3.2], [6.85, 0.4, 0.4]);
  group.rotation.set(0.15, 0.1, 0.4);
  return group;
}

/** Panels, rivets and wear, plus the crate's own decals: the placard band and its class stencils. */
export const maps: Maps = (() => {
  const kit = standard_maps({ seed: 17, panel: 0.09, rivets: 0.05, seams: 2, wear: 0.24, grime: 0.26, scorch: 1 });
  return {
    normal: kit.normal,
    roughness: kit.roughness,
    albedo: (ctx, size) => {
      kit.albedo?.(ctx, size);
      each_tile(ctx, size, (tile) => {
        placard(tile, { x: 0.08, y: 0.78, w: 0.44, h: 0.12 });
        stencil_text(tile, 'CARGO', { x: 0.1, y: 0.34, scale: 0.018 });
        stencil_text(tile, 'MAX 30T', { x: 0.1, y: 0.43, scale: 0.018 });
      });
    },
  };
})();

export const meta = { name: 'cargo', scale: 1, collider: 'auto' };
