// Hull tank module (plan-04 §2.5, ref 17): the tank variant of the 4.0 m / 2.5 m flange
// family. Quilted insulation blanket over the barrel, one external pipe run with its valve
// block, and the shared flange at BOTH ends — identical part, identical bolt pattern, so a
// ship stacks tank against hab against truss without adapters.
//
// Axis +Y, origin at the module centre. `tank_large` stays the depot's ground tank; this is
// the flight article.
import * as THREE from 'three';
import {
  copper, dark, frame, hazardPaint, insulation, metal,
  box, each_tile, flange, hazard_stripes, lathe, standard_maps, stencil_text, strut,
  tube, type Maps,
} from './prims';

/** Family envelope: 4.0 m flange to flange, 2.5 m across. */
export const MODULE_HALF = 2.0;
export const MODULE_RADIUS = 1.25;

export function build(): THREE.Object3D {
  const group = new THREE.Group(); group.name = 'tank_small';

  // Pressure shell: barrel with shallow domes tucked just inside the flange faces, one lathe
  // so the ends are real surfaces of revolution.
  lathe(group, metal, [
    [0, -1.95], [0.7, -1.9], [1.05, -1.72], [MODULE_RADIUS, -1.45],
    [MODULE_RADIUS, 1.45], [1.05, 1.72], [0.7, 1.9], [0, 1.95],
  ], 20);
  // Quilted blanket: lagging over the barrel, chamfered onto the shell at both rims, with
  // stitch rings that quilt it — the texture the sheet draws.
  lathe(group, insulation, [
    [MODULE_RADIUS, -1.35], [MODULE_RADIUS + 0.12, -1.15],
    [MODULE_RADIUS + 0.12, 1.15], [MODULE_RADIUS, 1.35],
  ], 20);
  for (const y of [-0.9, -0.3, 0.3, 0.9])
    tube(group, metal, MODULE_RADIUS + 0.16, MODULE_RADIUS + 0.16, 0.09, [0, y, 0], 20);
  // External pipe run: off the crown dome, down past the blanket on brackets, into a valve
  // block with its handwheel — the run stands clear of the stitch rings so it reads as
  // bracketed to them, not buried in them.
  const run = MODULE_RADIUS + 0.3;
  strut(group, copper, [0.55, 1.95, 0], [run, 1.2, 0], 0.09, 6);
  strut(group, copper, [run, 1.2, 0], [run, -1.35, 0], 0.09, 6);
  for (const y of [0.9, -0.1, -1.1]) box(group, frame, [0.2, 0.14, 0.3], [MODULE_RADIUS + 0.12, y, 0]);
  box(group, metal, [0.42, 0.5, 0.42], [run, -1.55, 0]);
  strut(group, metal, [run + 0.18, -1.55, 0], [run + 0.5, -1.55, 0], 0.07, 6);
  tube(group, hazardPaint, 0.24, 0.24, 0.08, [run + 0.52, -1.55, 0], 10, [0, 0, Math.PI / 2]);
  // Crown fittings: manway and relief stub, the two things that make a vessel read as one.
  tube(group, metal, 0.34, 0.34, 0.2, [0, 1.98, 0], 10);
  tube(group, copper, 0.11, 0.13, 0.34, [0.5, 1.9, 0], 8);

  // The interface, both ends, one part.
  flange(group, metal, [0, -MODULE_HALF, 0], [0, -1, 0]);
  flange(group, metal, [0, MODULE_HALF, 0], [0, 1, 0]);
  return group;
}

export const maps: Maps = (() => {
  const kit = standard_maps({
    seed: 21, panel: 0.17, rivets: 0.085, seams: 2, wear: 0.3, grime: 0.26, scorch: 1,
    stencils: [['FUEL', 0.1, 0.18]],
  });
  return {
    normal: kit.normal,
    roughness: kit.roughness,
    albedo: (ctx, size) => {
      kit.albedo?.(ctx, size);
      each_tile(ctx, size, (tile) => {
        hazard_stripes(tile, { x: 0.06, y: 0.78, w: 0.88, h: 0.12 }, { pitch: 0.04, angle: 0 });
        stencil_text(tile, 'CRYO', { x: 0.55, y: 0.4, scale: 0.024 });
      });
    },
  };
})();

export const meta = { name: 'tank_small', scale: 1, collider: 'auto' };
