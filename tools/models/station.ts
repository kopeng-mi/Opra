// Wayfarer: the ring station (plan-04 §2.6, ref 20). The torus lies in the gameplay plane
// (XY) so the ring reads from above, on 4 diagonal spokes, with a truss spine through the
// axis extending past the ring both ways. Solar farm one side, 2 docking ports per arm end
// (4 total). The previous plan-01 envelope and its fifth berth are gone: the sheet draws four
// collars, two a side, and the image wins (A1).
//
// Axes: nose +Y, dorsal +Z, starboard +X, metres. The ring spins about +Z.
import * as THREE from 'three';
import {
  armor, copper, dark, deckPlate, frame, hullPaint, insulation, lightArmor, metal, ochre,
  solarCell, teal,
  bevelled, box, dock, dome, each_tile, hazard_stripes, lamps, lathe, scorch, sphere,
  standard_maps, stencil_text, strut, torus, truss_box, tube, type Maps,
} from './prims';

const TAU = Math.PI * 2;
/** Pressure torus centreline radius and tube: the ring the sheet draws. */
const RING = 60;
const TUBE = 7;
/** Spine half-length: the truss runs past the ring to the berth tips. */
const SPINE_HALF = 95;

/**
 * One berth: a collar standing off the arm tip, hazard-striped face, the `dock.<id>` anchor
 * on the mating plane and approach lamps down the corridor. Local +Y is the outward normal.
 */
function berth(root: THREE.Object3D, id: string, x: number, y: number, rotation: number, cls: string): void {
  const site = new THREE.Group();
  site.position.set(x, y, 0);
  site.rotation.z = rotation;
  root.add(site);
  tube(site, frame, 4.4, 4.9, 2.4, [0, 3.0, 0], 10);
  torus(site, copper, 4.3, 0.35, [0, 5.6, 0], [Math.PI / 2, 0, 0], 4, 10);
  tube(site, dark, 3.4, 3.4, 1.0, [0, 5.0, 0], 10);
  box(site, deckPlate, [8.6, 0.3, 0.3], [0, 7.6, 0]);
  for (let i = 0; i < 4; i++) {
    const angle = (i / 4) * TAU + Math.PI / 4;
    box(site, metal, [0.4, 0.5, 0.5], [Math.cos(angle) * 4.4, 5.6 + Math.sin(angle) * 4.4, 0], angle);
  }
  dock(site, id, [0, 5.6, 0], 0, cls);
  lamps(site, '#b7dfdd', [[-3, 8, 0], [3, 8, 0]], 0.5, 4);
  lamps(site, '#e6554d', [[-3.6, 7.2, 0], [3.6, 7.2, 0]], 0.45, 4);
}

export function build(): THREE.Object3D {
  const group = new THREE.Group(); group.name = 'station';

  // Pressure torus in the gameplay plane, with rim modules and the identity bands.
  torus(group, hullPaint, RING, TUBE, [0, 0, 0], [0, 0, 0], 8, 36);
  for (let i = 0; i < 8; i++) {
    const angle = (i / 8) * TAU + TAU / 16;
    const x = Math.cos(angle) * RING, y = Math.sin(angle) * RING;
    box(group, lightArmor, [6.0, 5.0, 4.0], [x, y, TUBE + 1.0], angle);
    box(group, dark, [4.4, 0.4, 3.0], [x * 1.0, y * 1.0, -(TUBE + 0.6)], angle);
    box(group, teal, [3.2, 1.6, 0.5], [x, y, TUBE - 0.2], angle + Math.PI / 2);
  }
  {
    const angle = 0.35;
    box(group, ochre, [3.2, 1.6, 0.5], [Math.cos(angle) * RING, Math.sin(angle) * RING, TUBE - 0.2], angle + Math.PI / 2);
  }

  // Hub: drum, cupolas and the four spherical tanks clustered round it.
  tube(group, metal, 9.0, 9.0, 14.0, [0, 0, 0], 12, [Math.PI / 2, 0, 0]);
  dome(group, hullPaint, 9.0, [0, 0, 7.0], [0, 0, 0], 12);
  dome(group, hullPaint, 9.0, [0, 0, -7.0], [Math.PI, 0, 0], 12);
  for (let i = 0; i < 4; i++) {
    const angle = (i / 4) * TAU + TAU / 8;
    sphere(group, insulation, 4.2, [Math.cos(angle) * 13.5, Math.sin(angle) * 13.5, 0], 10, 8);
  }

  // Four spokes on the diagonals: hub to ring, cross-braced trusses.
  for (let i = 0; i < 4; i++) {
    const angle = (i / 4) * TAU + TAU / 8;
    const dx = Math.cos(angle), dy = Math.sin(angle);
    const spoke = new THREE.Group();
    spoke.position.set(dx * (RING / 2 + 6), dy * (RING / 2 + 6), 0);
    spoke.rotation.z = angle - Math.PI / 2;
    group.add(spoke);
    truss_box(spoke, frame, RING - 20, 4.5, 4.5, 3, 0.5);
  }

  // Spine: the truss through the axis, past the ring to the berth tips both ways.
  const spine = new THREE.Group();
  spine.rotation.z = Math.PI / 2;
  group.add(spine);
  truss_box(spine, frame, SPINE_HALF * 2, 7.0, 7.0, 6, 0.7);
  // Spine service modules and tankage between hub and ring.
  for (const side of [-1, 1]) {
    box(group, metal, [22.0, 6.0, 6.0], [side * 38, 0, 0]);
    tube(group, insulation, 2.6, 2.6, 10.0, [side * 52, 0, 5.5], 12, [Math.PI / 2, 0, 0]);
  }

  // Solar farm, starboard side only: a cross-boom with six arrays, the sheet's grey-blue wing.
  box(group, frame, [6.0, 30.0, 4.0], [78, 0, 0]);
  for (const iy of [-10, 0, 10]) for (const iz of [-7, 7]) {
    box(group, solarCell, [0.4, 8.0, 12.0], [84, iy, iz]);
    box(group, frame, [0.5, 8.6, 12.6], [84, iy, iz * 1.0]);
  }

  // Four berths, two per arm end, side by side across the tip — as drawn. Local +Y is the
  // outward normal, so the sites yaw ∓90° to face down the spine.
  berth(group, 'A', SPINE_HALF, -8, -Math.PI / 2, 'L');
  berth(group, 'B', SPINE_HALF, 8, -Math.PI / 2, 'L');
  berth(group, 'C', -SPINE_HALF, -8, Math.PI / 2, 'L');
  berth(group, 'D', -SPINE_HALF, 8, Math.PI / 2, 'L');

  // Approach lighting down the spine and strobes round the ring.
  const spineLights: number[][] = [];
  for (let i = -2; i <= 2; i++) spineLights.push([i * 20, 4.5, 0]);
  lamps(group, '#b7dfdd', spineLights, 0.5, 4);
  const ringStrobes: number[][] = [];
  for (let i = 0; i < 8; i++) {
    const angle = (i / 8) * TAU;
    ringStrobes.push([Math.cos(angle) * (RING + TUBE + 0.5), Math.sin(angle) * (RING + TUBE + 0.5), 0]);
  }
  lamps(group, '#e8c98a', ringStrobes, 0.5, 4);
  return group;
}

export const maps: Maps = (() => {
  const kit = standard_maps({
    seed: 5, panel: 0.2, rivets: 0.09, seams: 3, wear: 0.3, grime: 0.26, scorch: 1,
    stencils: [['WAYFARER', 0.08, 0.12]],
  });
  return {
    normal: kit.normal,
    roughness: kit.roughness,
    albedo: (ctx, size) => {
      kit.albedo?.(ctx, size);
      each_tile(ctx, size, (tile) => {
        hazard_stripes(tile, { x: 0.06, y: 0.78, w: 0.88, h: 0.12 }, { pitch: 0.04, angle: 0 });
        stencil_text(tile, 'DOCK', { x: 0.6, y: 0.3, scale: 0.026 });
      });
    },
  };
})();

export const meta = { name: 'station', scale: 1, collider: 'auto', untextured: ['solarCell'] };
