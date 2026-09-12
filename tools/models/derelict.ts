// Wrecked freighter (PLAN-03 §7.1, re-authored at F8). The plan-01 silhouette is kept — the bow
// block, the listing aft section, the two outboard plates and the bell all stay on their marks,
// because the wreck's world placement is tuned to this box — but none of it is a clean box any
// more. The skin is extruded from irregular outlines and then curled, the midships' dorsal plating
// is gone with the frame left standing, the spine is snapped, the bell is torn off its mount, and
// debris is still drifting off the wound. Scorch and rust live in the albedo, torn edges in the
// normal map, so the damage reads from any angle the atlas can see.
import * as THREE from 'three';
import {
  armor, black, copper, dark, frame, glass, lightArmor, metal, ochre, teal, box, bevelled, each_tile, hull, lathe, noise, noise_wash, rng,
  scorch, standard_maps, strut, torus, tube, type Maps,
} from './prims';

/** The wreck's own two materials: bare rusted plate and plate cooked black. Both are mapped, so the
 *  grime and the damage cross from the palette's steel onto the wreck's own skin. */
const rust = new THREE.MeshStandardMaterial({ color: '#5f4132', roughness: 0.92, metalness: 0.45 });
const scorched = new THREE.MeshStandardMaterial({ color: '#181512', roughness: 0.94, metalness: 0.4 });
export const warmLampMaterial = new THREE.MeshBasicMaterial({ color: '#efb879' });
for (const material of [rust, scorched, warmLampMaterial]) material.userData.shared = true;

/**
 * A torn plate: a rectangle with bites ripped out of its edges, extruded and then warped so the skin
 * curls away from whatever it was riveted to. The four corners are kept (inset a little, never
 * removed), which is what keeps the plate's footprint — and with it the wreck's box — on the mark
 * however the seed shapes the torn edge.
 */
function torn_plate(root: THREE.Object3D, material: THREE.Material, size: number[], pos: number[], rot: number[], seed: number, curl = 1.0): THREE.Mesh {
  const rand = rng(seed);
  const sx = size[0] / 2, sy = size[1] / 2;
  const outline: number[][] = [];
  const corners = [[1, 1], [-1, 1], [-1, -1], [1, -1]];
  for (let i = 0; i < 4; i++) {
    const [cx, cy] = corners[i];
    const [nx, ny] = corners[(i + 1) % 4];
    const inset = 0.9 + rand() * 0.1;
    outline.push([cx * sx * inset, cy * sy * inset]);
    for (let b = 0, bites = 1 + Math.floor(rand() * 2); b < bites; b++) {
      const along = (b + 1) / (bites + 1);
      const bite = 0.55 + rand() * 0.3;                 // how far the tear eats into the plate
      outline.push([(cx + (nx - cx) * along) * sx * bite, (cy + (ny - cy) * along) * sy * bite]);
    }
  }
  const shape = new THREE.Shape(outline.map(([x, y]) => new THREE.Vector2(x, y)));
  const geometry = new THREE.ExtrudeGeometry(shape, { depth: size[2], bevelEnabled: true, bevelSize: 0.45, bevelThickness: 0.4, bevelSegments: 1, steps: 1 });
  // Curl: lift z with the square of the distance from the plate's centre, plus a little noise so the
  // bend is not a clean dome. Face normals afterwards — a torn plate is faceted, not smooth.
  const position = geometry.attributes.position;
  for (let i = 0; i < position.count; i++) {
    const x = position.getX(i), y = position.getY(i);
    const u = x / sx, v = y / sy;
    position.setZ(i, position.getZ(i) + (u * u * 0.65 + v * v * 0.35) * curl + noise(x * 0.09 + seed * 0.5, y * 0.09, seed) * curl * 0.8);
  }
  geometry.computeVertexNormals();
  const mesh = new THREE.Mesh(geometry, material);
  mesh.position.set(pos[0], pos[1], pos[2]);
  mesh.rotation.set(rot[0], rot[1], rot[2]);
  mesh.castShadow = true;
  mesh.receiveShadow = true;
  root.add(mesh);
  return mesh;
}

export function build(): THREE.Object3D {
  const group = new THREE.Group(); group.name = 'derelict';

  // Bow: the plan-01 block, upper deck intact, with a plate peeled off the starboard shoulder.
  hull(group, 28, 34, 14, dark, 0, 51, 1);
  hull(group, 20, 30, 11, armor, 0, 52, 8.5);
  torn_plate(group, armor, [14, 18, 1.2], [12, 52, 6], [0, 0, 0.35], 211, 1.1);
  // Identity bands on the intact bow deck, teal aft of ochre — the marking the sheet draws.
  box(group, teal, [18, 3.0, 0.3], [0, 44, 14.1]);
  box(group, ochre, [18, 1.2, 0.3], [0, 41.4, 14.1]);
  // Bow windows: a lit strip across the upper deck front, dark where the panes are blown.
  box(group, glass, [10, 1.2, 0.4], [-3, 66.5, 10.5], 0.0);
  box(group, black, [3.5, 1.2, 0.45], [4, 66.5, 10.5], 0.0);
  // The dorsal pylon is snapped off at its root: the stump is left, and the struts that guyed it.
  box(group, frame, [3.6, 9, 2.2], [0, 62, 12]);
  for (const side of [-1, 1]) strut(group, dark, [0, 64, 12], [side * 4.2, 69, 13.6], 0.32, 6);

  // Midships: the lower hull survived, the dorsal plating did not. Two torn plates are all that is
  // left of the deck, curled off the edges they tore along, and the frame beneath is exposed.
  hull(group, 26, 40, 12, metal, 0, 12, -1);
  torn_plate(group, lightArmor, [16, 15, 1.1], [2, 21, 6.5], [0, 0, -0.2], 307, 1.3);
  torn_plate(group, armor, [13, 13, 1.0], [-3, 5, 6.2], [0, 0, 0.5], 401, 1.5);
  for (const x of [-9, -4.5, 4.5, 9]) strut(group, frame, [x, -8, 3], [x * 0.8, 32, 3], 0.55, 5);
  // Deck beams across the wound with the plating gone between them, and power cables hanging
  // off the torn edges into the opening — interior you fly past, not a texture.
  for (const y of [22, 10, -2]) {
    box(group, dark, [19, 1.0, 1.0], [0, y, 4.5]);
    strut(group, copper, [-8, y, 4.0], [-6, y - 4, -1.0], 0.12, 4);
    strut(group, copper, [8, y, 4.0], [6.5, y - 5, -1.5], 0.12, 4);
  }
  for (const y of [27, 15, 3, -9]) {
    strut(group, frame, [-11, y, -3], [-8, y, 5.5], 0.5, 6);
    strut(group, frame, [11, y, -3], [8, y, 5.5], 0.5, 6);
    strut(group, frame, [-8, y, 5.5], [8, y, 5.5], 0.45, 6);
  }
  for (const side of [-1, 1]) strut(group, frame, [side * 9, -8, 5], [side * 4.5, 12, 5], 0.4, 5);

  // The keel is snapped between the bow and the sagging aft section: two stubs with the middle gone.
  bevelled(group, frame, [7, 30, 5], [0, 26, -2], [0, 0, 0], { radius: 0.6, segments: 1 });
  box(group, frame, [10, 24, 6], [-4, -16, -1], 0.12);
  strut(group, dark, [0, 12, -3], [-3, -6, -4], 0.7, 6);

  // Aft: the stern section, listing to port and burnt along its spine. Its deck run is broken up
  // into beams with the plating gone between them, which is how a gutted deck looks from above.
  const aft = hull(group, 26, 62, 12, metal, -7, -50, 1);
  aft.rotation.z = 0.16;
  const stern = hull(group, 16, 26, 8, rust, -10, -72, 6);
  stern.rotation.z = 0.16;
  torn_plate(group, rust, [15, 20, 1.0], [-16, -52, 6], [0, 0, 0.42], 509, 1.1);
  torn_plate(group, dark, [12, 16, 0.9], [4, -66, 7], [0, 0, -0.3], 601, 1.2);
  torn_plate(group, scorched, [11, 15, 1.0], [-5, -79, 5], [0, 0, 0.22], 613, 1.0);
  for (const [y, width, material] of [[-12, 21, metal], [-24, 19, black], [-37, 20, metal], [-52, 17, dark], [-64, 14, rust]] as [number, number, THREE.Material][]) {
    box(group, material, [width, 1.2, 2.2], [0, y, 7.6]);
  }
  // The second drive, sheared: a torn gimbal ring and mount stubs reaching for a bell that is
  // no longer there. One drive left, one gone — the sheet's own asymmetry.
  torus(group, dark, 4.6, 0.7, [7, -76, 0], [0.2, 0, 0.1], 5, 14);
  for (const side of [-1, 1]) strut(group, dark, [7 + side * 4, -73, side * 3], [7 + side * 7, -79, side * 4], 0.55, 6);
  box(group, frame, [3.0, 4.0, 2.0], [7, -71, 0]);
  // Hanging radiator: torn off its root hinge, held by two cables, dangling below the aft
  // section with its coolant pipes exposed.
  const rad = new THREE.Group();
  rad.position.set(10, -48, -8);
  rad.rotation.set(0.5, 0, 0.9);
  group.add(rad);
  box(rad, dark, [11, 0.5, 7], [0, 0, 0]);
  box(rad, frame, [11.5, 0.8, 0.6], [0, 0, 3.4]);
  for (const x of [-3, 3]) tube(rad, copper, 0.12, 0.12, 6.4, [x, 0.4, 0], 6);
  strut(group, dark, [6, -42, -2], [8, -48, -7], 0.09, 4);
  strut(group, dark, [13, -43, -3], [12, -48, -7], 0.09, 4);

  // The drive bell, sheared off its mount: the lathe mouth is recessed irregularly along the rim, so
  // the bell was ripped rather than unbolted, and only the stubs of its mount still touch the hull.
  const bell = lathe(group, metal, [[4.2, 0], [5.4, 2.0], [8.6, 6.4], [11.5, 10.4]], 20, [-9, -79.4, 1], [Math.PI, 0, 0.18]);
  const bellPosition = bell.geometry.attributes.position;
  for (let i = 0; i < bellPosition.count; i++) {
    const x = bellPosition.getX(i), y = bellPosition.getY(i), z = bellPosition.getZ(i);
    const tear = Math.max(0, (y - 5) / 5.4) ** 2 * noise(Math.atan2(z, x) * 2.2 + 3.1, y * 0.4, 71) * 3.4;
    bellPosition.setY(i, y - tear);
  }
  bell.geometry.computeVertexNormals();
  tube(group, dark, 4.2, 4.2, 6.0, [-8.7, -76.5, 1], 12, [0, 0, 0.18]);
  for (const side of [-1, 1]) strut(group, dark, [-9 + side * 5, -77, 1 + side * 4], [-9 + side * 9, -84, 1 + side * 5], 0.6, 6);

  // The two plan-01 outboard plates are this model's x extremes, so they stay on their marks; they
  // are torn and curled now, which is what a plate that came off in a blast looks like.
  torn_plate(group, lightArmor, [16, 24, 1.5], [26, -34, 3], [0, 0, 0.5], 719, 0.7);
  torn_plate(group, armor, [14, 20, 1.4], [-32, -16, 4], [0, 0, -0.8], 821, 0.7);

  // Debris still drifting off the wound. The scatter is seeded, so the field is identical on every
  // export, and it is held inside the wreck's own envelope so placement stays as it was.
  const rand = rng(1301);
  for (let i = 0; i < 9; i++) {
    const chunk = new THREE.Mesh(new THREE.IcosahedronGeometry(0.7 + rand() * 1.5, 0), i % 3 ? metal : rust);
    chunk.position.set(-26 + rand() * 46, -70 + rand() * 52, -6 + rand() * 16);
    chunk.scale.set(1, 0.5 + rand() * 0.6, 0.5 + rand() * 0.8);
    chunk.rotation.set(rand() * 3, rand() * 3, rand() * 3);
    chunk.castShadow = true;
    group.add(chunk);
  }

  // One lamp still burning on a broken mast: the only light the wreck has left.
  strut(group, dark, [-11, -62, 6], [-11, -62, 8.4], 0.35, 6);
  const lamp = new THREE.Mesh(new THREE.SphereGeometry(1.4, 8, 6), warmLampMaterial);
  lamp.position.set(-11, -62, 9);
  group.add(lamp);
  return group;
}

/** Panels, welds and wear, then the wreck's own damage: torn edges for the normal map, rust blooms
 *  and soot for the albedo. The tiles are coarse because the model is 160 m long — a 512 map gives
 *  one hull section per panel line, which is the scale the wreck is wrecked at. */
export const maps: Maps = (() => {
  const kit = standard_maps({ seed: 71, panel: 0.05, rivets: 0.025, seams: 2, wear: 0.36, grime: 0.42, damage: 0.5, scorch: 3 });
  return {
    normal: kit.normal,
    roughness: kit.roughness,
    albedo: (ctx, size) => {
      kit.albedo?.(ctx, size);
      each_tile(ctx, size, (tile) => {
        noise_wash(tile, { color: '#7a4a2c', alpha: 0.38, scale: 0.05, octaves: 3, seed: 83 });
        noise_wash(tile, { color: '#241f1b', alpha: 0.4, scale: 0.02, octaves: 2, seed: 97, bias: 0.52 });
        for (const spot of [[0.3, 0.34, 0.16, 5], [0.68, 0.6, 0.13, 11], [0.44, 0.78, 0.2, 17]]) {
          scorch(tile, { x: spot[0], y: spot[1], radius: spot[2], seed: spot[3], alpha: 0.75 });
        }
      });
    },
  };
})();

export const meta = { name: 'derelict', scale: 1, collider: 'auto' };
