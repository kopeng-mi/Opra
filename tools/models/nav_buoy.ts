// Nav buoy (plan-04 §2.6, ref 31): a squat cylindrical marker on landing legs — ochre band,
// solar skirt, side reflector horn, piped cap and a caged lantern on top. 1.5 m tall. It is
// the cheapest thing in the brief because it is the cheapest thing in the world: a marker a
// tender drops and forgets. The lantern flash is the only geometry the simulation touches,
// so it is the one effect child — named `strobe` for the loader.
//
// Axes as always: nose +Y, dorsal +Z, starboard +X, metres. Origin at the foot plane.
import * as THREE from 'three';
import {
  copper, dark, glass, lightArmor, metal, ochre, solarCell,
  box, each_tile, hazard_stripes, lamp_material, lathe, standard_maps, stencil_text, strut,
  torus, tube, type Maps,
} from './prims';

export function build(): THREE.Object3D {
  const group = new THREE.Group(); group.name = 'nav_buoy';

  // Legs: four struts with round pads, splayed to stand the buoy off whatever it is set on.
  for (let i = 0; i < 4; i++) {
    const angle = (i / 4) * Math.PI * 2 + Math.PI / 4;
    const fx = Math.cos(angle) * 0.52, fz = Math.sin(angle) * 0.52;
    const hx = Math.cos(angle) * 0.3, hz = Math.sin(angle) * 0.3;
    strut(group, dark, [hx, 0.45, hz], [fx, 0.06, fz], 0.05, 6);
    tube(group, metal, 0.11, 0.13, 0.05, [fx, 0.03, fz], 10);
    box(group, metal, [0.1, 0.18, 0.1], [hx, 0.42, hz]);
  }
  // Body: a sealed drum with a stiffening band at its waist and a tie-down cleat low on the
  // flank for whoever recovers it.
  lathe(group, lightArmor, [[0.3, 0.35], [0.38, 0.5], [0.38, 1.05], [0.3, 1.15]], 16);
  tube(group, dark, 0.39, 0.39, 0.08, [0, 0.62, 0], 16);
  box(group, metal, [0.12, 0.14, 0.2], [0, 0.5, -0.4]);
  torus(group, metal, 0.1, 0.025, [0, 0.5, -0.48], [0, 0, 0], 4, 10);
  // Ochre band round the upper drum: the marking that reads at range.
  tube(group, ochre, 0.385, 0.385, 0.22, [0, 0.92, 0], 16);
  // Solar skirt: a band of cells round the lower drum, doubling as the flare that sheds water
  // off the recovery cleat.
  lathe(group, solarCell, [[0.38, 0.5], [0.44, 0.42], [0.44, 0.36]], 16);
  // Reflector horn: the corner reflector on the starboard flank, for whoever is ranging it.
  const horn = new THREE.Group();
  horn.position.set(0.42, 0.85, 0);
  horn.rotation.z = -Math.PI / 2;
  group.add(horn);
  lathe(horn, metal, [[0.16, 0], [0.16, 0.06], [0.05, 0.3], [0.02, 0.34]], 4);
  box(horn, glass, [0.1, 0.1, 0.1], [0, 0.2, 0]);
  // Cap: piped crown with the copper loop over the top and junction boxes either side.
  lathe(group, metal, [[0.3, 1.15], [0.32, 1.22], [0.2, 1.28]], 16);
  strut(group, copper, [0.2, 1.2, 0.15], [0.2, 1.45, 0.15], 0.04, 5);
  strut(group, copper, [0.2, 1.45, 0.15], [-0.2, 1.45, 0.15], 0.04, 5);
  strut(group, copper, [-0.2, 1.45, 0.15], [-0.2, 1.2, 0.15], 0.04, 5);
  for (const side of [-1, 1]) box(group, dark, [0.16, 0.14, 0.14], [side * 0.3, 1.22, -0.1]);
  // Lantern: a four-post cage on the crown with the green lamp inside. The housing is static;
  // only the flash is drawn by the game.
  for (let i = 0; i < 4; i++) {
    const angle = (i / 4) * Math.PI * 2 + Math.PI / 4;
    strut(group, dark, [Math.cos(angle) * 0.14, 1.28, Math.sin(angle) * 0.14],
      [Math.cos(angle) * 0.14, 1.5, Math.sin(angle) * 0.14], 0.025, 4);
  }
  tube(group, dark, 0.17, 0.17, 0.05, [0, 1.52, 0], 8);
  const strobe = new THREE.Mesh(new THREE.SphereGeometry(0.09, 8, 6), lamp_material('#b8e6c8'));
  strobe.name = 'strobe';
  strobe.position.set(0, 1.4, 0);
  strobe.userData.effect = true;
  strobe.visible = false;
  group.add(strobe);

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

// The skirt is a solar array and the reflector block is optics: both stay clean of the maps.
export const meta = { name: 'nav_buoy', scale: 1, collider: 'auto', untextured: ['glass', 'solarCell'] };
