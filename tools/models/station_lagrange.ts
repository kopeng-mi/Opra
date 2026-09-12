// Tessera L4 (PLAN-03 §7.1, order of work item e): the L4 base that assets/systems/nereid.json flies
// as `tessera_l4`. It is the largest built thing in the game, so the brief is not "a station" but a
// record of construction: the original truss and its drums are the oldest thing here, the ring was
// bolted around it a campaign later, the wing arrays and the lab stack are newer still, and each
// layer keeps its own material and panel treatment so the decades read at a glance.
//
// Arrangement. The habitat ring lies in the navigation plane and spins about the dorsal axis (z),
// which is the axis a cockpit view reads as a ring at all — the legacy `station` made the same
// choice, and its berth at the ring's rim is what the traffic here is modelled on. Through the hub
// runs the truss spine, along z: the ring turns on a bearing around the spine's midpoint, and the
// spine's ends carry the solar wings. So the structure that predates the habitation is visibly the
// thing everything else hangs off, and the ring is a pressurised vessel clamped around a working
// truss rather than a shape with trusses drawn on it.
//
// Ring radius 104 m: at 2.9 rev/min that is a full g on the outer deck, which is the number the rest
// of the layout is derived from (the deck depth is the tube diameter, the spokes are lift shafts,
// the ring's own hull is the pressure boundary). Axes as always: nose +Y, dorsal +Z, starboard +X.
import * as THREE from 'three';
import {
  armor, black, copper, dark, deckPlate, frame, glass, hazardPaint, hullPaint, insulation,
  lightArmor, metal, solarCell,
  bevelled, box, dock, dome, each_tile, hazard_stripes, hp, jet_nozzle, lamps, lathe,
  panel_relief, scorch, sensor_dish, standard_maps, stencil_text, strut, torus, truss_box, tube,
  type Maps,
} from './prims';

const RING_RADIUS = 104;              // centreline of the pressure hull
const RING_TUBE = 11.5;               // hull radius: 23 m of depth, six decks and the plant
const HUB_RADIUS = 26;                // the bearing's seat, sized off the spine's section
const SPINE_LENGTH = 140;
const SPINE_SECTION = 16;
const SPINE_BAYS = 8;
const CORE_HALF = 18;                 // half the length of the non-rotating hub

/** The azimuths the ring is laid out on. Six spokes at 60 degrees leave the fore and aft corridors
 *  (90 and 270) clear for the two hub berths, and the four pressure drums sit in the arc midpoints
 *  that are left: between 0-60, 120-180, 180-240 and 300-360. Everything on the ring is placed off
 *  these two lists, so a reader can check a clash by adding two angles rather than by measuring. */
const SPOKE_COUNT = 6;
const DRUM_AZIMUTH = [30, 150, 210, 330];

/** One berth: the cradle face a pilot aims at, the arms that carry it back to the structure, and the
 *  approach lighting down the corridor. `pos` and `rotation` are the `dock.` anchor's own — the
 *  rotation spins the berth about z, so the corridor runs along the anchor's local +Y. `cradle` is
 *  the host's pick-up points (truss corner, hub drum), which differ per berth and are the caller's
 *  to state rather than something this function could infer. */
function berth(root: THREE.Object3D, id: string, pos: number[], rotation: number, cls: string,
               cradle: number[][], lights: number[][]): void {
  // Bulkhead disc, then eight latch lugs on a 9.6 m circle: a berthing collar is a ring of latches
  // around a hole, not a plate, and the lugs alternate hazard paint and black. The striping a pilot
  // aims at is geometry because at the atlas's 1.28 m texel a painted stripe is a blur; the albedo
  // band lands on the same face anyway.
  tube(root, black, 10.5, 10.5, 1.2,
       [pos[0] + Math.sin(rotation) * 1.4, pos[1] - Math.cos(rotation) * 1.4, pos[2]], 16, [0, 0, rotation]);
  // The berth's own frame: local +Y is the corridor (outward along the anchor's normal), so the
  // cradle ring lies in the plane spanned by `inplane` and z.
  const inplane = [Math.cos(rotation), Math.sin(rotation)];
  for (let i = 0; i < 8; i++) {
    const angle = (i / 8) * Math.PI * 2;
    const out = Math.cos(angle) * 9.6;
    box(root, i % 2 === 0 ? hazardPaint : dark, [2.8, 3.4, 2.8],
        [pos[0] + inplane[0] * out, pos[1] + inplane[1] * out, pos[2] + Math.sin(angle) * 9.6], rotation);
  }
  // Arms from the host to the lug circle: they land on the ring, so the load path is visible.
  for (const point of cradle) {
    strut(root, frame, point, [pos[0] + inplane[0] * 8.0, pos[1] + inplane[1] * 8.0, pos[2]], 0.45, 6);
  }
  // Approach lighting: two rows of lamps on a beam apiece, each beam reaching back to the collar.
  // A lamp run floating in vacuum is the one thing that makes a station look like a diagram.
  const corridor = [-Math.sin(rotation), Math.cos(rotation)];
  for (let row = 0; row + 1 < lights.length; row += 2) {
    const [a, b] = [lights[row], lights[row + 1]];
    const length = Math.hypot(b[0] - a[0], b[1] - a[1], b[2] - a[2]);
    const mid = [(a[0] + b[0]) / 2, (a[1] + b[1]) / 2, (a[2] + b[2]) / 2];
    box(root, metal, [0.5, length + 5.0, 0.7], mid, rotation);
    strut(root, metal, [mid[0] - corridor[0] * (length / 2 + 2.5), mid[1] - corridor[1] * (length / 2 + 2.5), mid[2]], pos, 0.22, 5);
  }
  dock(root, id, pos, rotation, cls);
  lamps(root, '#efe4bb', lights, 1.5, 6);
}

/** The original unpressurised truss, plus everything bolted to it in the decades since. */
function spine(root: THREE.Object3D, effects: THREE.Mesh[]): void {
  // The spine is the station's axis, so the truss is turned from its own +Y onto z. Eight bays over
  // 140 m: a bay is one pressure-node spacing, which is how a truss is actually detailed.
  const truss = truss_box(root, frame, SPINE_LENGTH, SPINE_SECTION, SPINE_SECTION, SPINE_BAYS, 0.72);
  truss.rotation.x = Math.PI / 2;

  // Pressurised nodes on the truss: a drum with two stiffener collars and a hatch, at the spacing
  // the bays give. These are the spine's original working volume, before the ring existed.
  for (const at of [-62, -34, 34, 62]) {
    const side = at > 0 ? 1 : -1;
    tube(root, armor, 9.5, 9.5, 13.0, [0, 0, at], 14, [Math.PI / 2, 0, 0]);
    for (const end of [-1, 1]) tube(root, dark, 10.6, 10.6, 1.6, [0, 0, at + end * 7.4], 14, [Math.PI / 2, 0, 0]);
    box(root, lightArmor, [5.0, 3.4, 4.0], [0, side * 10.4, at]);
    box(root, black, [1.4, 1.0, 1.4], [0, side * 12.2, at]);
    // One consumables tank per node: enough to read as a working drum, not enough to clutter it.
    lathe(root, insulation, [[1.6, -5.0], [4.6, -3.6], [4.6, 3.6], [1.6, 5.0]], 14,
          [side * 5.6, side * 11.4, at], [Math.PI / 2, 0, 0]);
    // Handrails the maintenance runs follow: at this size a truss without them reads as a drawing.
    // On the drum's surface, so they read as welded rather than as two loose rods in space.
    for (const side2 of [-1, 1]) strut(root, metal, [side2 * 7.0, -7.0, at - 6.5], [side2 * 7.0, -7.0, at + 6.5], 0.2, 5);
  }

  // The oldest pressurised volume in the station: a coaxial drum from the first campaign, later
  // braced when the ring's core was built around the spine. It is the one the albedo scorches.
  lathe(root, insulation, [[11.0, -10.0], [13.5, -7.0], [13.5, 7.0], [11.0, 10.0]], 20, [0, 0, 24], [Math.PI / 2, 0, 0]);
  for (const at of [-6, 0, 6]) tube(root, metal, 14.2, 14.2, 1.4, [0, 0, 24 + at], 16, [Math.PI / 2, 0, 0]);
  tube(root, copper, 6.0, 6.0, 4.4, [0, 0, 36], 12, [Math.PI / 2, 0, 0]);
  for (const at of [-8, 8]) box(root, dark, [2.2, 2.2, 2.2], [13.0, 0, 24 + at]);

  // Newer addition: a lab stack on a deck pallet, with the raised panel treatment the older drums
  // do not have. It hangs off the +Y flank so the berth corridors on x stay clear.
  for (let i = 0; i < 3; i++) {
    bevelled(root, lightArmor, [9.0, 9.0, 8.6], [0, 20.0, -26 + (i - 1) * 9.6], [0, 0, 0], { radius: 0.9, segments: 1 });
  }
  box(root, deckPlate, [11.0, 11.0, 26.0], [0, 20.0, -26]);
  panel_relief(root, lightArmor, [9.4, 9.4, 9.0], [0, 20.0, -26], [0, 0, 0], { cols: 2, rows: 1, thickness: 0.7, depth: 0.5 });
  for (const end of [-1, 1]) {
    for (const side of [-1, 1]) strut(root, frame, [side * 4.5, 5.0, -26 + end * 12.0], [side * 4.5, 14.5, -26 + end * 12.0], 0.32, 5);
    strut(root, frame, [0, 12.0, -26 + end * 10.0], [0, 7.0, -26 + end * 5.0], 0.34, 6);
  }

  // Unpressurised cargo racks: a frame of struts around a pallet of crates, strapped, with nothing
  // sealed — the point of them is that the station is a working base, not a hotel.
  for (const at of [[0, -20, 18], [0, -20, -14]]) {
    const [x, y, z] = at;
    for (const sweep of [-6.5, 6.5]) for (const along of [-7.5, 7.5]) {
      strut(root, frame, [x + sweep, y, z + along], [x + sweep, y + 10.0, z + along], 0.34, 6);
    }
    for (const along of [-7.5, 7.5]) strut(root, frame, [x - 6.5, y + 10.0, z + along], [x + 6.5, y + 10.0, z + along], 0.34, 6);
    box(root, dark, [11.0, 7.4, 12.4], [x, y + 5.2, z]);
    for (const along of [-4.0, 0, 4.0]) box(root, copper, [12.0, 2.2, 3.4], [x, y + 8.4, z + along]);
    for (const sweep of [-6.0, 6.0]) box(root, metal, [0.5, 8.6, 14.0], [x + sweep, y + 5.4, z]);
    strut(root, frame, [x, y, z], [x, y + 10.0, z], 0.3, 5);
    for (const along of [-6.5, 6.5]) strut(root, frame, [0, -6.5, z + along], [x, y, z + along], 0.4, 6);
  }

  // Gantry: a walkway down the +Y flank with its rail, cross ties and a crane trolley — the thing
  // that makes a structure read as worked on rather than assembled.
  box(root, deckPlate, [4.6, 0.6, 116.0], [0, 15.4, 0]);
  for (const side of [-1, 1]) {
    box(root, frame, [0.22, 2.4, 116.0], [side * 2.2, 16.6, 0]);
    for (let i = -5; i <= 5; i += 2) strut(root, metal, [side * 2.2, 15.2, i * 10.5], [side * 2.2, 17.6, i * 10.5], 0.15, 5);
  }
  for (let i = -5; i <= 5; i++) {
    strut(root, metal, [0, 15.4, i * 10.5], [0, 8.0, i * 10.5], 0.24, 5);
    if (i % 2 === 0) strut(root, metal, [0, 15.4, i * 10.5], [7.0, 8.0, i * 10.5 - 5.0], 0.2, 5);
  }
  bevelled(root, hazardPaint, [4.4, 3.2, 5.4], [0, 16.6, 12], [0, 0, 0], { radius: 0.4, segments: 1 });
  box(root, frame, [0.4, 4.2, 0.4], [0, 13.3, 12]);
  hp(root, 'crane', [0, 19.4, 12]);

  // Fuel farm: cryo tanks in two clusters on the spine's flanks, with the transfer plumbing that
  // gives them a reason to be where they are.
  for (const [x, y, z] of [[0, 27, -52], [0, 27, -43], [0, -27, 43], [0, -27, 52]]) {
    lathe(root, insulation, [[1.7, -8.0], [5.4, -6.4], [5.4, 6.4], [1.7, 8.0]], 16, [x, y, z], [Math.PI / 2, 0, 0]);
    for (const end of [-1, 1]) tube(root, metal, 5.9, 5.9, 1.2, [x, y, z + end * 4.2], 12, [Math.PI / 2, 0, 0]);
    box(root, dark, [3.0, 3.0, 3.0], [x, y, z + 9.4]);
    strut(root, copper, [x, y, z + 9.4], [0, y * 0.22, z + 9.4], 0.3, 5);
  }
  for (const end of [-1, 1]) {
    strut(root, frame, [0, 7.0, end * 48], [0, 26.0, end * 48], 0.4, 6);
    strut(root, frame, [0, 26.0, end * 48], [0, 26.0, end * 62], 0.4, 6);
    strut(root, frame, [0, 26.0, end * 62], [0, 8.0, end * 62], 0.4, 6);
  }

  // Antenna farm on the dorsal end: the high-gain the base actually points at the primary, a
  // secondary on the ventral end, and the masts both stand on.
  strut(root, metal, [0, 4.0, 64.0], [0, 4.0, 86.0], 1.05, 8);
  for (let i = 0; i < 3; i++) {
    const angle = (i / 3) * Math.PI * 2 + 0.4;
    strut(root, dark, [Math.cos(angle) * 7.0, Math.sin(angle) * 7.0, 64.0], [Math.cos(angle) * 0.9, Math.sin(angle) * 0.9, 82.0], 0.18, 5);
  }
  sensor_dish(root, { radius: 7.5, depth: 2.6, pos: [0, 4.0, 88.0], rot: [Math.PI / 2, 0, 0], segments: 18, yoke: true, material: lightArmor });
  // The ventral end gets a whip and a pair of horns: the base's omni set, and enough to say the
  // dorsal mast is the exception rather than the rule.
  strut(root, metal, [0, -4.0, -64.0], [0, -4.0, -80.0], 0.5, 6);
  strut(root, dark, [0, -4.0, -80.0], [0, -4.0, -89.0], 0.16, 5);
  for (const end of [-1, 1]) {
    for (const side of [-1, 1]) {
      tube(root, metal, 0.5, 1.0, 4.0, [side * 6.0, end * 6.0, 70.0], 8);
      box(root, dark, [1.6, 1.6, 1.2], [side * 6.0, end * 6.0, 72.4]);
    }
  }

  // Station-keeping verniers: a ring station holds its spin axis, not its attitude, so the only
  // thrusters it carries are four on the spine's ends pushing along z. They are effects — the bells
  // are geometry, the plumes are the simulation's.
  for (const end of [-1, 1]) {
    for (const side of [-1, 1]) {
      const at = [side * 9.6, 0, end * 70];
      box(root, dark, [3.0, 3.0, 2.4], [side * 8.6, 0, end * 70.6]);
      jet_nozzle(root, effects, {
        name: `vernier.${end > 0 ? 'dorsal' : 'ventral'}.${side > 0 ? 's' : 'p'}`,
        radius: 2.2, pos: at, direction: [0, 0, end], length: 8,
      });
    }
  }
}

/** One deployable wing on a spine end: a cross-boom carrying one array outboard on each side, each
 *  array with its own radiator. `end` is the spine end (±1 on z). */
function wing(root: THREE.Object3D, end: number): void {
  const z = end * 66;
  const section = 5.5, boom = 62;
  // The boom's four longerons and its battens: a full lattice here would be invisible at the range a
  // 62 m boom is seen from, and the bays between battens are what the eye counts instead.
  for (const [dy, dz] of [[section, section], [section, -section], [-section, section], [-section, -section]]) {
    strut(root, frame, [-boom, dy, z + end * dz], [boom, dy, z + end * dz], 0.55, 6);
  }
  for (let i = -1; i <= 1; i++) {
    const at = i * (boom / 1.5);
    for (const [dy, dz] of [[section, section], [section, -section], [-section, section], [-section, -section]]) {
      if (dy > 0) strut(root, frame, [at, dy, z + end * dz], [at, -dy, z + end * dz], 0.42, 5);
      if (dz > 0) strut(root, frame, [at, dy, z + end * section], [at, dy, z - end * section], 0.42, 5);
    }
    if (i < 1) {
      for (const dz of [section, -section]) strut(root, frame, [at, section, z + end * dz], [at + boom / 1.5, -section, z + end * dz], 0.3, 5);
    }
  }
  // The saddle that clamps the boom to the truss's end bay.
  bevelled(root, hullPaint, [10.0, 13.0, 12.0], [0, 0, z], [0, 0, 0], { radius: 1.0, segments: 1 });

  for (const side of [-1, 1]) {
    const inner = side * (boom + 6);
    // Hinge and tension strut: the array folds against the boom for a burn and locks out here.
    bevelled(root, metal, [4.0, 5.0, 5.0], [inner, 0, z], [0, 0, 0], { radius: 0.5, segments: 1 });
    strut(root, metal, [inner, section, z + end * section], [inner + side * 8.0, 0, z], 0.3, 5);
    strut(root, metal, [inner, -section, z - end * section], [inner + side * 8.0, 0, z], 0.3, 5);
    // Four panel strings on a spar, held flat in the navigation plane: the station yaws the whole
    // wing to face the star, and a flat wing is the only orientation a cockpit view reads as solar.
    for (let p = 0; p < 4; p++) {
      const at = inner + side * (12 + p * 21);
      box(root, solarCell, [19.0, 26.0, 0.6], [at, 0, z]);
      box(root, frame, [21.0, 1.3, 1.4], [at, 13.6, z - end * 1.0]);
      box(root, frame, [21.0, 1.3, 1.4], [at, -13.6, z - end * 1.0]);
      for (let s = 0; s < 2; s++) box(root, metal, [0.6, 26.4, 0.9], [at - 7.0 + s * 14.0, 0, z + 0.5]);
      for (const edge of [-1, 1]) box(root, metal, [19.4, 0.6, 0.9], [at, edge * 12.6, z + 0.5]);
    }
    // The array's own radiator: a tab on a standoff above the boom, where it shadows neither the
    // cells nor the ring, and where it does not add to the station's already long x footprint.
    const radiator = inner + side * 40;
    box(root, dark, [16.0, 10.0, 0.5], [radiator, 21.0, z]);
    box(root, metal, [16.4, 0.5, 0.8], [radiator, 21.0, z + 0.6]);
    tube(root, metal, 1.1, 1.1, 8.0, [radiator, 15.0, z], 8, [Math.PI / 2, 0, 0]);
    strut(root, metal, [radiator, 21.0, z], [radiator, 10.0, z], 0.3, 5);
    strut(root, metal, [inner, 0, z], [radiator, 10.0, z], 0.28, 5);
  }
}

/** The rotating assembly, in its own named group so a loader can spin it about z. Nothing outside
 *  the ring lives in here: the ring is a rigid body, the spine is not. */
function ring_assembly(ring: THREE.Group): void {
  // Pressure hull. A ring habitat is a pressure vessel before it is a picture, so the hull is one
  // torus and every other feature is either clamped to it or cut into it.
  torus(ring, armor, RING_RADIUS, RING_TUBE, [0, 0, 0], [0, 0, 0], 12, 72);
  // Stiffener bands: a collar across the tube section every 22.5 degrees, which is the ring's frame
  // pitch and the spacing the window bands are laid out against.
  for (let i = 0; i < 10; i++) {
    const angle = (i / 10) * Math.PI * 2;
    tube(ring, metal, RING_TUBE + 1.5, RING_TUBE + 1.5, 2.4,
         [Math.cos(angle) * RING_RADIUS, Math.sin(angle) * RING_RADIUS, 0], 12, [0, 0, angle]);
  }
  // Window bands: three runs of panes — outer rim, dorsal face, ventral face — so the ring reads as
  // inhabited from above, from the side and from inside the rim, which is where ships see it from.
  for (let i = 0; i < 18; i++) {
    const angle = (i / 18) * Math.PI * 2 + 0.09;
    const x = Math.cos(angle), y = Math.sin(angle);
    box(ring, glass, [3.2, 6.0, 4.6], [x * (RING_RADIUS + RING_TUBE - 1.5), y * (RING_RADIUS + RING_TUBE - 1.5), 0], angle);
    for (const face of [-1, 1]) {
      box(ring, glass, [6.0, 2.8, 2.6], [x * RING_RADIUS, y * RING_RADIUS, face * (RING_TUBE - 1.0)], angle);
    }
  }

  // Pressure drums between the spokes: big tangential cylinders clamped across the hull, which is
  // where the decks widen out. The azimuth 150 one is the first campaign's drum — insulation over
  // its ribs instead of paint, and the scorch the albedo draws lands on it.
  for (const degrees of DRUM_AZIMUTH) {
    const angle = (degrees * Math.PI) / 180;
    const x = Math.cos(angle) * RING_RADIUS, y = Math.sin(angle) * RING_RADIUS;
    const tx = -Math.sin(angle), ty = Math.cos(angle);
    const older = degrees === 150;
    const shell = older ? insulation : hullPaint;
    tube(ring, shell, 14.0, 14.0, 44.0, [x, y, 0], 16, [0, 0, angle]);
    for (const end of [-1, 1]) {
      tube(ring, shell, 11.0, 14.0, 6.0, [x + tx * end * 25.0, y + ty * end * 25.0, 0], 10, [0, 0, angle]);
    }
    tube(ring, metal, 15.4, 15.4, 3.0, [x, y, 0], 16, [0, 0, angle]);
    for (const face of [-1, 1]) {
      for (const along of [-13, 0, 13]) {
        box(ring, glass, [7.0, 3.4, 2.4], [x + tx * along, y + ty * along, face * 13.2], angle);
      }
      box(ring, dark, [3.0, 3.0, 3.0], [x + (older ? tx * 17 : tx * -17), y + (older ? ty * 17 : ty * -17), face * 9.0], angle);
    }
  }

  // Six spokes, each a lift shaft rather than a strut: a pressure tube from the hub cage out to the
  // rim, a pair of longerons alongside it, and the collar at the rim end where it lands.
  for (let i = 0; i < SPOKE_COUNT; i++) {
    const angle = (i / SPOKE_COUNT) * Math.PI * 2;
    const inner = HUB_RADIUS + 2.0, outer = RING_RADIUS - RING_TUBE + 2.0;
    const mid = (inner + outer) / 2;
    tube(ring, armor, 6.2, 6.2, outer - inner, [Math.cos(angle) * mid, Math.sin(angle) * mid, 0], 12, [0, 0, angle]);
    for (const offset of [-1, 1]) {
      strut(ring, frame, [Math.cos(angle) * inner, Math.sin(angle) * inner, offset * 8.5],
            [Math.cos(angle) * outer, Math.sin(angle) * outer, offset * 8.5], 0.42, 5);
      // The shaft's own window run, off the spine side of the tube.
      box(ring, glass, [3.4, 2.6, 2.2], [Math.cos(angle) * (mid + 14), Math.sin(angle) * (mid + 14), offset * 6.6], angle);
    }
    tube(ring, metal, 8.2, 8.2, 3.0, [Math.cos(angle) * outer, Math.sin(angle) * outer, 0], 12, [0, 0, angle]);
  }

  // The ring's half of the hub: the cage the spokes land on and the bearing's outer race. The inner
  // race, the slip rings and the berths belong to the core, which does not turn.
  tube(ring, dark, 30.0, 30.0, 16.0, [0, 0, 0], 24, [Math.PI / 2, 0, 0]);
  tube(ring, metal, 31.4, 31.4, 4.0, [0, 0, 0], 24, [Math.PI / 2, 0, 0]);
  for (let i = 0; i < 12; i++) {
    const angle = (i / 12) * Math.PI * 2;
    box(ring, copper, [2.6, 2.6, 4.2], [Math.cos(angle) * 30.6, Math.sin(angle) * 30.6, 0], angle);
  }
  for (let i = 0; i < 6; i++) {
    const angle = (i / 6) * Math.PI * 2 + Math.PI / 6;
    strut(ring, metal, [Math.cos(angle) * 29.0, Math.sin(angle) * 29.0, -8.0], [Math.cos(angle) * 29.0, Math.sin(angle) * 29.0, 8.0], 0.3, 5);
  }

  // Rim lamps: four strobes on the outer band, phased off the axes so they miss both berth
  // corridors, because the ring's rotation should be visible from a distance — that is the one cue
  // that says the habitat is under spin rather than under construction.
  const strobes: number[][] = [];
  for (let i = 0; i < 4; i++) {
    const angle = (i / 4) * Math.PI * 2 + 0.3;
    strobes.push([Math.cos(angle) * (RING_RADIUS + RING_TUBE + 1.2), Math.sin(angle) * (RING_RADIUS + RING_TUBE + 1.2), 0]);
  }
  lamps(ring, '#b7dfdd', strobes, 1.4, 6);
}

/** The non-rotating core: the spine's mid station, the bearing's inner race, the slip-ring stack the
 *  ring draws power and data through, and the two hub berths. */
function hub_core(root: THREE.Object3D): void {
  // Core drum, built around the truss rather than beside it — the truss passes through it, which is
  // why nothing here is longer than the drum's own 52 m.
  lathe(root, armor, [[16.0, -CORE_HALF], [24.0, -13.0], [HUB_RADIUS, -6.0], [HUB_RADIUS, 6.0], [24.0, 13.0], [16.0, CORE_HALF]], 24,
        [0, 0, 0], [Math.PI / 2, 0, 0]);
  // The bearing is a ring in the honest sense, so it is a torus: the ring's race rides on this one.
  torus(root, metal, 20.5, 2.0, [0, 0, 0], [0, 0, 0], 8, 28);
  // Slip rings: a drum outboard of the bearing carrying three copper bands and their brush gear,
  // which is where the ring's power and data cross the gap. Outboard because that is where a
  // technician can reach the brushes without stopping the spin.
  tube(root, dark, 19.0, 19.0, 12.0, [0, 0, -22.0], 20, [Math.PI / 2, 0, 0]);
  for (const at of [-19.5, -22.0, -24.5]) {
    tube(root, copper, 20.2, 20.2, 2.0, [0, 0, at], 20, [Math.PI / 2, 0, 0]);
    box(root, black, [2.4, 2.4, 1.6], [18.6, 0, at]);
    strut(root, dark, [18.6, 0, at], [15.0, 0, at], 0.4, 5);
  }
  // Observation blisters: the only place on the core with a view, on the diagonals the spokes at 60
  // and 240 leave open, so no spoke crosses them as the ring turns.
  for (const angle of [Math.PI / 4, Math.PI * 1.25]) {
    dome(root, glass, 5.5, [Math.cos(angle) * 26.5, Math.sin(angle) * 26.5, 0], [0, 0, angle - Math.PI / 2], 14);
    tube(root, lightArmor, 6.2, 6.2, 1.4, [Math.cos(angle) * 25.0, Math.sin(angle) * 25.0, 0], 16, [0, 0, angle - Math.PI / 2]);
  }
  // Plant pods clamped either side of the bearing, and the pump deck each end of the core carries:
  // the transfer lines the fuel farm runs end here, which is what the deck is for.
  for (const end of [-1, 1]) {
    box(root, dark, [7.0, 7.0, 6.0], [0, 25.0, end * 13.0]);
    box(root, deckPlate, [20.0, 3.0, 1.2], [0, 0, end * CORE_HALF]);
    for (const side of [-1, 1]) strut(root, copper, [side * 9.5, 0, end * CORE_HALF], [side * 9.5, side * 12.5, end * 15.0], 0.3, 5);
  }
}

export function build(): THREE.Object3D {
  const group = new THREE.Group(); group.name = 'station_lagrange';
  const effects: THREE.Mesh[] = [];

  spine(group, effects);
  for (const end of [-1, 1]) wing(group, end);
  hub_core(group);

  // The ring is the one thing on the station that moves: its own node, named, with the whole
  // rotating assembly inside it and nothing else, so a loader can find it by name and spin it about
  // the dorsal axis. The spine, the wings and the core stay outside it because they do not turn.
  const ring = new THREE.Group();
  ring.name = 'habitat-ring';
  group.add(ring);
  ring_assembly(ring);

  // Four berths. The spine's flanks take the heavy hulls: the corridors run out along ±x at z = ±52,
  // clear above the ring and below the wing booms at z = ±66. The core's take the traffic that has
  // to come inside the rim: those corridors run along ±y at the hub, through the 90 and 270 arcs the
  // spoke layout leaves open.
  berth(group, 'A', [22, 0, 52], -Math.PI / 2, 'L',
        [[8.0, 5.0, 47.0], [8.0, -5.0, 47.0], [8.0, 5.0, 57.0], [8.0, -5.0, 57.0]],
        [[36, 6.5, 52], [54, 6.5, 52], [36, -6.5, 52], [54, -6.5, 52]]);
  berth(group, 'B', [-22, 0, -52], Math.PI / 2, 'L',
        [[-8.0, 5.0, -47.0], [-8.0, -5.0, -47.0], [-8.0, 5.0, -57.0], [-8.0, -5.0, -57.0]],
        [[-36, 6.5, -52], [-54, 6.5, -52], [-36, -6.5, -52], [-54, -6.5, -52]]);
  berth(group, 'C', [0, 36, 0], 0, 'M',
        [[6.0, 24.0, 4.0], [-6.0, 24.0, 4.0], [6.0, 24.0, -4.0], [-6.0, 24.0, -4.0]],
        [[6.5, 46, 0], [6.5, 62, 0], [-6.5, 46, 0], [-6.5, 62, 0]]);
  berth(group, 'D', [0, -36, 0], Math.PI, 'M',
        [[6.0, -24.0, 4.0], [-6.0, -24.0, 4.0], [6.0, -24.0, -4.0], [-6.0, -24.0, -4.0]],
        [[6.5, -46, 0], [6.5, -62, 0], [-6.5, -46, 0], [-6.5, -62, 0]]);

  return group;
}

/**
 * Panels, welds, wear and grime, plus the markings a base accumulates: section numbers on the truss,
 * a hazard band at each berth's cradle face, and the scorch on the first campaign's ring drum.
 *
 * The atlas is planar and 512 squared over the whole 307 m footprint, so a texel is about 1.28 m:
 * panel lines land at 10 m and the stencils are set at 0.009 of a tile, which is a 14 m mask. That
 * is deliberately huge — anything under three texels is a smudge, and a section number on a 200 m
 * ring has to read against the hull's panel lines rather than merely be present.
 *
 * The fractions below are the model coordinates of the features, divided by that 307 m span: the
 * box is x -153.5..153.5, y -117..117, z -89..95.9, so `across` is (x + 153.5)/307 in the x tiles
 * and (z + 89)/307 in the z one, and `down` is (117 - y)/307 or (95.9 - z)/307. A berth face is
 * 21 m, i.e. 0.07 of a tile, which is the width the hazard bands are drawn at.
 */
export const maps: Maps = (() => {
  const kit = standard_maps({ seed: 73, panel: 0.036, rivets: 0.015, seams: 3, wear: 0.32, grime: 0.28 });
  return {
    normal: kit.normal,
    roughness: kit.roughness,
    albedo: (ctx, size) => {
      kit.albedo?.(ctx, size);
      each_tile(ctx, size, (tile) => {
        // Cell 0 is the dorsal/ventral projection. The older drum sits on the ring at azimuth 150,
        // 104 m out: x = -90, y = 52, so ((-90 + 153.5)/307, (117 - 52)/307) = (0.207, 0.212).
        if (tile.cell === 0) {
          scorch(tile, { x: 0.207, y: 0.212, radius: 0.09, seed: 61 });
          stencil_text(tile, 'SEC 07', { x: 0.55, y: 0.30, scale: 0.009 });
          stencil_text(tile, 'HOLD 3', { x: 0.22, y: 0.55, scale: 0.009 });
        }
        // Cell 1 is the ±x projection: the two spine berths at z = ±52, y = 0, so across is
        // (52 + 89)/307 = 0.459 and (-52 + 89)/307 = 0.121, both down 117/307 = 0.381.
        if (tile.cell === 1) {
          for (const x of [0.121, 0.459]) {
            hazard_stripes(tile, { x: x - 0.037, y: 0.344, w: 0.074, h: 0.074 }, { pitch: 0.016 });
          }
          stencil_text(tile, 'DOCK 1', { x: 0.125, y: 0.62, scale: 0.009 });
          stencil_text(tile, 'DOCK 2', { x: 0.463, y: 0.62, scale: 0.009 });
        }
        // Cell 2 is the ±y projection: the two hub berths at x = 0 (0.500 across) and z = 0, which
        // is 95.9/307 = 0.312 down.
        if (tile.cell === 2) {
          hazard_stripes(tile, { x: 0.463, y: 0.275, w: 0.074, h: 0.074 }, { pitch: 0.016 });
          stencil_text(tile, 'L4', { x: 0.14, y: 0.20, scale: 0.009 });
          stencil_text(tile, 'TESSERA', { x: 0.58, y: 0.74, scale: 0.009 });
        }
      });
    },
  };
})();

export const meta = { name: 'station_lagrange', scale: 1, collider: 'auto', untextured: ['glass', 'solarCell'] };
