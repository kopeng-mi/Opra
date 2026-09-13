// nose_hammerhead: Variant A bridge/bow module, 4 m axial, nose_command.
//
// The hammerhead is the CIC standing outboard of the pressure hull: a corvette's fire-control and
// optical sensors need baseline separation, so the bridge sponsons cantilever off the hull on
// carry-through beams and carry the wingtip RCS at the longest moment arm on the ship. Everything
// above the armour belt is CIC; everything forward of the neck is the sensor and torpedo bay.
//
// Hex sections are rotated 30 degrees about Y so the hull carries a flat dorsal deck and a chine at
// each flank, not a ridge. Un-rotated, three.js puts a hex vertex at +Z and the ship reads as a
// pipe from the 32 degree camera (TASTE-PROFILE s2: faceted panels, flat deck plating).
import * as THREE from 'three';
import {
  black, copper, dark, glass, glow, lightArmor, metal, ochre,
  box, flange, standard_maps, tube, type Maps,
} from './prims';

/** Hex roll that puts flats dorsal/ventral and vertices on the flanks. */
const CHINE = [0, Math.PI / 6, 0];

export function build(): THREE.Object3D {
  const root = new THREE.Group();
  root.name = 'nose_hammerhead';

  flange(root, metal, [0, 0, 0], [0, -1, 0]).name = 'flange';

  // Pressure-hull ring: the module's structural root, continuing the combat section's 3.4 m body.
  tube(root, lightArmor, 1.66, 1.72, 0.82, [0, 0.44, 0], 6, CHINE);

  // --- CIC sponsons: the hammerhead ---------------------------------------------------------
  // 4.8 m across a 3.4 m body. This is the ship's instant-read silhouette (handoff s2, beat 1).
  box(root, lightArmor, [4.8, 1.9, 1.5], [0, 1.58, -0.02]);
  // Dorsal armour deck over the CIC. Dark, and the highest surface on the module, so in plan view
  // it is what the eye (and the raster) sees - the hammerhead reads as armour, not as hull panel.
  // 3.2 m, not the full 4.8: the belt has to stop short of the sponson tips or the module goes to
  // one colour and the wings stop reading as wings.
  box(root, dark, [3.3, 1.92, 0.42], [0, 1.58, 0.78]);
  // Girth armour belt at the wing root: the join between sponson and pressure hull is the weak
  // line, so it carries the thickest plate on the module.
  box(root, dark, [5.0, 0.38, 1.6], [0, 0.66, -0.02]);
  // Fire-control radar faces, one per sponson, outboard of the armour deck. Flat black panels
  // canted nowhere - a phased array does not need to steer mechanically, and this is the reason
  // the sponsons exist at all: two arrays 4 m apart resolve what one cannot.
  for (const sx of [-1, 1]) box(root, black, [0.92, 1.55, 0.18], [sx * 1.92, 1.58, 0.74]);
  // Wing-root carry-through beams, ventral: what the sponsons are actually hanging from.
  for (const sx of [-1, 1]) box(root, metal, [1.7, 0.55, 0.55], [sx * 1.5, 0.52, -0.6]);

  // Armoured slit viewport and its brow. 1.9 m of glass behind a shutter housing - no glasshouse
  // (PLAN-07 s4.4), and the brow is what stops the sun blinding the watch.
  box(root, glass, [1.9, 0.32, 0.16], [0, 2.44, 0.5]);
  box(root, dark, [2.4, 0.34, 0.36], [0, 2.62, 0.64]);

  // Wingtip pods: RCS cluster plus the nav beacon, at the longest moment arm the hull offers.
  for (const sx of [-1, 1]) {
    box(root, metal, [0.76, 1.3, 0.95], [sx * 2.22, 1.58, -0.02]);
    box(root, glow, [0.22, 0.22, 0.22], [sx * 2.45, 1.58, 0.44]);
  }

  // Dorsal crew airlock on the port sponson only - a ship has one, and the asymmetry is the point
  // (TASTE-PROFILE s6).
  tube(root, dark, 0.56, 0.56, 0.34, [-1.5, 1.58, 1.08], 6, [Math.PI / 2, 0, 0]);
  box(root, ochre, [0.86, 0.86, 0.12], [-1.5, 1.58, 1.28]);

  // --- Bow: sensor wedge and torpedo bay ----------------------------------------------------
  tube(root, lightArmor, 1.12, 1.5, 0.78, [0, 2.88, 0], 6, CHINE);
  box(root, lightArmor, [2.2, 1.62, 1.4], [0, 3.4, 0]);
  // Armoured dorsal sensor cap: the forward optics and the fire-control radar live under it.
  box(root, dark, [1.66, 0.9, 0.64], [0, 3.8, 0.74]);
  // Forward optics, recessed into the bow face behind a black aperture frame.
  box(root, black, [1.36, 0.5, 0.52], [0, 4.14, 0.14]);
  box(root, glass, [1.02, 0.36, 0.36], [0, 4.22, 0.14]);
  // Twin torpedo tubes, ventral-forward, muzzles ringed in copper so the loader can see the bore.
  for (const sx of [-1, 1]) {
    tube(root, black, 0.34, 0.34, 1.3, [sx * 0.62, 3.55, -0.44], 6);
    tube(root, copper, 0.4, 0.4, 0.16, [sx * 0.62, 4.18, -0.44], 6);
  }

  return root;
}

export const maps: Maps = standard_maps({
  seed: 701, panel: 0.18, rivets: 0.06, seams: 2, wear: 0.05, grime: 0.04,
  stencils: [['CIC-1', 0.12, 0.2], ['TUBE 1-2', 0.62, 0.74]],
});

export const meta = {
  name: 'nose_hammerhead', kind: 'nose_command', span: 1, axial: true, modular: true,
  flange: { pos: [0, 0, 0], normal: [0, -1, 0] },
  mass: 7.5, thrust: 0, propellant: 0, cooling: 0, heat_capacity: 12,
  rcs_jets: 2, rcs_authority: 0.01534,
  scale: 1, collider: 'auto', untextured: ['glass', 'glow'],
};
