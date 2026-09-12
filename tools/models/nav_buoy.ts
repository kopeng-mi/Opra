// Nav buoy (PLAN-03 §7.1, props): a slim spar with a sealed body, a solar skirt, retroreflector
// plates, a tether loop and a mast-top strobe. It is the cheapest thing in the brief because it is
// the cheapest thing in the world: a marker a tender drops and forgets. The strobe is the only
// geometry the simulation touches, so it is the one effect child — named `strobe` for the loader.
//
// Axes as always: nose +Y, dorsal +Z, starboard +X, metres.
import * as THREE from 'three';
import {
  copper, dark, glass, lightArmor, metal, solarCell,
  box, each_tile, hazard_stripes, lamp_material, lathe, standard_maps, stencil_text, strut, torus,
  tube, type Maps,
} from './prims';

export function build(): THREE.Object3D {
  const group = new THREE.Group(); group.name = 'nav_buoy';

  // Spar: the mast everything is threaded onto, tapering to a whip at the top and running on below
  // the body to the counterweight that keeps the beacon end up.
  lathe(group, lightArmor, [[0.14, -1.9], [0.16, -0.6], [0.18, 0.2], [0.06, 1.2]], 12);
  // Body: a sealed drum with a stiffening band at its waist.
  lathe(group, metal, [[0.3, -0.75], [0.5, -0.45], [0.55, 0.25], [0.28, 0.7]], 12);
  tube(group, dark, 0.5, 0.5, 0.12, [0, -0.1, 0], 12);
  // Solar skirt: a shallow cone of cells, which doubles as the flare that stands the buoy off
  // whatever it is set down on.
  lathe(group, solarCell, [[0.3, -0.78], [1.0, -1.08], [1.06, -1.16]], 14);
  // Retroreflector plates: four corner cubes, one per cardinal face, for whoever is ranging it.
  box(group, glass, [0.2, 0.44, 0.3], [0.56, -0.15, 0]);
  box(group, glass, [0.2, 0.44, 0.3], [-0.56, -0.15, 0]);
  box(group, glass, [0.3, 0.44, 0.2], [0, -0.15, 0.56]);
  box(group, glass, [0.3, 0.44, 0.2], [0, -0.15, -0.56]);
  // Strobe mast: the housing is static, the flash is not. Only the flash is drawn by the game.
  tube(group, dark, 0.14, 0.2, 0.34, [0, 1.32, 0], 6);
  box(group, metal, [0.38, 0.08, 0.38], [0, 1.5, 0]);
  const strobe = new THREE.Mesh(new THREE.SphereGeometry(0.2, 8, 6), lamp_material('#eef6ff'));
  strobe.name = 'strobe';
  strobe.position.set(0, 1.68, 0);
  strobe.userData.effect = true;
  strobe.visible = false;
  group.add(strobe);
  // Tether loop: the ring a tender's line is hooked through to recover it.
  torus(group, metal, 0.28, 0.05, [0, -1.24, 0], [Math.PI / 2, 0, 0], 4, 12);
  // Two recovery whips off the body, well clear of the strobe's line of sight.
  for (const side of [-1, 1]) strut(group, copper, [side * 0.3, 0.5, 0], [side * 0.62, 1.05, 0], 0.035, 4);

  return group;
}

/** Panels, rivets and the hazard band around the waist, plus the buoy's unit number. */
export const maps: Maps = (() => {
  const kit = standard_maps({ seed: 77, panel: 0.14, rivets: 0.07, seams: 1, wear: 0.34, grime: 0.26, stencils: [['NB-3', 0.12, 0.2]] });
  return {
    normal: kit.normal,
    roughness: kit.roughness,
    albedo: (ctx, size) => {
      kit.albedo?.(ctx, size);
      each_tile(ctx, size, (tile) => {
        hazard_stripes(tile, { x: 0.06, y: 0.58, w: 0.88, h: 0.18 }, { pitch: 0.05, angle: Math.PI / 4 });
        stencil_text(tile, 'NB-3', { x: 0.6, y: 0.22, scale: 0.03, mirror: true });
      });
    },
  };
})();

// The skirt is a solar array and the reflector plates are optics: both stay clean of the maps.
export const meta = { name: 'nav_buoy', scale: 1, collider: 'auto', untextured: ['glass', 'solarCell'] };
