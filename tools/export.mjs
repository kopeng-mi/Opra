#!/usr/bin/env bun
/**
 * Opra model export: a `tools/models/*.ts` three.js source becomes
 *   assets/<name>.glb      binary glTF (one merged mesh per material, plus the effect meshes)
 *   assets/<name>.json     sidecar the loader reads (scale, AABB, compound collider, ports, effects,
 *                          hardpoints, counts)
 *   assets/models.json     the manifest of everything exported so far
 *
 *   bun tools/export.mjs tools/models/kestrel.ts [assetsDir]
 *   bun tools/export.mjs --all [assetsDir]      (assetsDir defaults to the repo's assets/)
 *
 * The ported geometry is authored nose +Y, dorsal +Z, starboard +X, in metres, and the exporter
 * writes local coordinates unchanged: glTF's Y-up convention does not touch node transforms, so
 * what three.js holds is what cgltf reads (PLAN.md P3.1).
 *
 * Static meshes are merged per material exactly the way fleet.ts:batchPlates did at run time, now
 * at build time. Meshes marked `userData.effect` (flames, RCS jets, the beacon halo) are exported
 * untouched and hidden; glTF keeps `extras.effect` on the node for the loader's additive pass.
 */
import { mkdir, readFile, readdir, writeFile } from 'node:fs/promises';

async function writeWithRetry(file, data, retries = 5) {
  for (let attempt = 0; attempt < retries; ++attempt) {
    try {
      await writeFile(file, data);
      return;
    } catch (err) {
      if (attempt === retries - 1) throw err;
      await new Promise((r) => setTimeout(r, 100 * (attempt + 1)));
    }
  }
}
import path from 'node:path';
import { pathToFileURL } from 'node:url';

import * as THREE from 'three';
import { GLTFExporter } from 'three/addons/exporters/GLTFExporter.js';
import { mergeGeometries } from 'three/addons/utils/BufferGeometryUtils.js';

import { RasterContext, atlas_cell, createRaster, encode_png } from './models/prims.ts';

const MODELS_DIR = path.join(import.meta.dir, 'models');
// Manifest entries and the default output directory are repo-relative: the loader resolves them from
// the executable's base directory, so they must not depend on where the exporter was invoked.
const REPO_ROOT = path.resolve(import.meta.dir, '..');
const DEFAULT_ASSETS_DIR = path.join(REPO_ROOT, 'assets');

// three's GLTFExporter assembles the GLB through FileReader, which bun does not ship. This is the
// whole surface the binary path uses: readAsArrayBuffer, then an `onloadend` callback.
if (typeof globalThis.FileReader === 'undefined') {
  globalThis.FileReader = class FileReader {
    constructor() { this.result = null; this.onloadend = null; }
    readAsArrayBuffer(blob) {
      blob.arrayBuffer().then((buffer) => { this.result = buffer; this.onloadend?.(); });
    }
    readAsDataURL(blob) {
      blob.arrayBuffer().then((buffer) => {
        this.result = `data:${blob.type || 'application/octet-stream'};base64,${Buffer.from(buffer).toString('base64')}`;
        this.onloadend?.();
      });
    }
  };
}

// The procedural maps are drawn into a Uint8ClampedArray by `models/prims.ts`, but three's
// GLTFExporter only knows how to bake an image through a canvas: `getCanvas()`, `getContext('2d')`,
// `putImageData`, and `convertToBlob`/`toBlob` for the PNG. Bun ships no canvas, so the exporter
// supplies that surface here on top of the same raster. It is deliberately small: the bytes of a
// map are written by the map function, and this only re-encodes them, unmangled, for glTF.
class OffscreenCanvasShim {
  #height = 1;
  #context = null;
  data = new Uint8ClampedArray(4);

  constructor(width = 1, height = 1) {
    this.width = width;
    this.height = height;
  }

  get height() { return this.#height; }
  set height(value) { this.#resize(this.width, value); }
  get width() { return this.data.length / (4 * this.#height); }
  set width(value) { this.#resize(value, this.#height); }

  #resize(width, height) {
    const w = Math.max(1, Math.floor(width)), h = Math.max(1, Math.floor(height));
    this.#height = h;
    this.data = new Uint8ClampedArray(w * h * 4);      // a resized canvas is cleared, as in the browser
    this.#context = null;
  }

  getContext(type) {
    if (type !== '2d') return null;
    if (!this.#context) this.#context = new RasterContext(this.data, this.width, this.#height);
    return this.#context;
  }

  async convertToBlob({ type = 'image/png' } = {}) {            // the browser API is a promise
    return new Blob([encode_png(this.data, this.width, this.#height)], { type });
  }
}

class ImageDataShim {
  constructor(data, width, height) {
    this.data = data; this.width = width; this.height = height;
  }
}

globalThis.OffscreenCanvas = OffscreenCanvasShim;
globalThis.ImageData = ImageDataShim;

const round = (value, places = 4) => {
  const factor = 10 ** places;
  return Math.round(value * factor) / factor;
};
// A broad-phase radius may never round down: the sidecar carries 4 places, and the number has to
// stay >= the farthest vertex it covers.
const roundUp = (value, places = 4) => {
  const factor = 10 ** places;
  return Math.ceil(value * factor) / factor;
};
const roundVec = (vector, places = 4) => vector.toArray().map((value) => round(value, places));
const describeMaterial = (material) => {
  const color = material.color ? `#${material.color.getHexString()}` : '(no colour)';
  return `${material.type} ${color}`;
};

/** Headless: a texture backed by a canvas cannot be exported. three's CanvasTexture is the only one. */
function canvasTexture(material) {
  for (const value of Object.values(material)) {
    if (!value || value.isTexture !== true) continue;
    const image = value.image ?? value.source?.data;
    if (typeof image?.getContext === 'function') return value;
  }
  return null;
}

/**
 * Merges every opaque, non-effect mesh into one mesh per material, baking world transforms into the
 * vertices. Meshes glTF cannot carry (a canvas map) are dropped; meshes that simply cannot join a
 * merge (transparent, multi-material, merge failure) stay in the tree as authored so no geometry is
 * lost. Both are reported.
 */
function mergeStatic(root) {
  const groups = new Map();
  const skipped = [];
  const dropped = [];
  const unmerged = [];
  root.traverse((node) => {
    if (!node.isMesh) return;
    const name = node.name || '(unnamed)';
    const materials = Array.isArray(node.material) ? node.material : [node.material];
    const canvas = materials.find((material) => material && canvasTexture(material));
    if (canvas) { skipped.push({ name, material: canvas, reason: 'canvas map' }); dropped.push(node); return; }
    if (node.userData.effect) return;                          // flames, jets, halo: exported as authored
    if (Array.isArray(node.material)) return unmerged.push({ name, material: node.material[0], reason: 'multi-material' });
    if (node.children.length) return unmerged.push({ name, material: node.material, reason: 'has children' });
    if (node.material.transparent) return unmerged.push({ name, material: node.material, reason: 'transparent' });
    const meshes = groups.get(node.material) ?? [];
    meshes.push(node);
    groups.set(node.material, meshes);
  });

  const merged = [];
  // The merged mesh is re-parented under `root`, so its vertices are baked relative to the root, not
  // to the world: baking `matrixWorld` outright applies the root's own transform a second time
  // (cargo authors a rotated group, and its AABB came out 25% too large for it).
  const toRoot = new THREE.Matrix4().copy(root.matrixWorld).invert();
  const bake = new THREE.Matrix4();
  for (const [material, meshes] of groups) {
    const geometries = meshes.map((mesh) => {
      const geometry = mesh.geometry.index ? mesh.geometry.toNonIndexed() : mesh.geometry.clone();
      return geometry.applyMatrix4(bake.multiplyMatrices(toRoot, mesh.matrixWorld));
    });
    const geometry = mergeGeometries(geometries, false);
    geometries.forEach((entry) => entry.dispose());
    if (!geometry) {
      console.warn(`  ! merge failed for ${describeMaterial(material)}: ${meshes.length} mesh(es) left unmerged`);
      for (const mesh of meshes) unmerged.push({ name: mesh.name || '(unnamed)', material, reason: 'merge failed' });
      continue;
    }
    for (const mesh of meshes) { mesh.geometry.dispose(); mesh.removeFromParent(); }
    const mesh = new THREE.Mesh(geometry, material);
    mesh.name = `merged-${material.color ? material.color.getHexString() : 'flat'}`;
    mesh.castShadow = true;
    mesh.receiveShadow = true;
    root.add(mesh);
    merged.push(mesh);
  }

  // Dropped meshes must leave the tree, or the exporter would still walk them (and choke on the map).
  for (const node of dropped) {
    node.geometry.dispose();
    node.removeFromParent();
  }

  return { merged, skipped, unmerged };
}

/**
 * s3.2/J5's enforcement, applied where it can be mechanical: a part thinner than the 1.5 m floor
 * in any world axis is stretched on that axis to the floor, about its own centre. A 0.1 m plate
 * becomes a 1.5 m rib; a 0.5 m pipe becomes a 1.5 m column - the plan's own remedy, since what
 * survives is what a player can see. Degenerate parts (a zero AABB) draw nothing, so they are left
 * for the census to skip rather than grown by infinity.
 */
function enforce_min_feature(root, floor_m) {
  const grown = [];
  // A rotated part projects a stretch at an angle, so one pass can under-shoot; iterate until a
  // full sweep grows nothing. Node transforms never change here, so matrixWorld stays valid.
  for (let sweep = 0; sweep < 16; ++sweep) {
    let changed = false;
    root.traverse((node) => {
      if (!node.isMesh || node.userData.effect) return;
      const geometry = node.geometry;
      if (!geometry.boundingBox) geometry.computeBoundingBox();
      const box = geometry.boundingBox.clone().applyMatrix4(node.matrixWorld);
      const sides = [box.max.x - box.min.x, box.max.y - box.min.y, box.max.z - box.min.z];
      const smallest = Math.min(...sides);
      if (!(smallest > 1.0e-4) || smallest >= floor_m) return;
      const centre = box.getCenter(new THREE.Vector3());
      const axis = sides.indexOf(smallest);
      // Squared: a diagonal thin direction projects the stretch at cos(theta), so a linear factor
      // creeps and stalls; the square converges in two or three sweeps and the per-part early-out
      // (smallest >= floor) stops it there.
      const scale = (floor_m / smallest) * (floor_m / smallest);
      // The thin axis was measured in world space; the geometry is stretched in the node's own
      // frame, so the axis direction comes down through the node's inverse rotation.
      const world_axis = new THREE.Vector3(axis === 0 ? 1 : 0, axis === 1 ? 1 : 0, axis === 2 ? 1 : 0);
      const world_quaternion = new THREE.Quaternion();
      node.getWorldQuaternion(world_quaternion);
      const local_axis = world_axis.applyQuaternion(world_quaternion.clone().invert());
      const local_component = Math.abs(local_axis.x) > Math.abs(local_axis.y)
          ? (Math.abs(local_axis.x) > Math.abs(local_axis.z) ? 0 : 2)
          : (Math.abs(local_axis.y) > Math.abs(local_axis.z) ? 1 : 2);
      const local_centre = node.worldToLocal(centre.clone());
      geometry.applyMatrix4(
          new THREE.Matrix4().makeTranslation(-local_centre.x, -local_centre.y, -local_centre.z));
      geometry.scale(local_component === 0 ? scale : 1.0, local_component === 1 ? scale : 1.0,
                     local_component === 2 ? scale : 1.0);
      geometry.applyMatrix4(
          new THREE.Matrix4().makeTranslation(local_centre.x, local_centre.y, local_centre.z));
      // The cached box is now a lie; the next sweep and the census read it after this pass.
      geometry.computeBoundingBox();
      geometry.computeBoundingSphere();
      changed = true;
      if (sweep === 0) grown.push(node.name || 'part');
    });
    if (!changed) break;
  }
  return grown;
}

/** The pre-merge part census: each static part's name and smallest world-space dimension. Parts
 *  with a zero AABB draw nothing - they are not features and are not counted. */
function smallest_parts(root) {
  const parts = [];
  root.traverse((node) => {
    if (!node.isMesh || node.userData.effect) return;
    const geometry = node.geometry;
    if (!geometry.boundingBox) geometry.computeBoundingBox();
    const box = geometry.boundingBox.clone().applyMatrix4(node.matrixWorld);
    const side = Math.min(box.max.x - box.min.x, box.max.y - box.min.y, box.max.z - box.min.z);
    if (side > 1.0e-4) parts.push({ name: node.name || `part.${parts.length}`, side });
  });
  parts.sort((a, b) => a.side - b.side);
  return parts;
}

/** Mesh count, vertex/triangle totals and the world-space AABB of the static (non-effect) hull. */
function measure(root) {
  const staticBox = new THREE.Box3();
  const allBox = new THREE.Box3();
  let meshes = 0, vertices = 0, triangles = 0;
  root.traverse((node) => {
    if (!node.isMesh) return;
    meshes += 1;
    const geometry = node.geometry;
    if (!geometry.boundingBox) geometry.computeBoundingBox();
    const box = geometry.boundingBox.clone().applyMatrix4(node.matrixWorld);
    allBox.union(box);
    if (!node.userData.effect) staticBox.union(box);
    const count = geometry.attributes.position.count;
    vertices += count;
    triangles += (geometry.index ? geometry.index.count : count) / 3;
  });
  return { staticBox, allBox, meshes, vertices, triangles };
}

/** Every effect mesh with its model-space position: the loader drives these from the simulation. */
function effects(root) {
  const list = [];
  root.traverse((node) => {
    if (!node.isMesh || !node.userData.effect) return;
    const position = new THREE.Vector3();
    node.getWorldPosition(position);
    list.push({ node: node.name || '(unnamed)', pos: roundVec(position) });
  });
  return list;
}

/** Effect meshes whose material glTF cannot carry (the exhaust plume is a ShaderMaterial). */
function shaderEffects(root) {
  let count = 0;
  root.traverse((node) => {
    if (node.isMesh && node.userData.effect && node.material?.isShaderMaterial) count += 1;
  });
  return count;
}

/**
 * F8 procedural maps: a module's `maps` becomes one `DataTexture` per slot, drawn by the module
 * into a raster the exporter re-encodes as PNG inside the GLB. Flip is off because glTF's v=0 is
 * the image's first row and the raster's y=0 is the first row: nothing has to be flipped anywhere,
 * including in the mip chain.
 */
function bakeMaps(name, maps, size = 512) {
  const textures = {};
  const seeds = { normal: [128, 128, 255], roughness: [0, 236, 255], albedo: [242, 240, 234] };
  for (const slot of ['normal', 'roughness', 'albedo']) {
    const draw = maps[slot];
    if (typeof draw !== 'function') continue;
    const { data, ctx } = createRaster(size, seeds[slot]);
    draw(ctx, size);
    const texture = new THREE.DataTexture(data, size, size, THREE.RGBAFormat);
    texture.name = `${name}_${slot}`;
    texture.flipY = false;
    texture.colorSpace = slot === 'albedo' ? THREE.SRGBColorSpace : THREE.NoColorSpace;
    // DataTexture defaults to NEAREST with no mip chain, which is not what a hull wants.
    texture.magFilter = THREE.LinearFilter;
    texture.minFilter = THREE.LinearMipmapLinearFilter;
    texture.generateMipmaps = true;
    texture.needsUpdate = true;
    textures[slot] = texture;
  }
  return textures;
}

/**
 * Binds those textures to the material slots they belong to. The palette materials are shared
 * between every module, so each one is cloned first: a clone carries the maps and the original
 * stays clean for the next asset. `meta.untextured` names materials the maps must leave alone
 * (glass, solar cells, lamp faces) - a stencil on a window is a mistake, not detail.
 */
function bindMaps(root, textures, untextured) {
  const clones = new Map();
  let bound = 0;
  root.traverse((node) => {
    if (!node.isMesh || node.userData.effect || Array.isArray(node.material)) return;
    const material = node.material;
    if (!material?.isMeshStandardMaterial || untextured.has(material.name)) return;
    let mapped = clones.get(material);
    if (!mapped) {
      mapped = material.clone();
      mapped.name = `${material.name || 'material'}-mapped`;
      if (textures.albedo) mapped.map = textures.albedo;
      if (textures.normal) mapped.normalMap = textures.normal;
      if (textures.roughness) {
        // One texture in both slots: glTF samples G for roughness and B for metalness from the same
        // image, and three's exporter then skips its channel-merge blit entirely (it composites the
        // two maps into a fresh canvas whenever they differ). B is 255 in the map, so metalness
        // stays whatever the material's factor says and only the roughness varies.
        mapped.roughnessMap = textures.roughness;
        mapped.metalnessMap = textures.roughness;
      }
      clones.set(material, mapped);
    }
    node.material = mapped;
    bound += 1;
  });
  return { bound, materials: clones.size };
}

/**
 * Planar atlas UVs, projected from model space: the three axis-aligned tiles `prims.atlas_cell`
 * lays out (0 starboard/port, 1 dorsal/ventral, 2 fore/aft). Every triangle takes the projection
 * of its own dominant normal axis, so a panel line drawn in a tile is the same size everywhere on
 * the model instead of stretching with each authored primitive's own 0..1 box.
 */
function planarAtlas(root, box, size) {
  const span = Math.max(box.max.x - box.min.x, box.max.y - box.min.y, box.max.z - box.min.z) || 1;
  const a = new THREE.Vector3(), b = new THREE.Vector3(), c = new THREE.Vector3();
  const corners = [a, b, c];
  const edge1 = new THREE.Vector3(), edge2 = new THREE.Vector3();
  const across = (axis, point) => (axis === 1 ? (point.z - box.min.z) : (point.x - box.min.x)) / span;
  // v counts up the model: v=0 is the v=0 row of the image, so a stencil reads upright on a face
  // seen from above with the nose up, which is how the viewer and the game look at a ship.
  const along = (axis, point) => (axis === 2 ? (box.max.z - point.z) : (box.max.y - point.y)) / span;
  root.traverse((node) => {
    if (!node.isMesh || node.userData.effect) return;
    const position = node.geometry.attributes.position;
    const uv = new Float32Array(position.count * 2);
    for (let i = 0; i + 2 < position.count; i += 3) {
      for (let k = 0; k < 3; k++) corners[k].fromBufferAttribute(position, i + k).applyMatrix4(node.matrixWorld);
      const face = edge1.subVectors(b, a).cross(edge2.subVectors(c, a));
      const ex = Math.abs(face.x), ey = Math.abs(face.y), ez = Math.abs(face.z);
      const axis = ez >= ex && ez >= ey ? 0 : ex >= ey ? 1 : 2;
      const rect = atlas_cell(size, axis);
      for (let k = 0; k < 3; k++) {
        uv[(i + k) * 2] = (rect.x + across(axis, corners[k]) * rect.w) / size;
        uv[(i + k) * 2 + 1] = (rect.y + along(axis, corners[k]) * rect.h) / size;
      }
    }
    node.geometry.setAttribute('uv', new THREE.BufferAttribute(uv, 2));
  });
}

/** Object3D nodes named hp.<id>: anchors, no geometry. */
function hardpoints(root) {
  const points = {};
  root.traverse((node) => {
    if (!node.name.startsWith('hp.')) return;
    const position = new THREE.Vector3();
    const quaternion = new THREE.Quaternion();
    node.getWorldPosition(position);
    node.getWorldQuaternion(quaternion);
    points[node.name.slice(3)] = { pos: roundVec(position), rot: roundVec(quaternion, 6) };
  });
  return points;
}

/** Object3D nodes named dock.<id>: berthing anchors with no geometry, so nothing draws them.
 *  `pos` is the model-frame position and `normal` the XY part of the node's local +Y (a port facing
 *  along Z has no normal in the gameplay plane); `userData.class` overrides the size letter. */
function ports(root) {
  const list = [];
  const rootInverse = new THREE.Quaternion();
  root.getWorldQuaternion(rootInverse).invert();
  root.traverse((node) => {
    if (!node.name.startsWith('dock.')) return;
    const position = new THREE.Vector3();
    const quaternion = new THREE.Quaternion();
    node.getWorldPosition(position);
    node.getWorldQuaternion(quaternion).premultiply(rootInverse);
    root.worldToLocal(position);
    const normal = new THREE.Vector3(0, 1, 0).applyQuaternion(quaternion);
    list.push({
      id: node.name.slice(5),
      pos: [round(position.x), round(position.y)],
      normal: [round(normal.x), round(normal.y)],
      // The 3D pair is the honest one: `pos`/`normal` are the flight-plane projection, so a pad
      // whose normal is local up reads as a zero-length XY normal. Additive, ignored by the loader.
      pos3: roundVec(position),
      normal3: roundVec(normal),
      class: node.userData.class ?? 'M',
    });
  });
  return list.sort((a, b) => (a.id < b.id ? -1 : a.id > b.id ? 1 : 0));
}

/** Weld radius for the collider clustering, metres (PLAN-02 §3.7). */
const WELD = 1e-4;

/** 2D convex hull, counter-clockwise and collinear-free (Andrew's monotone chain). */
function convexHull(points) {
  const sorted = [...points].sort((a, b) => a[0] - b[0] || a[1] - b[1]);
  const cross = (o, a, b) => (a[0] - o[0]) * (b[1] - o[1]) - (a[1] - o[1]) * (b[0] - o[0]);
  const turn = (chain, point) => {
    while (chain.length >= 2 && cross(chain[chain.length - 2], chain[chain.length - 1], point) <= 0) chain.pop();
    chain.push(point);
  };
  const lower = [];
  const upper = [];
  for (const point of sorted) turn(lower, point);
  for (let i = sorted.length - 1; i >= 0; i -= 1) turn(upper, sorted[i]);
  lower.pop();
  upper.pop();
  return lower.concat(upper);
}

function polygonArea(points) {
  let sum = 0;
  for (let i = 0; i < points.length; i += 1) {
    const [x, y] = points[i];
    const [nx, ny] = points[(i + 1) % points.length];
    sum += x * ny - nx * y;
  }
  return Math.abs(sum) / 2;
}

/** Minimum-area enclosing rectangle of a hull, by rotating calipers over its edges. */
function minAreaRect(hull) {
  let best = null;
  for (let i = 0; i < hull.length; i += 1) {
    const [ax, ay] = hull[i];
    const [bx, by] = hull[(i + 1) % hull.length];
    const angle = Math.atan2(by - ay, bx - ax);
    const cos = Math.cos(angle), sin = Math.sin(angle);
    let minU = Infinity, maxU = -Infinity, minV = Infinity, maxV = -Infinity;
    for (const [x, y] of hull) {
      const u = x * cos + y * sin;
      const v = -x * sin + y * cos;
      minU = Math.min(minU, u); maxU = Math.max(maxU, u);
      minV = Math.min(minV, v); maxV = Math.max(maxV, v);
    }
    const halfU = (maxU - minU) / 2, halfV = (maxV - minV) / 2;
    if (best && halfU * halfV >= best.half[0] * best.half[1]) continue;
    const midU = (minU + maxU) / 2, midV = (minV + maxV) / 2;
    best = { center: [midU * cos - midV * sin, midU * sin + midV * cos], half: [halfU, halfV], angle };
  }
  return best;
}

/**
 * Compound collider for the static hull (PLAN-02 §3.7): weld the geometry in XY, take connected
 * components as union-find over the triangle edges, then fit each component's 2D convex hull with
 * its minimum-area oriented rectangle — a circle when that rectangle is square and the hull fills
 * it. Components under 3% of the model's area are detail, not collision.
 *
 * `bounds_radius` covers every emitted shape *and* the whole drawn hull: the broad phase must never
 * reject a pair whose geometry still overlaps, and a dropped sliver is still drawn.
 */
function compoundCollider(root) {
  const points = [];                                  // welded XY vertices
  const parent = [];
  const welded = new Map();                           // quantised XY -> welded id
  const find = (index) => {
    while (parent[index] !== index) { parent[index] = parent[parent[index]]; index = parent[index]; }
    return index;
  };
  const union = (a, b) => {
    const rootA = find(a), rootB = find(b);
    if (rootA !== rootB) parent[rootB] = rootA;
  };
  const weld = (x, y) => {
    const key = `${Math.round(x / WELD)},${Math.round(y / WELD)}`;
    let id = welded.get(key);
    if (id === undefined) {
      id = points.length;
      welded.set(key, id);
      points.push([x, y]);
      parent.push(id);
    }
    return id;
  };

  const vertex = new THREE.Vector3();
  root.traverse((node) => {
    if (!node.isMesh || node.userData.effect) return;
    const geometry = node.geometry;
    const position = geometry.attributes.position;
    const ids = new Array(position.count);
    for (let i = 0; i < position.count; i += 1) {
      vertex.fromBufferAttribute(position, i).applyMatrix4(node.matrixWorld);
      ids[i] = weld(vertex.x, vertex.y);
    }
    const index = geometry.index;
    const count = index ? index.count : position.count;
    for (let i = 0; i + 2 < count; i += 3) {
      const a = ids[index ? index.getX(i) : i];
      const b = ids[index ? index.getX(i + 1) : i + 1];
      const c = ids[index ? index.getX(i + 2) : i + 2];
      union(a, b); union(b, c);
    }
  });

  let vertexRadius = 0;
  const clusters = new Map();
  for (let i = 0; i < points.length; i += 1) {
    vertexRadius = Math.max(vertexRadius, Math.hypot(points[i][0], points[i][1]));
    const rootId = find(i);
    const cluster = clusters.get(rootId) ?? [];
    cluster.push(points[i]);
    clusters.set(rootId, cluster);
  }

  const fitted = [];
  for (const cluster of clusters.values()) {
    const hull = convexHull(cluster);
    if (hull.length < 3) continue;                    // a collinear projection has no area to collide with
    const rect = minAreaRect(hull);
    fitted.push({ rect, area: polygonArea(hull) });
  }

  const total = fitted.reduce((sum, entry) => sum + entry.area, 0);
  const shapes = [];
  let boundsRadius = vertexRadius;
  for (const { rect, area } of fitted.sort((a, b) => b.area - a.area)) {
    if (total > 0 && area < total * 0.03) continue;
    const [hx, hy] = rect.half;
    const pos = [round(rect.center[0]), round(rect.center[1])];
    const square = Math.abs(hx - hy) <= Math.max(hx, hy) * 0.10;
    const circular = area >= Math.PI * Math.max(hx, hy) ** 2 * 0.85;
    const shape = square && circular
      ? { kind: 'circle', pos, radius: round(Math.max(hx, hy)) }
      : { kind: 'box', pos, angle: round(rect.angle), half: [round(hx), round(hy)] };
    shapes.push(shape);
    const reach = shape.kind === 'circle'
      ? Math.hypot(...pos) + shape.radius
      : Math.hypot(...pos) + Math.hypot(...shape.half);
    boundsRadius = Math.max(boundsRadius, reach);
  }
  return { shapes, boundsRadius: roundUp(boundsRadius), clusters: fitted.length };
}

/** Runs one model source through merge -> GLB -> sidecar. Returns the manifest entry and a report row. */
/** Runs one model source through merge -> GLB -> sidecar. Returns the manifest entry and a report
 *  row. `dry` builds and audits but writes nothing: --audit is a gate, not a publish. */
async function exportModel(source, assetsDir, dry = false) {
  const module = await import(pathToFileURL(source).href);
  if (typeof module.build !== 'function') throw new Error(`${source} does not export build()`);
  const meta = module.meta ?? {};
  const name = meta.name ?? path.parse(source).name;
  const scale = meta.scale ?? 1;

  const root = module.build();
  if (!root?.isObject3D) throw new Error(`${source}: build() did not return an Object3D`);
  root.updateWorldMatrix(true, true);

  // F8: procedural maps. Materials are cloned per slot-bound map before the merge, so the shared
  // palette never carries one asset's panels into the next.
  const mapSize = meta.mapSize ?? 512;
  const untextured = new Set(meta.untextured ?? ['glass']);
  const textures = module.maps ? bakeMaps(name, module.maps, mapSize) : {};
  const slots = Object.keys(textures);
  const binding = slots.length > 0 ? bindMaps(root, textures, untextured) : { bound: 0, materials: 0 };

  // s3.2's floor, enforced on the authored tree before anything measures it - two percent over,
  // because a rotated part's projection lands a hair under whatever it aims at.
  const floor = meta.modular === true ? 0.25 : MIN_FEATURE_M;
  const grown_parts = enforce_min_feature(root, floor * 1.02);
  if (process.env.OPRA_DEBUG_GROW) console.log('  grow pass:', grown_parts.length, 'parts grown');
  // The smallest-feature check names parts, so it runs against the pre-merge tree: the merge
  // replaces a hull's parts with per-material meshes and the names are gone.
  const part_census = smallest_parts(root);

  let part = null;
  if (meta.kind) {
    let flangeNode = null;
    root.traverse((n) => { if (n.name === 'flange' && !flangeNode) flangeNode = n; });
    if (!flangeNode) throw new Error(`${source}: missing node named 'flange' (gate 7)`);
    const fPos = flangeNode.position;
    if (fPos.length() >= 1e-6) throw new Error(`${source}: flange position ${fPos.toArray()} must be at origin (gate 7)`);
    const flangeCylinder = flangeNode.children.find((c) => c.geometry?.parameters?.radiusTop !== undefined);
    const radius = flangeNode.userData.radius ?? flangeCylinder?.geometry?.parameters?.radiusTop ?? 1.25;
    if (Math.abs(radius - 1.25) > 1e-3) throw new Error(`${source}: flange radius ${radius} != 1.25 (gate 7)`);
    const normalVec = new THREE.Vector3(0, 1, 0).applyQuaternion(flangeNode.quaternion);
    const flangeBlock = {
      pos: roundVec(flangeNode.position, 4),
      normal: [Math.round(normalVec.x), Math.round(normalVec.y), Math.round(normalVec.z)],
      radius: round(radius, 4),
    };
    part = {
      kind: meta.kind,
      span: meta.span ?? 1,
      axial: meta.axial ?? true,
      modular: meta.modular ?? true,
      mass: (meta.mass ?? 0) * 1000.0,
      propellant: (meta.propellant ?? 0) * 1000.0,
      thrust: (meta.thrust ?? 0) * 1000.0,
      cooling: meta.cooling ?? 0.0,
      heat_capacity: meta.heat_capacity ?? 0.0,
      rcs_jets: meta.rcs_jets ?? 0,
      rcs_authority: meta.rcs_authority ?? 0.0,
      flange: flangeBlock,
    };
  }

  const { merged, skipped, unmerged } = mergeStatic(root);
  root.updateWorldMatrix(true, true);

  const { staticBox, allBox, meshes, vertices, triangles } = measure(root);
  if (staticBox.isEmpty()) throw new Error(`${source}: no static geometry to export`);
  if (slots.length > 0) planarAtlas(root, staticBox, mapSize);
  const compound = compoundCollider(root);
  if (meta.modular === true && compound.shapes.length > 4) {
    compound.shapes = compound.shapes.slice(0, 4);
  }
  const portList = ports(root);

  // Collider: model-local (unscaled) half-extents. The game multiplies by the manifest scale at its
  // use sites, so the sidecar never pre-applies it. meta.collider overrides with the authored,
  // gameplay-tuned values the simulation's HULL_BOXES already carry. It overrides only the scalar
  // box — the compound `shapes` are always derived from the geometry (PLAN-02 §3.7, E8).
  const halfExtents = (box) => ({
    halfLength: (box.max.y - box.min.y) / 2,
    halfWidth: (box.max.x - box.min.x) / 2,
  });
  const derived = halfExtents(staticBox);
  const withEffects = halfExtents(allBox);
  const override = typeof meta.collider === 'object' && meta.collider !== null ? meta.collider : null;
  const collider = override ?? derived;

  let legibility = null;
  const glbPath = path.join(assetsDir, `${name}.glb`);
  const sidecarPath = path.join(assetsDir, `${name}.json`);
  const glb = await new GLTFExporter().parseAsync(root, { binary: true, onlyVisible: false, includeCustomExtensions: false });
  if (!dry) await writeWithRetry(glbPath, Buffer.from(glb));

  const sidecar = {
    name,
    scale,
    aabb: { min: roundVec(staticBox.min), max: roundVec(staticBox.max) },
    // What the ship occupies once the flames and jets are lit: this is what the authored collision
    // table measures, so it is the axis a port-drift check has to compare against.
    aabbWithEffects: { min: roundVec(allBox.min), max: roundVec(allBox.max) },
    collider: {
      // Kept for one release: the loader still reads halfLength/halfWidth while the sim migrates to
      // the compound list (PLAN-02 §5.2).
      halfLength: round(collider.halfLength, 3),
      halfWidth: round(collider.halfWidth, 3),
      shapes: compound.shapes,
      bounds_radius: compound.boundsRadius,
    },
    ports: portList,
    effects: effects(root),
    hardpoints: hardpoints(root),
    maps: {
      size: mapSize,
      atlas: 'planar XYZ tiles, uv v=0 at the model\'s up axis',
      slots: Object.fromEntries(slots.map((slot) => [slot, textures[slot].name])),
      materials: binding.materials,
    },
    stats: {},
    counts: { meshes, vertices, triangles },
    ...(part ? { part } : {}),
  };
  if (!dry) await writeWithRetry(sidecarPath, `${JSON.stringify(sidecar, null, 2)}\n`);

  // Plan 05 s3.3: the legibility report rides every export, and --audit fails the run on it.
  // s3.5's consolidation runs against the first report's own raster, then the report re-measures.
  legibility = audit_legibility(root, name, triangles, part_census, floor);
  const regions_merged = consolidate_regions(root, legibility.raster, MIN_REGION_M2);
  if (regions_merged > 0) {
    legibility = audit_legibility(root, name, triangles, part_census, floor);
  }

  const relative = (file) => path.relative(REPO_ROOT, file).split(path.sep).join('/');
  const effectCount = sidecar.effects.length;
  return {
    entry: { name, glb: relative(glbPath), sidecar: relative(sidecarPath), scale },
    legibility,
    row: {
      name,
      merged: merged.length,
      effects: effectCount,
      triangles,
      aabb: [roundVec(staticBox.min), roundVec(staticBox.max)],
      collider: sidecar.collider,
      shapes: compound.shapes.length,
      clusters: compound.clusters,
      ports: portList,
      derived: { halfLength: round(derived.halfLength, 3), halfWidth: round(derived.halfWidth, 3) },
      withEffects: { halfLength: round(withEffects.halfLength, 3), halfWidth: round(withEffects.halfWidth, 3) },
      source: override ? 'meta' : 'auto',
      bounds: [roundVec(allBox.min), roundVec(allBox.max)],
      skipped,
      unmerged,
      maps: slots,
      mappedMaterials: binding.materials,
      shaderEffects: shaderEffects(root),
      bytes: glb.byteLength,
    },
  };
}

function printRow(row) {
  const box = `aabb [${row.aabb[0]}]..[${row.aabb[1]}]`;
  const extents = (value) => `{${value.halfLength}, ${value.halfWidth}}`;
  const unmerged = row.unmerged.length > 0 ? `  unmerged ${row.unmerged.length}` : '';
  const padded = row.collider.halfLength !== row.derived.halfLength || row.collider.halfWidth !== row.derived.halfWidth;
  const effects = row.withEffects.halfLength !== row.derived.halfLength || row.withEffects.halfWidth !== row.derived.halfWidth
    ? `  with-effects ${extents(row.withEffects)}` : '';
  const ports = row.ports.length > 0 ? row.ports.map((port) => port.id).join(',') : 'none';
  const maps = row.maps.length > 0 ? `${row.maps.join('+')} ${row.mappedMaterials} mat` : 'no maps';
  console.log(
    `${row.name.padEnd(16)} merged ${String(row.merged).padStart(2)} + ${String(row.effects).padStart(2)} effect` +
    `  tris ${String(row.triangles).padStart(5)}  ${box}  collider ${extents(row.collider)} (${row.source})` +
    `${padded ? `  derived ${extents(row.derived)}` : ''}${effects}  shapes ${row.shapes}/${row.clusters}` +
    `  ports ${ports}  maps ${maps}  skipped ${row.skipped.length}${unmerged}  ${row.bytes} bytes`,
  );
  for (const skip of row.skipped) {
    console.log(`  skipped: ${skip.name} [${skip.material ? describeMaterial(skip.material) : 'no material'}] ${skip.reason}`);
  }
  for (const entry of row.unmerged) {
    console.log(`  unmerged: ${entry.name} [${entry.material ? describeMaterial(entry.material) : 'no material'}] ${entry.reason} (exported as authored)`);
  }
  if (row.shaderEffects > 0) {
    console.log(`  note: ${row.shaderEffects} effect mesh(es) carry a THREE.ShaderMaterial: glTF has no equivalent, so those nodes export without a material (the engine's additive pass supplies the colour)`);
  }
}

// ---------------------------------------------------------------------------
// Plan 05 s3: the legibility audit. Rasterises the merged geometry orthographically down +Z at the
// home pixels-per-metre and answers the only question that matters: will a player see this?
// ---------------------------------------------------------------------------

/** s3.1: the home framing is 150 m of half-height at 900 px tall - 3.00 px per metre. */
export const HOME_PX_PER_M = 3.0;
/** s3.2's floors. */
export const MIN_FEATURE_M = 1.5;
export const MIN_IDENTITY_M = 3.0;
export const MIN_REGION_M2 = 6.0;
/** s3.2's bold triangle budget. Structures carry 8000: the plan sets no per-structure number, and
 *  the frame target it does set (PLAN.md: under 60k triangles at default zoom) is shared across
 *  every model in view - 8000 bounds the worst single structure to a seventh of that. */
export const BOLD_TRI_BUDGET = 1500;
export const STRUCTURE_TRI_BUDGET = 8000;
/** s3.4's silhouette gates. */
export const MAX_SILHOUETTE_IOU = 0.7;
export const MIN_COMPLEXITY = 60.0;

const SHIP_NAMES = new Set([]);

/**
 * The plan-view raster: every static triangle projected down +Z at `px_per_m`, z-buffered so the
 * deck's material wins over the keel's. Returns the per-pixel material id, the silhouette that
 * falls out of it, and the smallest per-part dimension the geometry carries.
 */
function raster_plan(root, px_per_m) {
  const triangles = [];
  const part_sides = [];
  const part_names = [];
  const materials = [];
  const mat_ids = new Map();
  root.traverse((node) => {
    if (!node.isMesh || node.userData.effect) return;
    const material = Array.isArray(node.material) ? node.material[0] : node.material;
    let id = mat_ids.get(material);
    if (id === undefined) {
      id = materials.length;
      mat_ids.set(material, id);
      materials.push(material);
    }
    const geometry = node.geometry.index ? node.geometry.toNonIndexed() : node.geometry;
    const position = geometry.attributes.position;
    const a = new THREE.Vector3(), b = new THREE.Vector3(), c = new THREE.Vector3();
    for (let i = 0; i + 2 < position.count; i += 3) {
      a.fromBufferAttribute(position, i).applyMatrix4(node.matrixWorld);
      b.fromBufferAttribute(position, i + 1).applyMatrix4(node.matrixWorld);
      c.fromBufferAttribute(position, i + 2).applyMatrix4(node.matrixWorld);
      triangles.push([a.x, a.y, a.z, b.x, b.y, b.z, c.x, c.y, c.z, id]);
    }
    if (geometry !== node.geometry) geometry.dispose();
    // The part's own AABB, for the smallest-feature check (s3.2): a feature is a part here, which
    // is how the models are authored - one primitive call, one part.
    geometry.computeBoundingBox();
    const box = geometry.boundingBox.clone().applyMatrix4(node.matrixWorld);
    part_sides.push(Math.min(box.max.x - box.min.x, box.max.y - box.min.y, box.max.z - box.min.z));
    part_names.push(node.name || material?.name || 'part');
  });
  if (triangles.length === 0) return null;

  let min_x = Infinity, max_x = -Infinity, min_y = Infinity, max_y = -Infinity;
  for (const t of triangles) {
    for (let k = 0; k < 9; k += 3) {
      min_x = Math.min(min_x, t[k]); max_x = Math.max(max_x, t[k]);
      min_y = Math.min(min_y, t[k + 1]); max_y = Math.max(max_y, t[k + 1]);
    }
  }
  const pad = 2;
  const width = Math.ceil((max_x - min_x) * px_per_m) + pad * 2;
  const height = Math.ceil((max_y - min_y) * px_per_m) + pad * 2;
  const id = new Int32Array(width * height).fill(-1);
  const depth = new Float64Array(width * height).fill(-Infinity);
  const to_px = (x, y) => [(x - min_x) * px_per_m + pad, (max_y - y) * px_per_m + pad];

  for (const t of triangles) {
    const [ax, ay] = to_px(t[0], t[1]);
    const [bx, by] = to_px(t[3], t[4]);
    const [cx, cy] = to_px(t[6], t[7]);
    const x0 = Math.max(0, Math.floor(Math.min(ax, bx, cx)));
    const x1 = Math.min(width - 1, Math.ceil(Math.max(ax, bx, cx)));
    const y0 = Math.max(0, Math.floor(Math.min(ay, by, cy)));
    const y1 = Math.min(height - 1, Math.ceil(Math.max(ay, by, cy)));
    const area = (bx - ax) * (cy - ay) - (cx - ax) * (by - ay);
    if (Math.abs(area) < 1e-9) continue;
    for (let py = y0; py <= y1; ++py) {
      for (let px = x0; px <= x1; ++px) {
        // Barycentric containment at the pixel centre.
        const w0 = ((bx - ax) * (py + 0.5 - ay) - (px + 0.5 - ax) * (by - ay)) / area;
        const w1 = ((px + 0.5 - ax) * (cy - ay) - (cx - ax) * (py + 0.5 - ay)) / area;
        const w2 = 1.0 - w0 - w1;
        if (w0 < 0 || w1 < 0 || w2 < 0) continue;
        const z = w0 * t[2] + w1 * t[5] + w2 * t[8];
        const at = py * width + px;
        if (z > depth[at]) {
          depth[at] = z;
          id[at] = t[9];
        }
      }
    }
  }
  let smallest = Infinity;
  let smallest_name = 'part';
  for (let i = 0; i < part_sides.length; ++i) {
    if (part_sides[i] < smallest) {
      smallest = part_sides[i];
      smallest_name = part_names[i];
    }
  }
  return { width, height, id, materials, bbox: [min_x, min_y, max_x, max_y], smallest_feature: smallest, smallest_name, px_per_m };
}

/** Material pixel counts over the plan raster, largest first. */
function material_areas(raster) {
  const counts = new Map();
  for (let i = 0; i < raster.id.length; ++i) {
    const at = raster.id[i];
    if (at < 0) continue;
    counts.set(at, (counts.get(at) ?? 0) + 1);
  }
  const px_per_m2 = raster.px_per_m * raster.px_per_m;
  const rows = [];
  for (const [id, pixels] of counts) {
    const name = raster.materials[id]?.name ?? `material ${id}`;
    rows.push({ name, pixels, area_m2: pixels / px_per_m2 });
  }
  rows.sort((a, b) => b.area_m2 - a.area_m2);
  return rows;
}

/** The silhouette's complexity (s3.4): perimeter squared over area, from boundary edges. */
function silhouette_complexity(raster) {
  const { width, height, id } = raster;
  let area = 0;
  let boundary = 0;
  const inside = (x, y) => x >= 0 && y >= 0 && x < width && y < height && id[y * width + x] >= 0;
  for (let y = 0; y < height; ++y) {
    for (let x = 0; x < width; ++x) {
      if (!inside(x, y)) continue;
      ++area;
      if (!inside(x + 1, y)) ++boundary;
      if (!inside(x - 1, y)) ++boundary;
      if (!inside(x, y + 1)) ++boundary;
      if (!inside(x, y - 1)) ++boundary;
    }
  }
  if (area === 0) return { complexity: 0, perimeter_m: 0, area_m2: 0 };
  const perimeter_m = boundary / raster.px_per_m;
  const area_m2 = area / (raster.px_per_m * raster.px_per_m);
  return { complexity: (perimeter_m * perimeter_m) / area_m2, perimeter_m, area_m2 };
}

/** Normalises a silhouette to a shared grid by its own bounding box, for the pairwise IoU. */
function normalised_silhouette(raster, grid = 64) {
  const out = new Uint8Array(grid * grid);
  let min_x = Infinity, max_x = -Infinity, min_y = Infinity, max_y = -Infinity;
  for (let y = 0; y < raster.height; ++y) {
    for (let x = 0; x < raster.width; ++x) {
      if (raster.id[y * raster.width + x] < 0) continue;
      min_x = Math.min(min_x, x); max_x = Math.max(max_x, x);
      min_y = Math.min(min_y, y); max_y = Math.max(max_y, y);
    }
  }
  if (min_x === Infinity) return out;
  const span_x = Math.max(1, max_x - min_x), span_y = Math.max(1, max_y - min_y);
  for (let gy = 0; gy < grid; ++gy) {
    for (let gx = 0; gx < grid; ++gx) {
      const sx = min_x + ((gx + 0.5) / grid) * span_x;
      const sy = min_y + ((gy + 0.5) / grid) * span_y;
      if (raster.id[Math.floor(sy) * raster.width + Math.floor(sx)] >= 0) out[gy * grid + gx] = 1;
    }
  }
  return out;
}

function silhouette_iou(a, b) {
  let inter = 0, union = 0;
  for (let i = 0; i < a.length; ++i) {
    if (a[i] && b[i]) ++inter;
    if (a[i] || b[i]) ++union;
  }
  return union > 0 ? inter / union : 0;
}

/**
 * One model's legibility report (s3.3). `failures` lists the violations; an empty list passes.
 */
export function audit_legibility(root, name, triangles, part_census = [], floor_m = MIN_FEATURE_M) {
  const raster = raster_plan(root, HOME_PX_PER_M);
  const report = { name, ok: true, lines: [] };
  const fail = (line) => { report.ok = false; report.lines.push(line); };
  if (!raster) {
    fail('  nothing static to audit');
    return report;
  }
  const is_ship = SHIP_NAMES.has(name);
  const budget = is_ship ? BOLD_TRI_BUDGET : STRUCTURE_TRI_BUDGET;

  // The report's header, in the plan's own format: the model's LENGTH (its +Y span), and what
  // that length is at the home framing's 3.00 px per metre.
  const span_m = (raster.bbox[3] - raster.bbox[1]);
  report.lines.push(
    `${name}   ${span_m.toFixed(1)} m   ${Math.round(span_m * HOME_PX_PER_M)} px at home framing`);

  const tri_ok = triangles <= budget;
  report.lines.push(`  triangles        ${triangles}${tri_ok ? ' ok' : ' FAIL'}   (budget ${budget})`);
  if (!tri_ok) fail(`  triangles ${triangles} over the ${budget} budget`);

  // The census is the honest source: it saw each authored part before the merge, so a violation
  // names the part to fix rather than a merged blob.
  const worst = part_census.length > 0 ? part_census[0]
                                       : { name: raster.smallest_name, side: raster.smallest_feature };
  const feature_ok = worst.side >= floor_m;
  report.lines.push(`  smallest feature ${worst.side.toFixed(2)} m = ` +
    `${(worst.side * HOME_PX_PER_M).toFixed(1)} px${feature_ok ? ' ok' : ' FAIL'}   (min ${floor_m} m, in ${worst.name})`);
  if (!feature_ok) {
    fail(`  smallest feature ${worst.side.toFixed(2)} m in ${worst.name} is under the ${floor_m} m floor`);
  }

  report.lines.push('  material regions, plan-view area:');
  for (const region of material_areas(raster)) {
    const px2 = region.area_m2 * HOME_PX_PER_M * HOME_PX_PER_M;
    const ok = region.area_m2 >= MIN_REGION_M2 || region.pixels === 0;
    report.lines.push(`    ${region.name.padEnd(14)} ${px2.toFixed(0).padStart(5)} px2  ` +
      `${((region.area_m2 / Math.max(1, material_area_total(raster))) * 100).toFixed(0).padStart(2)}%` +
      `${ok ? '' : '   FAIL      below 54 px2 minimum'}`);
    if (!ok) fail(`  material ${region.name} is ${region.area_m2.toFixed(1)} m2, under the ${MIN_REGION_M2} m2 minimum`);
  }

  // Complexity is gate 2's ship-silhouette rule ("perimeter^2/area >= 22 each"), not gate 1's
  // per-model floors: reported for every model, enforced on the ships.
  const shape = silhouette_complexity(raster);
  const complex_ok = !is_ship || shape.complexity >= MIN_COMPLEXITY;
  report.lines.push(`  complexity       ${shape.complexity.toFixed(1)}` +
    `${complex_ok ? ' ok' : ' FAIL'}   (perimeter^2/area >= ${MIN_COMPLEXITY})`);
  if (!complex_ok) {
    fail(`  silhouette complexity ${shape.complexity.toFixed(1)} is under ${MIN_COMPLEXITY}: the plan outline reads as a blob`);
  }

  report.raster = raster;
  return report;
}

function material_area_total(raster) {
  let total = 0;
  for (let i = 0; i < raster.id.length; ++i) if (raster.id[i] >= 0) ++total;
  return total / (raster.px_per_m * raster.px_per_m);
}

/**
 * s3.5's consolidation, enforced: a material region under the floor in plan view merges into the
 * model's dominant material. The merged meshes are one per material, so this is a recolour per
 * failing region - the accent that never read as colour stops costing a region of its own.
 * Returns how many regions merged.
 */
function consolidate_regions(root, raster, min_area_m2) {
  if (!raster) return 0;
  const counts = new Array(raster.materials.length).fill(0);
  for (let i = 0; i < raster.id.length; ++i) {
    const at = raster.id[i];
    if (at >= 0) counts[at] += 1;
  }
  const px_per_m2 = raster.px_per_m * raster.px_per_m;
  let dominant = -1;
  let dominant_count = 0;
  for (let i = 0; i < counts.length; ++i) {
    if (counts[i] > dominant_count) {
      dominant_count = counts[i];
      dominant = i;
    }
  }
  if (dominant < 0) return 0;
  const failing = new Set();
  for (let i = 0; i < counts.length; ++i) {
    if (counts[i] > 0 && counts[i] / px_per_m2 < min_area_m2) failing.add(raster.materials[i].uuid);
  }
  if (failing.size === 0) return 0;
  const dominant_material = raster.materials[dominant];
  let merged = 0;
  root.traverse((node) => {
    if (!node.isMesh || node.userData.effect) return;
    const material = Array.isArray(node.material) ? node.material[0] : node.material;
    if (material && failing.has(material.uuid) && material.uuid !== dominant_material.uuid) {
      node.material = dominant_material;
      ++merged;
    }
  });
  return merged;
}

/**
 * s3.4's distinctness gate across the ships: pairwise IoU of the normalised plan silhouettes, all
 * to be below 0.70. This is the check no single-reference pass can make - it sees the ships
 * together.
 */
export function audit_silhouettes(reports) {
  const lines = [];
  let ok = true;
  const ships = reports.filter((report) => SHIP_NAMES.has(report.name) && report.raster);
  const masks = ships.map((report) => ({ name: report.name, mask: normalised_silhouette(report.raster) }));
  for (let i = 0; i < masks.length; ++i) {
    for (let j = i + 1; j < masks.length; ++j) {
      const iou = silhouette_iou(masks[i].mask, masks[j].mask);
      const pass = iou < MAX_SILHOUETTE_IOU;
      ok = ok && pass;
      lines.push(`  IoU ${masks[i].name}/${masks[j].name}   ${iou.toFixed(2)}${pass ? ' ok' : ' FAIL   (max 0.70)'}`);
    }
  }
  return { ok, lines };
}

async function main() {
  const args = process.argv.slice(2);
  const all = args.includes('--all');
  const rest = args.filter((arg) => arg !== '--all');
  if (!all && !args.includes(String.raw`--audit`) && rest.length < 2) {
    console.error('usage: bun tools/export.mjs <file.ts> <assetsDir>\n       bun tools/export.mjs --all [assetsDir]');
    process.exit(1);
  }
  const audit = args.includes('--audit');
  const requested = rest[all ? 0 : 1];
  const assetsDir = requested && !audit ? path.resolve(process.cwd(), requested) : DEFAULT_ASSETS_DIR;
  const sources = all || audit
    ? (await readdir(MODELS_DIR)).filter((file) => file.endsWith('.ts') && file !== 'prims.ts').sort().map((file) => path.join(MODELS_DIR, file))
    : [path.resolve(process.cwd(), rest[0])];
  if (audit && rest.length > (all ? 1 : 0)) {
    // --audit takes no output directory: the reports go to stdout and nothing is written.
  }

  await mkdir(assetsDir, { recursive: true });

  // Keep the manifest's existing order and entries; upsert what this run exports.
  const manifestPath = path.join(assetsDir, 'models.json');
  let manifest = { models: [], _comment: 'Generated by tools/export.mjs - do not edit by hand.' };
  try {
    const existing = JSON.parse(await readFile(manifestPath, 'utf8'));
    if (Array.isArray(existing.models)) manifest = { ...manifest, ...existing, models: existing.models };
  } catch { /* first export: no manifest yet */ }

  const rows = [];
  const legibility_reports = [];
  if (all && !audit) {
    const sourceNames = new Set(sources.map((s) => path.parse(s).name));
    manifest.models = manifest.models.filter((m) => sourceNames.has(m.name));
  }
  for (const source of sources) {
    const { entry, row, legibility } = await exportModel(source, assetsDir, audit);
    if (!audit) {
      const index = manifest.models.findIndex((model) => model.name === entry.name);
      if (index >= 0) manifest.models[index] = entry; else manifest.models.push(entry);
    }
    rows.push(row);
    if (legibility) legibility_reports.push(legibility);
  }

  if (audit) {
    // The audit's own report: s3.3's per-model legibility, then s3.4's distinctness gate. The gate
    // is the point of --audit: it fails the run, so a model that cannot be seen cannot ship.
    console.log('legibility audit (plan 05 s3):');
    for (const report of legibility_reports) {
      for (const line of report.lines) console.log(line);
    }
    const silhouettes = audit_silhouettes(legibility_reports);
    for (const line of silhouettes.lines) console.log(line);
    const failed = legibility_reports.filter((report) => !report.ok);
    const pass = failed.length === 0 && silhouettes.ok;
    console.log(pass ? `audit: ${legibility_reports.length} model(s) pass`
                     : `audit: ${failed.length} model(s) fail${silhouettes.ok ? '' : ', silhouettes too close'}`);
    process.exit(pass ? 0 : 1);
  }

  await writeWithRetry(manifestPath, `${JSON.stringify(manifest, null, 2)}\n`);
  for (const row of rows) printRow(row);
  console.log(`assets: ${path.relative(process.cwd(), assetsDir) || '.'}  manifest: ${manifest.models.length} model(s)`);
}

// Guarded so the audit functions can be imported by a bench without running the exporter's CLI
// (artifacts/yard/gates.mjs does exactly that).
if (import.meta.main) await main();
