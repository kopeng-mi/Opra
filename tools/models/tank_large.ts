// Cryogenic propellant tank, large (PLAN-03 §7.1): the same idiom as the small tank at twice the
// diameter, but squat rather than stretched - a wide vessel needs a skid, not a stand, so it rides
// on six radial pads instead of four posts, carries five stiffener rings instead of three, and its
// transfer plumbing is a pair of runs bridged by a cross-over manifold over the crown. Axis +Y,
// origin on the skid's foot plane, so a depot sets it down the same way it sets down the small one.
import * as THREE from 'three';
import {
  black, copper, dark, deckPlate, frame, hazardPaint, insulation, metal,
  box, each_tile, hazard_stripes, lathe, standard_maps, stencil_text, strut, tube, type Maps,
} from './prims';

// Big vessel: barrel radius 4.2 m, barrel between y 3.4 and 9.6, poles at 1.5 and 11.6. The skid,
// the blanket and both runs are measured from these.
const R = 4.2;
const POLE = 1.5;
const TOP = 11.6;
const BARREL = [3.4, 9.6];

/** One skid pad. `box` only yaws about Z, so a radial pad carries its own frame group. The pad is
 *  tall enough to reach the clamp band rather than stopping short of the dome: a tank that appears
 *  to float above its own skid is worse than one with one fewer ring. */
function pad(root: THREE.Object3D, x: number, z: number, yaw: number): void {
  const site = new THREE.Group();
  site.position.set(x, 0, z);
  site.rotation.y = yaw;
  root.add(site);
  box(site, frame, [1.6, 3.0, 3.0], [0, 1.5, 0]);
}

export function build(): THREE.Object3D {
  const group = new THREE.Group(); group.name = 'tank_large';

  // Skid: ring, six pads, and a clamp band around the bottom dome at the height where it is 7.5 m
  // across. The band is what actually holds the vessel; the pads only spread the load onto a deck.
  tube(group, dark, 5.0, 5.0, 0.5, [0, 0.25, 0], 10);
  for (let i = 0; i < 6; i++) {
    const angle = (i / 6) * Math.PI * 2;
    pad(group, Math.cos(angle) * 3.9, Math.sin(angle) * 3.9, -angle);
  }
  tube(group, frame, 3.95, 3.95, 0.7, [0, 2.9, 0], 12);

  // Vessel: pole to pole in one lathe, so both domes are real surfaces of revolution.
  lathe(group, metal, [
    [0, POLE], [1.8, POLE + 0.2], [3.4, POLE + 0.9], [R, BARREL[0]],
    [R, BARREL[1]], [3.4, TOP - 0.9], [1.8, TOP - 0.2], [0, TOP],
  ], 14);

  // Cryo lagging, chamfered onto the shell radius at both rims so no annulus is left open.
  lathe(group, insulation, [
    [R, BARREL[0] + 0.5], [R + 0.3, BARREL[0] + 0.9], [R + 0.3, BARREL[1] - 0.9], [R, BARREL[1] - 0.5],
  ], 12);

  // Five rings: at this diameter the shell is a balloon and the rings are the reason it stays one.
  for (const y of [4.4, 5.75, 7.1, 8.45, 9.0]) tube(group, metal, R + 0.45, R + 0.45, 0.4, [0, y, 0], 8);

  // The two transfer runs sit clear of the rings on brackets, one per side, and are bridged over the
  // crown: a depot tank is fed from either side and must be able to cross-feed its own saddles.
  const run = R + 0.35;
  strut(group, copper, [2.4, 11.45, 0], [run, BARREL[1] - 0.9, 0], 0.26, 6);
  strut(group, copper, [run, BARREL[1] - 0.9, 0], [run, 2.8, 0], 0.26, 6);
  strut(group, copper, [-2.2, 11.5, 0], [-run, BARREL[1] - 1.1, 0], 0.26, 6);
  strut(group, copper, [-run, BARREL[1] - 1.1, 0], [-run, 4.2, 0], 0.26, 6);
  // The bridge rides clear of the crown so the cross-over is legible: buried in the shoulder it
  // reads as two pipes that happen to stop at the same height.
  strut(group, copper, [-2.2, 11.5, 0], [2.4, 11.45, 0], 0.22, 6);
  // Valve block with a handwheel on its outboard face: the coupling it feeds hangs below the block.
  box(group, metal, [1.1, 1.3, 1.1], [run, 3.4, 0]);
  tube(group, hazardPaint, 0.55, 0.55, 0.16, [run + 0.62, 3.4, 0], 8, [0, 0, Math.PI / 2]);

  // Equatorial walkway: a ring the crew stands on to reach the upper rings and the crown fittings.
  tube(group, deckPlate, R + 0.75, R + 0.75, 0.25, [0, 6.4, 0], 10);

  // Crown: manway and the vent stack that keeps the boil-off moving.
  tube(group, metal, 1.3, 1.3, 0.55, [0, TOP + 0.25, 0], 8);
  tube(group, black, 0.5, 0.5, 1.5, [-1.7, TOP - 0.2, 0], 8);
  return group;
}

export const maps: Maps = (() => {
  const kit = standard_maps({ seed: 79, panel: 0.15, rivets: 0.08, seams: 3, wear: 0.3, grime: 0.26 });
  // Same arithmetic as the small tank: cell 0 is the ±Z-facing tile, across is model x and along is
  // model y. Span 12.15 m, so model x = 0 (the +Z generator) lands at (0 - -4.775)/12.15 = 0.393,
  // and the blanket's y 3.9..9.1 lands at (12.15 - y)/12.15 = 0.251..0.679. The band below is model
  // x -0.30..0.30 m, y 3.9..9.1 m; the valve block at model x 4.55, y 3.4 is tile x 0.768, y 0.720.
  const band = { x: 0.368, w: 0.05, top: 0.251, height: 0.428 };
  return {
    normal: kit.normal,
    roughness: kit.roughness,
    albedo: (ctx, size) => {
      kit.albedo?.(ctx, size);
      each_tile(ctx, size, (tile) => {
        if (tile.cell !== 0) return;
        const x = band.x * tile.w, w = band.w * tile.w;
        const top = band.top * tile.h, h = band.height * tile.h;
        const ink = Math.max(1, tile.w * 0.0035);
        // Graduated band: painted channel, filled level, sixteen ticks at 0.35 m apiece.
        ctx.fillStyle = '#f6f3e9'; ctx.fillRect(x, top, w, h);
        ctx.fillStyle = '#8fb9c6'; ctx.fillRect(x, top + h * 0.52, w, h * 0.48);   // 48% full
        ctx.fillStyle = '#3a4046';
        for (let i = 0; i <= 16; i++) {
          const at = top + (i / 16) * h;
          ctx.fillRect(x, at - ink / 2, w * (i % 4 === 0 ? 1.0 : 0.55), ink);
        }
        ctx.strokeStyle = '#3a4046'; ctx.lineWidth = ink * 1.2;
        ctx.strokeRect(x, top, w, h);
        stencil_text(tile, 'CH4', { x: x - w * 0.02, y: top - 0.05 * tile.h, scale: 0.013 });
        stencil_text(tile, 'FILL', { x: x + w * 1.2, y: top + h * 0.44, scale: 0.011, color: '#3a4046' });
        // Warning block beside the gauge, on the blanket: the valve block below it is bare metal, and
        // a hazard band multiplied by bare metal is mud.
        hazard_stripes(tile, { x: 0.30, y: 0.60, w: 0.05, h: 0.03 }, { pitch: 0.022, colors: ['#b8862f', '#23262b'], angle: Math.PI / 2 });
      });
    },
  };
})();

export const meta = { name: 'tank_large', scale: 1, collider: 'auto' };
