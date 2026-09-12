// Cryogenic propellant tank, small (PLAN-03 §7.1): a lathed pressure vessel with domed ends in a
// saddle mount, wrapped in cryo lagging, ringed with stiffeners and served by a transfer line whose
// valve block sits at the foot. The tank axis is +Y and the origin is the mount's foot plane, so a
// deck or a station cradle places the whole assembly by putting y = 0 on its deck.
//
// The fill gauge is not geometry: it is drawn into the albedo atlas. The atlas is a planar
// projection from model space, so where the decal lands is a fact about this module's envelope, and
// the arithmetic is written beside the band below rather than guessed at.
import * as THREE from 'three';
import {
  copper, dark, frame, hazardPaint, insulation, metal,
  box, each_tile, hazard_stripes, lathe, standard_maps, stencil_text, strut, tube, type Maps,
} from './prims';

// Vessel: barrel radius 2.2 m, barrel between y 2.1 and 8.5, poles at 0.8 and 9.8. The mount, the
// blanket and the plumbing are all measured from these, so they live here where a change moves
// everything together instead of leaving one part behind.
const R = 2.2;
const POLE = 0.8;
const TOP = 9.8;
const BARREL = [2.1, 8.5];

export function build(): THREE.Object3D {
  const group = new THREE.Group(); group.name = 'tank_small';

  // Mount: a foot plate, four legs, and a clamp band that bites the bottom dome where it is 3.5 m
  // across. A band at barrel radius would float beside the dome, which is what makes a tank look
  // dropped on a stand instead of bolted into one; the legs run from the plate to that band, splayed
  // so their tops land on it rather than beside it.
  tube(group, dark, 2.7, 2.7, 0.45, [0, 0.22, 0], 10);
  tube(group, frame, 2.0, 2.0, 0.6, [0, 1.4, 0], 10);
  for (let i = 0; i < 4; i++) {
    const angle = Math.PI / 4 + (i * Math.PI) / 2;
    strut(group, frame, [Math.cos(angle) * 2.4, 0.45, Math.sin(angle) * 2.4], [Math.cos(angle) * 1.9, 1.35, Math.sin(angle) * 1.9], 0.24, 6);
  }

  // Vessel: one lathe from pole to pole, so the domes are a real surface of revolution rather than a
  // barrel with two flat caps. The profile ends at radius zero on both poles, which closes it.
  lathe(group, metal, [
    [0, POLE], [0.95, POLE + 0.15], [1.75, POLE + 0.6], [R, BARREL[0]],
    [R, BARREL[1]], [1.75, TOP - 0.6], [0.95, TOP - 0.15], [0, TOP],
  ], 14);

  // Cryo lagging: a blanket over the barrel that chamfers down to the shell radius at both ends, so
  // its rim meets the vessel and leaves no open annulus for the camera to find.
  lathe(group, insulation, [
    [R, BARREL[0] + 0.4], [R + 0.14, BARREL[0] + 0.7], [R + 0.14, BARREL[1] - 0.7], [R, BARREL[1] - 0.4],
  ], 14);

  // Stiffener rings proud of the blanket: a thin shell only stays round because something holds it.
  for (const y of [3.6, 5.3, 7.0]) tube(group, metal, R + 0.25, R + 0.25, 0.3, [0, y, 0], 10);

  // Transfer line: off the top dome, down past the blanket, into a valve block at the foot, then
  // the coupling. The run sits clear of the stiffener rings so it reads as bracketed to them.
  const run = R + 0.35;
  strut(group, copper, [1.3, TOP - 0.5, 0], [run, BARREL[1] - 0.6, 0], 0.18, 6);
  strut(group, copper, [run, BARREL[1] - 0.6, 0], [run, 1.55, 0], 0.18, 6);
  box(group, metal, [0.75, 0.85, 0.75], [run, 1.9, 0]);
  strut(group, copper, [run, 1.55, 0], [run, 0.7, 0], 0.16, 6);
  tube(group, copper, 0.3, 0.3, 0.45, [run, 0.45, 0], 8);
  // Handwheel on its own stem, axis along X: it has to clear the dome, so it points away from the
  // tank rather than lying on it.
  strut(group, metal, [run + 0.3, 1.9, 0], [run + 0.75, 1.9, 0], 0.12, 6);
  tube(group, hazardPaint, 0.4, 0.4, 0.12, [run + 0.78, 1.9, 0], 8, [0, 0, Math.PI / 2]);

  // Crown: a manway and a relief stub, the two fittings that make a pressure vessel read as one.
  // The stub is copper and rises clear of the dome - a black stub half-buried in the shell reads as
  // a hole rather than a fitting.
  tube(group, metal, 0.8, 0.8, 0.45, [0, TOP + 0.2, 0], 8);
  tube(group, copper, 0.26, 0.3, 0.75, [0.95, 9.85, 0], 8);
  return group;
}

export const maps: Maps = (() => {
  const kit = standard_maps({ seed: 63, panel: 0.17, rivets: 0.09, seams: 2, wear: 0.28, grime: 0.22 });
  // The atlas spans max(aabb) = 10.23 m, and a tile coordinate is (model - box.min)/span. The face
  // the camera sees is cell 0 (±Z-facing), whose across axis is model x and along axis is model y:
  // model x = 0 is the +Z generator and lands at (0 - -2.57)/10.23 = 0.251, the blanket's y 2.1..8.5
  // lands at (10.23 - y)/10.23 = 0.17..0.79. Hence the band below: model x -0.11..0.38 m and
  // y 2.0..8.4 m. Move the envelope and these numbers move with it (span and aabb are in the
  // sidecar).
  const band = { x: 0.24, w: 0.048, top: 0.18, height: 0.62 };
  return {
    normal: kit.normal,
    roughness: kit.roughness,
    albedo: (ctx, size) => {
      kit.albedo?.(ctx, size);
      each_tile(ctx, size, (tile) => {
        if (tile.cell !== 0) return;                 // one gauge, on the face that is looked at
        const x = band.x * tile.w, w = band.w * tile.w;
        const top = band.top * tile.h, h = band.height * tile.h;
        const ink = Math.max(1, tile.w * 0.0035);
        // Graduated band: painted channel, filled level, then twelve ticks between the poles.
        ctx.fillStyle = '#f6f3e9'; ctx.fillRect(x, top, w, h);
        ctx.fillStyle = '#8fb9c6'; ctx.fillRect(x, top + h * 0.31, w, h * 0.69);   // 69% full
        ctx.fillStyle = '#3a4046';
        for (let i = 0; i <= 12; i++) {
          const at = top + (i / 12) * h;
          ctx.fillRect(x, at - ink / 2, w * (i % 4 === 0 ? 1.0 : 0.55), ink);
        }
        ctx.strokeStyle = '#3a4046'; ctx.lineWidth = ink * 1.2;
        ctx.strokeRect(x, top, w, h);
        stencil_text(tile, 'LOX', { x: x - w * 0.02, y: top - 0.052 * tile.h, scale: 0.013 });
        stencil_text(tile, 'FILL', { x: x + w * 1.2, y: top + h * 0.66, scale: 0.011, color: '#3a4046' });
        // Warning block beside the gauge, on the blanket: the tank's own paint is cream, so a hazard
        // band drawn over the dark skid below would come out as mud.
        hazard_stripes(tile, { x: 0.34, y: 0.60, w: 0.05, h: 0.03 }, { pitch: 0.022, colors: ['#b8862f', '#23262b'], angle: Math.PI / 2 });
      });
    },
  };
})();

export const meta = { name: 'tank_small', scale: 1, collider: 'auto' };
