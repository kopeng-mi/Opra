// Probe scanner (PLAN-03 §7.1, props): a small expendable scan probe, roughly 3-5 m from the motor
// skirt to the lens. It is built around the one thing it exists for — the aperture package forward
// of the bus — so the barrel, its dark aperture ring and the glass lens sit on the nose axis, with
// the four instrument booms hinged off the bus shoulders behind them.
//
// Axes as always: nose +Y, dorsal +Z, starboard +X, metres.
import * as THREE from 'three';
import {
  copper, dark, frame, glass, hazardPaint, insulation, lightArmor, metal, solarCell,
  bevelled, box, dish, dome, each_tile, effect_cone, hazard_stripes, lathe,
  standard_maps, stencil_text, strut, tube, type Maps,
} from './prims';

export function build(): THREE.Object3D {
  const group = new THREE.Group(); group.name = 'probe_scanner';
  const effects: THREE.Mesh[] = [];

  // Bus: a compact core, a dorsal harness cover, and frame rails carrying the cell plates that
  // power it for the few hours it lives.
  bevelled(group, lightArmor, [1.3, 1.5, 1.3], [0, 0, 0], [0, 0, 0], { radius: 0.26, segments: 1 });
  box(group, metal, [1.1, 1.2, 0.28], [0, 0, 0.78]);
  for (const side of [-1, 1]) {
    box(group, frame, [0.12, 1.42, 1.12], [side * 0.7, 0, 0]);
    box(group, solarCell, [0.1, 1.3, 1.0], [side * 0.78, 0, 0]);
  }

  // Sensor package: barrel, aperture ring, lens. The lens is `glass` on purpose — it is an optic,
  // so the procedural maps leave it clean (meta.untextured).
  tube(group, metal, 0.46, 0.58, 1.1, [0, 1.2, 0], 12);
  tube(group, dark, 0.5, 0.5, 0.16, [0, 1.82, 0], 12);
  dome(group, glass, 0.4, [0, 1.88, 0], [0, 0, 0], 6);

  // Instrument booms: four hinged masts raked aft out of the shoulders, each with a sensor head.
  for (let i = 0; i < 4; i++) {
    const angle = (i / 4) * Math.PI * 2 + Math.PI / 4;
    const cx = Math.cos(angle), sz = Math.sin(angle);
    const root = [cx * 0.66, -0.3, sz * 0.66];
    const tip = [cx * 1.75, -1.5, sz * 1.75];
    tube(group, metal, 0.09, 0.09, 0.34, root, 4);
    strut(group, frame, root, tip, 0.055, 4);
    box(group, dark, [0.3, 0.4, 0.3], tip);
  }

  // Solid motor: the probe is kicked onto its scan track and then has nothing left to do.
  lathe(group, metal, [[0.42, -0.75], [0.55, -1.35], [0.55, -1.9], [0.36, -2.2]], 10);
  lathe(group, dark, [[0.18, -2.15], [0.2, -2.35], [0.44, -2.65]], 10);
  effect_cone(group, effects, { name: 'motor', radius: 0.26, length: 1.1, pos: [0, -2.7, 0] });

  // High-gain antenna: fixed on the ventral face with the bus as its rough pointing reference.
  dish(group, insulation, 0.82, 0.3, [0, -0.15, -1.0], [-Math.PI / 2, 0, 0], 8);
  const focus = [0, -0.15, -1.0 - 0.3 - 0.82 * 0.55];
  for (let i = 0; i < 3; i++) {
    const angle = (i / 3) * Math.PI * 2 + 0.4;
    strut(group, metal, [Math.cos(angle) * 0.78, -0.15 + Math.sin(angle) * 0.78, -1.3], focus, 0.045, 4);
  }
  lathe(group, copper, [[0.09, 0], [0.09, 0.4]], 8, [focus[0], focus[1], focus[2]], [Math.PI / 2, 0, 0]);

  // Launch cradle: the separation ring the dispenser clamps, its four lugs and the arms that hold
  // them. The motor passes through the ring's throat, which is why the ring is wider than the case.
  tube(group, dark, 0.74, 0.78, 0.3, [0, -0.9, 0], 12);
  for (let i = 0; i < 4; i++) {
    const angle = i * Math.PI / 2 + Math.PI / 4;
    const cx = Math.cos(angle), sz = Math.sin(angle);
    box(group, hazardPaint, [0.24, 0.3, 0.24], [cx * 0.72, -0.9, sz * 0.72]);
    strut(group, metal, [cx * 0.7, -0.9, sz * 0.7], [cx * 1.0, -0.58, sz * 1.0], 0.05, 4);
  }

  return group;
}

/** Panels, rivets and the release band, with the unit number and the no-handling warning. */
export const maps: Maps = (() => {
  const kit = standard_maps({ seed: 52, panel: 0.11, rivets: 0.06, seams: 1, wear: 0.3, grime: 0.18, stencils: [['PRB-9', 0.12, 0.18]] });
  return {
    normal: kit.normal,
    roughness: kit.roughness,
    albedo: (ctx, size) => {
      kit.albedo?.(ctx, size);
      each_tile(ctx, size, (tile) => {
        hazard_stripes(tile, { x: 0.06, y: 0.66, w: 0.88, h: 0.16 }, { pitch: 0.045, angle: -Math.PI / 4 });
        stencil_text(tile, 'PRB-9', { x: 0.58, y: 0.26, scale: 0.028, mirror: true });
        stencil_text(tile, 'EXPENDABLE', { x: 0.08, y: 0.42, scale: 0.018 });
      });
    },
  };
})();

// The cell plates and the sensor lens stay clean; this model carries no other `glass`.
export const meta = { name: 'probe_scanner', scale: 1, collider: 'auto', untextured: ['glass', 'solarCell'] };
