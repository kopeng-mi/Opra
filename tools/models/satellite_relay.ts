// Satellite relay (PLAN-03 §7.1, props): a deployable comms relay. The drawn model is the deployed
// state — bus, two solar wings on A-frame booms, a dorsal high-gain dish, omni whips and horn
// antennas — and `hp.unfolded` is the frame that structure is authored in. The stowed state hangs
// off `hp.folded` as the same wings flat against the bus flanks and the dish stowed against the
// ventral deck, which is what a modular loader needs to hand a stowed relay to a fairing.
//
// Trade-off, stated plainly: the exporter merges every opaque mesh it can whether or not it is
// visible, so a stowed state authored as ordinary geometry would draw through the deployed one
// forever. Every stowed mesh therefore takes `userData.effect = true` and `visible = false`: effect
// meshes are exported as authored and the loader drives them from the simulation, so the folded set
// lands in the additive slot nothing draws — and `hp.folded` still records its anchor in the
// sidecar. Because effects are never merged, the folded meshes also stay where they were authored:
// in the GLB they hang off `hp.folded`, which is what a modular loader wants to find there. The
// deployed hull, being ordinary geometry, is merged to the root and only its anchor survives.
//
// Axes as always: nose +Y, dorsal +Z, starboard +X, metres.
import * as THREE from 'three';
import {
  copper, dark, frame, insulation, lightArmor, metal, solarCell,
  bevelled, box, dish, each_tile, effect_cone, hazard_stripes, hp, lathe,
  standard_maps, stencil_text, strut, torus, tube, type Maps,
} from './prims';

/**
 * Flags a stowed-state subtree as an effect. The group's name carries to each mesh with an index,
 * so the sidecar's `effects` list says which folded part every entry is.
 */
function stow(node: THREE.Object3D): void {
  let part = 0;
  node.traverse((child) => {
    if (!child.isMesh) return;
    child.name = `${node.name}.${part++}`;
    child.userData.effect = true;
    child.visible = false;
  });
}

export function build(): THREE.Object3D {
  const group = new THREE.Group(); group.name = 'satellite_relay';
  const effects: THREE.Mesh[] = [];
  const deployed = hp(group, 'unfolded', [0, 0, 0]);

  // Bus: a boxed core, a raised dorsal equipment deck, a ventral deck the folded state stows
  // against, and frame rails along both flanks.
  bevelled(deployed, lightArmor, [2.6, 3.4, 2.8], [0, 0, 0], [0, 0, 0], { radius: 0.3, segments: 1 });
  box(deployed, metal, [2.1, 2.8, 0.36], [0, 0, 1.56]);
  box(deployed, dark, [2.3, 3.0, 0.3], [0, 0, -1.56]);
  for (const side of [-1, 1]) {
    strut(deployed, frame, [side * 1.36, -1.5, 1.2], [side * 1.36, 1.5, 1.2], 0.11, 5);
    strut(deployed, frame, [side * 1.36, -1.5, -1.2], [side * 1.36, 1.5, -1.2], 0.11, 5);
    // Radiator blades under the ventral deck: the relay dissipates what its transmitter makes.
    box(deployed, dark, [0.14, 2.6, 1.2], [side * 1.5, 0, -2.35]);
  }
  // Kick motor and its plume: the relay is delivered to its slot, then deploys.
  lathe(deployed, metal, [[0.5, -1.7], [0.62, -2.6], [0.52, -3.4]], 14);
  tube(deployed, dark, 0.3, 0.5, 0.5, [0, -3.7, 0], 12);
  effect_cone(deployed, effects, { name: 'kick', radius: 0.34, length: 1.5, pos: [0, -3.95, 0] });
  // Antennas: a whip pair off the dorsal deck and a wide-beam horn on each shoulder.
  for (const side of [-1, 1]) {
    strut(deployed, copper, [side * 0.85, 1.4, 1.7], [side * 1.15, 2.7, 2.2], 0.06, 5);
    tube(deployed, metal, 0.2, 0.3, 0.5, [side * 1.0, 1.05, 2.05], 8, [Math.PI / 2, 0, 0]);
  }

  // Solar wings: an A-frame boom pair out of each flank, a hinge drum, two framed panels a side.
  for (const side of [-1, 1]) {
    strut(deployed, frame, [side * 1.3, 0.95, 0.5], [side * 3.3, 0.95, 0.3], 0.12, 6);
    strut(deployed, frame, [side * 1.3, -0.95, 0.5], [side * 3.3, -0.95, 0.3], 0.12, 6);
    tube(deployed, metal, 0.28, 0.28, 0.6, [side * 3.5, 0, 0.3], 8);
    for (let panel = 0; panel < 2; panel++) {
      const x = side * (4.5 + panel * 2.9);
      box(deployed, dark, [2.7, 3.6, 0.2], [x, 0, 0.3]);
      box(deployed, solarCell, [2.5, 3.4, 0.1], [x, 0, 0.42]);
    }
    // Inter-panel spar on the fold line: two wings are one wing, not two floating plates.
    strut(deployed, metal, [side * 5.95, -1.8, 0.3], [side * 5.95, 1.8, 0.3], 0.08, 4);
  }

  // High-gain dish: boresight dorsal, on a yoke off the deck, so the bus can hold its arrays while
  // the dish tracks. Rim, tripod feed and horn are the parts that make it read as a dish.
  dish(deployed, insulation, 1.75, 0.58, [0, 0.3, 3.4], [Math.PI / 2, 0, 0], 12);
  torus(deployed, metal, 1.7, 0.09, [0, 0.3, 3.98], [0, 0, 0], 5, 10);
  const focus = [0, 0.3, 3.4 + 0.58 + 1.75 * 0.55];
  for (let i = 0; i < 3; i++) {
    const angle = (i / 3) * Math.PI * 2 + 0.4;
    strut(deployed, metal, [Math.cos(angle) * 1.64, 0.3 + Math.sin(angle) * 1.64, 3.98], focus, 0.05, 4);
  }
  lathe(deployed, copper, [[0.13, 0], [0.13, 0.55]], 8, [focus[0], focus[1], focus[2] - 0.55]);
  const yoke = [0, 0.3, 3.2];
  for (const side of [-1, 1]) strut(deployed, metal, [side * 1.15, 0.3, 1.8], [side * 1.0, 0.3, 3.3], 0.13, 5);
  tube(deployed, dark, 0.3, 0.3, 0.7, yoke, 8);

  // Stowed state, all of it effect-flagged and hidden. Both wings fold flat on the flanks (two
  // plates a side, hinge drum at the root), the dish stows face-up against the ventral deck and the
  // booms are strapped outboard of the stacks. The parts are authored in the bus frame; `stowed`
  // drops them onto hp.folded's launch-interface plane, just clear of the folded stack's lowest
  // point, which is where a dispenser clamps a relay that is still folded.
  const folded = hp(group, 'folded', [0, 0, -2.0]);
  const stowed = new THREE.Group();
  stowed.position.set(0, 0, 2.0);
  folded.add(stowed);
  for (const side of [-1, 1]) {
    const stack = new THREE.Group();
    stack.name = side < 0 ? 'folded-wing-port' : 'folded-wing-starboard';
    for (let panel = 0; panel < 2; panel++) {
      const x = side * (1.14 + panel * 0.26);
      box(stack, dark, [0.22, 3.4, 2.8], [x, 0, 0]);
      box(stack, solarCell, [0.12, 3.2, 2.6], [x + side * 0.06, 0, 0]);
    }
    tube(stack, metal, 0.2, 0.2, 0.5, [side * 1.06, -1.35, 0], 8);
    stowed.add(stack);
    stow(stack);
  }
  const stowed_dish = new THREE.Group(); stowed_dish.name = 'folded-dish';
  lathe(stowed_dish, insulation, [[1.5, 0], [1.5, 0.22], [0.3, 0.3]], 14, [0, 0, -1.95], [Math.PI / 2, 0, 0]);
  stowed.add(stowed_dish);
  stow(stowed_dish);
  const stowed_booms = new THREE.Group(); stowed_booms.name = 'folded-booms';
  for (const side of [-1, 1]) strut(stowed_booms, frame, [side * 1.62, -1.4, 0], [side * 1.62, 1.4, 0], 0.07, 5);
  stowed.add(stowed_booms);
  stow(stowed_booms);

  return group;
}

/** Panels, rivets and the deployment band, with the unit number the roster prints against it. */
export const maps: Maps = (() => {
  const kit = standard_maps({ seed: 24, panel: 0.09, rivets: 0.05, seams: 2, wear: 0.26, grime: 0.2, stencils: [['RLY-2', 0.12, 0.16]] });
  return {
    normal: kit.normal,
    roughness: kit.roughness,
    albedo: (ctx, size) => {
      kit.albedo?.(ctx, size);
      each_tile(ctx, size, (tile) => {
        hazard_stripes(tile, { x: 0.06, y: 0.70, w: 0.88, h: 0.18 }, { pitch: 0.05, angle: Math.PI / 4 });
        stencil_text(tile, 'DEPLOY', { x: 0.1, y: 0.4, scale: 0.024 });
        stencil_text(tile, 'RLY-2', { x: 0.6, y: 0.24, scale: 0.03, mirror: true });
      });
    },
  };
})();

// The cells stay clean: a panel line drawn across a solar cell is a short circuit, not detail.
// `glass` is not listed because this model carries none of it.
export const meta = { name: 'satellite_relay', scale: 1, collider: 'auto', untextured: ['solarCell'] };
