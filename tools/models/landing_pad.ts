// Landing pad (PLAN-03 §7.1, order of work item a): a real deck to set a ship down on. Origin at
// pad level — the deck's top face is the z = 0 plane — and the footprint is flat, so the radial
// heightfield in sim/terrain.cpp can force a level span under it. `dock.pad` faces local up, which
// the exporter records as `normal3: [0, 0, 1]` (the planar normal is legitimately zero: a pad does
// not face into the flight plane).
//
// G14 needs this asset before anything else in the brief, so the deck is also exported as
// `landing_pad_assembly()` for surface_base to stand its pads on: one implementation, not two.
import * as THREE from 'three';
import {
  dark, deckPlate, hazardPaint, hullPaint, lamp_material, metal, box, bevelled, cylinder, dock,
  each_tile, hazard_stripes, lathe, standard_maps, stencil_text, strut, tube, type Maps,
} from './prims';

export interface PadOptions {
  /** Deck radius, metres. The touchdown gate's 12 m acceptance circle is marked inside it. */
  radius?: number;
  /** Hold-down clamps around the touchdown circle. */
  clamps?: number;
  /** Approach light masts around the perimeter. */
  lights?: number;
  /** `dock.<id>` for this deck; omit for a pad that is not itself a berth. */
  id?: string;
  /** Approach-light colour. Runway white by default, ochre for a service pad. */
  lamp?: string;
}

/**
 * One complete pad, centred on `parent`'s local origin, deck top at z = 0. Split out of `build()`
 * because a surface base is a handful of these plus buildings, and the clamps and approach lighting
 * are the parts worth authoring once.
 */
export function landing_pad_assembly(parent: THREE.Object3D, options: PadOptions = {}): THREE.Group {
  const group = new THREE.Group();
  group.name = 'pad';
  const radius = options.radius ?? 22;
  const clamps = options.clamps ?? 4;
  const lights = options.lights ?? 8;
  const lamp = options.lamp ?? '#efe4bb';

  // Deck: a 24-sided plate with a chamfered rim, a shallow skirt and a sub-base ring, all of it
  // below z = 0 except for the relief that reads as the deck surface.
  tube(group, deckPlate, radius, radius, 1.6, [0, 0, -0.8], 16, [Math.PI / 2, 0, 0]);
  lathe(group, metal, [[radius, 0], [radius + 1.2, -0.5], [radius + 1.2, -1.6], [radius - 3.0, -3.4]], 14, [0, 0, 0], [Math.PI / 2, 0, 0]);
  tube(group, dark, radius - 3.4, radius - 5.0, 2.2, [0, 0, -4.4], 14, [Math.PI / 2, 0, 0]);
  // Deck relief: eight radial beams and two concentric rings, 0.24 m proud — this is what the
  // planar tile reads against, and low enough that nothing trips over the pad edge.
  for (let i = 0; i < 8; i++) {
    const angle = (i / 8) * Math.PI * 2;
    box(group, metal, [0.5, radius - 4.5, 0.24], [Math.sin(angle) * (radius - 2.2) * 0.5, Math.cos(angle) * (radius - 2.2) * 0.5, 0.12], -angle);
  }
  tube(group, metal, radius - 4.0, radius - 4.0, 0.24, [0, 0, 0.12], 16, [Math.PI / 2, 0, 0]);
  tube(group, metal, 13.6, 13.6, 0.24, [0, 0, 0.12], 16, [Math.PI / 2, 0, 0]);
  // The touchdown gate: a painted circle at 12 m, exactly the gate's distance limit (§3.4).
  tube(group, hazardPaint, 12.2, 12.2, 0.18, [0, 0, 0.16], 16, [Math.PI / 2, 0, 0]);
  for (let i = 0; i < 12; i++) {
    const angle = (i / 12) * Math.PI * 2;
    box(group, hazardPaint, [2.6, 0.8, 0.16], [Math.sin(angle) * 12.2, Math.cos(angle) * 12.2, 0.16], -angle);
  }
  // Hold-down clamps: base casting, jaw, hydraulic ram and hinge pin, spaced on the gate circle.
  for (let i = 0; i < clamps; i++) {
    const angle = (i / clamps) * Math.PI * 2 + Math.PI / clamps;
    const at = [Math.sin(angle) * 14.6, Math.cos(angle) * 14.6];
    bevelled(group, dark, [3.4, 5.2, 1.8], [at[0], at[1], 0.9], [0, 0, -angle], { radius: 0.45, segments: 1 });
    bevelled(group, metal, [2.2, 3.4, 1.2], [at[0] - Math.sin(angle) * 1.6, at[1] - Math.cos(angle) * 1.6, 2.3], [0, 0, -angle], { radius: 0.3, segments: 1 });
    cylinder(group, metal, 0.55, 0.8, 3.2, [at[0] + Math.sin(angle) * 2.4, at[1] + Math.cos(angle) * 2.4, 1.9], 8);
    cylinder(group, hullPaint, 0.9, 0.9, 5.4, [at[0], at[1], 2.6], 8);
    cylinder(group, dark, 1.5, 1.5, 0.6, [at[0], at[1], 4.4], 6, [Math.PI / 2, 0, 0]);
  }
  // Approach lighting: masts at the perimeter with a lamp head on each, plus flush edge lamps.
  const heads: number[][] = [];
  for (let i = 0; i < lights; i++) {
    const angle = (i / lights) * Math.PI * 2;
    const x = Math.sin(angle) * (radius + 2.6), y = Math.cos(angle) * (radius + 2.6);
    strut(group, metal, [x, y, -0.6], [x, y, 4.6], 0.35, 6);
    box(group, dark, [1.6, 1.2, 0.8], [x, y, 5.0], -angle);
    heads.push([x, y, 5.4]);
  }
  const material = lamp_material(lamp);
  for (const head of heads) {
    const mesh = new THREE.Mesh(new THREE.SphereGeometry(0.6, 6, 4), material);
    mesh.position.set(head[0], head[1], head[2]);
    group.add(mesh);
  }
  // Service trench, cabinet and the umbilical mast a landed ship takes fuel and power from.
  box(group, dark, [3.2, 10.0, 0.7], [0, -(radius - 8.5), 0.1]);
  bevelled(group, hullPaint, [3.0, 3.4, 3.2], [radius - 8.0, radius - 9.0, 1.6], [0, 0.4, 0], { radius: 0.35, segments: 1 });
  strut(group, metal, [-(radius - 7.0), -(radius - 7.0), 0], [-(radius - 7.0), -(radius - 7.0), 9.0], 0.5, 6);
  bevelled(group, metal, [2.4, 1.4, 1.4], [-(radius - 7.0), -(radius - 7.0), 9.4], [0, 0.7, 0], { radius: 0.3, segments: 1 });
  strut(group, dark, [-(radius - 7.0), -(radius - 7.0), 8.4], [-(radius - 3.4), -(radius - 3.0), 4.2], 0.22, 6);
  const umbilical = new THREE.Mesh(new THREE.SphereGeometry(0.55, 8, 6), lamp_material('#b7dfdd'));
  umbilical.position.set(-(radius - 7.0), -(radius - 7.0), 9.9);
  group.add(umbilical);
  if (options.id) dock(group, options.id, [0, 0, 0], 0, 'L', Math.PI / 2);
  parent.add(group);
  return group;
}

export function build(): THREE.Object3D {
  const group = new THREE.Group(); group.name = 'landing_pad';
  landing_pad_assembly(group, { id: 'pad' });
  return group;
}

export const maps: Maps = (() => {
  const kit = standard_maps({ seed: 63, panel: 0.18, rivets: 0.09, seams: 2, wear: 0.3, grime: 0.24, stencils: [['LP-01', 0.1, 0.14]] });
  return {
    normal: kit.normal,
    roughness: kit.roughness,
    albedo: (ctx, size) => {
      kit.albedo?.(ctx, size);
      each_tile(ctx, size, (tile) => {
        hazard_stripes(tile, { x: 0.06, y: 0.60, w: 0.88, h: 0.16 }, { pitch: 0.06, angle: -Math.PI / 4 });
        stencil_text(tile, 'MAX 12 M/S', { x: 0.1, y: 0.4, scale: 0.026 });
      });
    },
  };
})();

export const meta = { name: 'landing_pad', scale: 1, collider: 'auto' };
