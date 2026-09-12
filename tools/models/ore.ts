// Ore chunk (PLAN-03 §7.1, re-authored at F8): a displaced rock instead of an icosahedron. The
// shell is a sphere whose radius is quantised noise — terraces and facets under smooth shading, so
// the silhouette is irregular and the surface still lights continuously — the crust is a scatter of
// jagged patches, the crystals break out of the shallowest dips, and the vein is a low-poly shell
// that shows through wherever the rock's terraces fall below it. The ~14 m envelope is the plan-01
// chunk's: the scene scatters ore at a tuned size.
import * as THREE from 'three';
import {
  ceramic, each_tile, lathe, noise, noise_wash, oreShell, oreVein, rng, standard_maps, type Maps,
} from './prims';

/** The mineral crust: paler than the shell so the patches read as deposits rather than more rock. */
const crust = new THREE.MeshStandardMaterial({ color: '#b4aa93', roughness: 0.9, metalness: 0.1 });
crust.name = 'crust';
crust.userData.shared = true;

const UP = new THREE.Vector3(0, 1, 0);

/**
 * Radius of the shell at a unit direction. Both octaves are sampled from the direction itself, so
 * the sphere's seam and its poles get the same value on either side of the vertex split and no
 * crack opens; quantising the result into 0.17 m terraces is what gives a smooth-shaded mesh facets.
 */
function shell_radius(direction: THREE.Vector3): number {
  const macro = noise(direction.x * 1.6 + 4.3, direction.y * 1.6 + direction.z * 1.1 + 9.1, 13);
  const fine = noise(direction.x * 3.7 + 21.4, direction.z * 3.7 + direction.y * 2.1, 31);
  return 6.3 + Math.round((macro * 0.72 + fine * 0.28) * 4) * 0.26;
}

/** A point on the shell along `direction`, pushed out by `offset` and flattened in z with it. */
function surface(direction: THREE.Vector3, offset: number): THREE.Vector3 {
  const radius = shell_radius(direction) + offset;
  return new THREE.Vector3(direction.x * radius, direction.y * radius, direction.z * radius * 0.92);
}

/** A uniformly scattered direction off the module's seeded stream: same rock on every export. */
function scatter(rand: () => number): THREE.Vector3 {
  const theta = rand() * Math.PI * 2;
  const phi = Math.acos(1 - 2 * rand());
  return new THREE.Vector3(Math.sin(phi) * Math.cos(theta), Math.cos(phi), Math.sin(phi) * Math.sin(theta));
}

export function build(): THREE.Object3D {
  const group = new THREE.Group(); group.name = 'ore';
  // Shell: 18 x 12 segments is as coarse as the terraces can be and still describe a rock; the
  // smooth normals are what keep the terraces reading as facets in the surface rather than creases.
  const geometry = new THREE.SphereGeometry(1, 18, 12);
  const position = geometry.attributes.position;
  const direction = new THREE.Vector3();
  for (let i = 0; i < position.count; i++) {
    direction.fromBufferAttribute(position, i).normalize();
    const radius = shell_radius(direction);
    position.setXYZ(i, direction.x * radius, direction.y * radius, direction.z * radius * 0.92);
  }
  geometry.computeVertexNormals();
  const shell = new THREE.Mesh(geometry, oreShell);
  shell.castShadow = true;
  shell.receiveShadow = true;
  group.add(shell);

  // Crust and crystals share one scatter: the deposits settle where they land, and the crystals
  // break out of the six shallowest of those places, where the crust is thin. Icosahedra rather
  // than spheres — 20 triangles each, and the per-face normals they get after displacement are what
  // makes a patch read as jagged mineral instead of a blister.
  const rand = rng(1013);
  const places: THREE.Vector3[] = [];
  for (let i = 0; i < 9; i++) places.push(scatter(rand));
  for (const place of places) {
    const patch = new THREE.Mesh(new THREE.IcosahedronGeometry(1, 0), crust);
    patch.position.copy(surface(place, -0.3));
    patch.scale.set(1.3 + rand() * 0.8, 0.5, 1.1 + rand() * 0.7);
    patch.quaternion.setFromUnitVectors(UP, place);
    patch.geometry.computeVertexNormals();
    patch.castShadow = true;
    group.add(patch);
  }
  places.sort((a, b) => shell_radius(a) - shell_radius(b));
  for (let i = 0; i < 6; i++) {
    const place = places[i];
    // One lathe per crystal: a hexagonal shaft with a point, so a four-point profile still reads as
    // a crystal. The base is buried, which is why the profile runs below the surface it sits on.
    const crystal = lathe(group, ceramic, [[0, 1.5], [0.36, 1.1], [0.48, 0.3], [0.58, -0.8]], 6,
      surface(place, -0.9).toArray());
    crystal.quaternion.setFromUnitVectors(UP, place);
  }
  // Vein: radius 6.58 sits between the terraces, so the glow shows only in the low spots. It keeps
  // the `oreVein` basic material, which is what makes it read as ore and not as paint, and staying
  // under the crest keeps it out of the silhouette.
  const vein = new THREE.IcosahedronGeometry(1, 1);
  const veinPosition = vein.attributes.position;
  for (let i = 0; i < veinPosition.count; i++) {
    direction.fromBufferAttribute(veinPosition, i).normalize();
    const radius = 6.58 + noise(direction.x * 3.1 + 2.7, direction.y * 3.1 + direction.z * 1.7, 47) * 0.14;
    veinPosition.setXYZ(i, direction.x * radius, direction.y * radius, direction.z * radius * 0.92);
  }
  group.add(new THREE.Mesh(vein, oreVein));
  return group;
}

/** Rock has no panels or rivets: the kit is used for its grime, wear and the mineral washes. */
export const maps: Maps = (() => {
  const kit = standard_maps({ seed: 59, panel: 0, rivets: 0, seams: 0, wear: 0.3, grime: 0.42 });
  return {
    normal: kit.normal,
    roughness: kit.roughness,
    albedo: (ctx, size) => {
      kit.albedo?.(ctx, size);
      each_tile(ctx, size, (tile) => {
        // A pale mineral wash and a dark one, so the terraces do not read as one flat colour.
        noise_wash(tile, { color: '#8d8574', alpha: 0.3, scale: 0.06, octaves: 3, seed: 77 });
        noise_wash(tile, { color: '#3f3a33', alpha: 0.3, scale: 0.025, octaves: 3, seed: 91, bias: 0.5 });
      });
    },
  };
})();

export const meta = { name: 'ore', scale: 1, collider: 'auto' };
