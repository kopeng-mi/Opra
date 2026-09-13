// section_combat_a: Variant A weapons section, 4 m axial, section_weapons.
//
// The armoured waist. A hex pressure hull rolled 30 degrees about Y so the deck is flat and the
// flanks carry chines, a dorsal armour spine running the full station, and two PDC turrets placed
// on opposite quadrants (port-dorsal, starboard-ventral) so the pair covers the sphere rather than
// doubling one hemisphere - which is why they are not mirrored.
import * as THREE from 'three';
import {
  copper, dark, glow, lightArmor, metal, ochre,
  box, flange, standard_maps, strut, tube, type Maps,
} from './prims';

const CHINE = [0, Math.PI / 6, 0];

export function build(): THREE.Object3D {
  const root = new THREE.Group();
  root.name = 'section_combat_a';

  flange(root, metal, [0, 0, 0], [0, -1, 0]).name = 'flange';

  // Primary pressure hull, 3.4 m across the chines, tapering slightly forward.
  tube(root, lightArmor, 1.58, 1.7, 3.94, [0, 2.0, 0], 6, CHINE);
  // Bulkhead girth bands: heavier plate over the frames the hull sections bolt to.
  tube(root, dark, 1.74, 1.74, 0.3, [0, 0.52, 0], 6, CHINE);
  tube(root, dark, 1.62, 1.62, 0.3, [0, 3.48, 0], 6, CHINE);

  // Dorsal armour spine: the belt over the magazine and the fire-control trunk. 1.35 m wide, which
  // is also what keeps the plan view from being one grey - it is the topmost surface on the module.
  box(root, dark, [1.3, 4.0, 0.8], [0, 2.0, 1.32]);
  box(root, ochre, [0.95, 0.95, 0.14], [0, 2.9, 1.76]);   // magazine loading hatch, hazard-marked

  // PDC turrets: traversing drum plus twin barrels, on opposite quadrants.
  tube(root, dark, 0.68, 0.8, 0.62, [-0.95, 2.6, 1.12], 8, [0, 0, 0.2]);
  box(root, metal, [0.24, 1.55, 0.24], [-0.95, 3.42, 1.28], 0.2);
  tube(root, dark, 0.68, 0.8, 0.62, [0.95, 1.4, -1.12], 8, [0, 0, -0.2]);
  box(root, metal, [0.24, 1.55, 0.24], [0.95, 2.22, -1.28], -0.2);

  // Flank conduit raceways: the power and coolant runs between the reactor aft and the bridge
  // forward have to go somewhere, and on a hull this size they go outside the pressure boundary.
  for (const sx of [-1, 1]) box(root, metal, [0.5, 2.7, 0.46], [sx * 0.98, 2.0, 1.3]);
  // Coolant risers feeding each turret's heat sink.
  strut(root, copper, [-1.44, 0.8, 0.86], [-1.44, 3.3, 0.86], 0.12, 4);
  strut(root, copper, [1.44, 0.8, -0.86], [1.44, 3.3, -0.86], 0.12, 4);

  // Ventral ECM fairings, one pair - the countermeasure dispensers sit where the hull is coldest.
  for (const sx of [-1, 1]) box(root, dark, [0.8, 1.7, 0.58], [sx * 1.2, 3.1, -0.86]);
  // Formation lights, pinpoint, on the chine.
  for (const sx of [-1, 1]) box(root, glow, [0.2, 0.2, 0.2], [sx * 1.64, 0.85, 0.3]);

  return root;
}

export const maps: Maps = standard_maps({
  seed: 702, panel: 0.16, rivets: 0.06, seams: 2, wear: 0.06, grime: 0.05, scorch: 1,
  stencils: [['MAG-A', 0.14, 0.22], ['PDC-2', 0.58, 0.66]],
});

export const meta = {
  name: 'section_combat_a', kind: 'section_weapons', span: 1, axial: true, modular: true,
  flange: { pos: [0, 0, 0], normal: [0, -1, 0] },
  mass: 9.0, thrust: 0, propellant: 0, cooling: 0, heat_capacity: 14,
  rcs_jets: 0, rcs_authority: 0,
  scale: 1, collider: 'auto', untextured: ['glow'],
};
