// Offscreen preview rasters. No browser: playwright cannot launch chromium in this environment
// (every channel times out at <launched>), and a preview of the approved hull is not optional, so
// the bench draws its own.
//
// The game view is the engine's, not an approximation: render/camera.h's CAMERA_FOV_Y and
// CAMERA_PITCH_DEFAULT, and camera.cpp's orbit_eye() offset (0, -cos p, sin p) with the up vector
// the C++ frustum derives, cross(right, forward). What comes out is the framing the player gets.
//
//   bun run render.mjs designs/variant_a_hammerhead.ts            # all four views
//   bun run render.mjs --view game --size 1600x1000 designs/*.ts
import { writeFile, mkdir } from 'node:fs/promises';
import { pathToFileURL } from 'node:url';
import * as THREE from 'three';
import { encode_png } from '../../tools/models/prims.ts';

/** render/camera.h: 24 degrees vertical, 32 degrees of orbit pitch. */
const FOV_Y = 0.41887903;
const PITCH = 0.55850536;
/** Supersampling factor. Flat-shaded facets alias badly on a chine; 3x costs nothing here. */
const SS = 3;

const VIEWS = {
  game: { dir: [0, -Math.cos(PITCH), Math.sin(PITCH)], up: [0, 1, 0], fit: 1.06 },
  plan: { dir: [0, 0, 1], up: [0, 1, 0], fit: 1.02 },
  iso: { dir: [0.58, -0.62, 0.52], up: [0, 0, 1], fit: 1.04 },
  side: { dir: [1, 0, 0.06], up: [0, 0, 1], fit: 1.02 },
};

/** World-space triangles with a face normal and the source material. */
function collect(root) {
  const out = [];
  const a = new THREE.Vector3(), b = new THREE.Vector3(), c = new THREE.Vector3();
  const ab = new THREE.Vector3(), ac = new THREE.Vector3();
  root.traverse((node) => {
    if (!node.isMesh || node.userData.effect) return;
    const material = Array.isArray(node.material) ? node.material[0] : node.material;
    const geometry = node.geometry.index ? node.geometry.toNonIndexed() : node.geometry;
    const position = geometry.attributes.position;
    for (let i = 0; i + 2 < position.count; i += 3) {
      a.fromBufferAttribute(position, i).applyMatrix4(node.matrixWorld);
      b.fromBufferAttribute(position, i + 1).applyMatrix4(node.matrixWorld);
      c.fromBufferAttribute(position, i + 2).applyMatrix4(node.matrixWorld);
      ab.subVectors(b, a); ac.subVectors(c, a);
      const n = new THREE.Vector3().crossVectors(ab, ac).normalize();
      out.push({ a: a.clone(), b: b.clone(), c: c.clone(), n, material });
    }
    if (geometry !== node.geometry) geometry.dispose();
  });
  return out;
}

/**
 * Three lights, matching the prototype's rig so the production hull can be compared to the approved
 * frame: a hard key over the starboard shoulder, a cold fill from below and to port, a rim from
 * astern. Unlit materials (the glow and the amber lamp) bypass all of it.
 */
const KEY = new THREE.Vector3(0.45, 0.42, 0.79).normalize();
const FILL = new THREE.Vector3(-0.62, -0.5, 0.6).normalize();
const RIM = new THREE.Vector3(0, -0.82, -0.57).normalize();
const FILL_TINT = [0.28, 0.41, 0.51];
const RIM_TINT = [0.51, 0.71, 0.85];

function shade(material, n) {
  const c = material.color;
  if (!material.isMeshStandardMaterial) return [c.r, c.g, c.b];  // glow, lamps: unlit by design
  const key = Math.max(0, n.dot(KEY)) * 1.35;
  const fill = Math.max(0, n.dot(FILL)) * 0.42;
  const rim = Math.max(0, n.dot(RIM)) * 0.3;
  // Metalness reads as a tighter, brighter key rather than a real specular lobe: at 3 px per metre
  // in the game a Blinn term is one pixel, and this keeps the palette's own relationships intact.
  const gloss = Math.pow(Math.max(0, n.dot(KEY)), 8) * (0.25 + 0.55 * (material.metalness ?? 0)) *
    (1 - (material.roughness ?? 0.6) * 0.8);
  // Fill and rim modulate the albedo like the key does. Adding them unmodulated washes every
  // material toward the fill's blue and the palette stops meaning anything.
  return [
    c.r * (0.14 + key + FILL_TINT[0] * fill + RIM_TINT[0] * rim) + gloss,
    c.g * (0.14 + key + FILL_TINT[1] * fill + RIM_TINT[1] * rim) + gloss,
    c.b * (0.14 + key + FILL_TINT[2] * fill + RIM_TINT[2] * rim) + gloss,
  ];
}

function render(root, view, width, height) {
  root.updateMatrixWorld(true);
  const tris = collect(root);
  const box = new THREE.Box3();
  for (const t of tris) { box.expandByPoint(t.a); box.expandByPoint(t.b); box.expandByPoint(t.c); }
  const target = box.getCenter(new THREE.Vector3());
  const sphere = box.getBoundingSphere(new THREE.Sphere());

  const spec = VIEWS[view];
  const forward = new THREE.Vector3(...spec.dir).normalize().negate();   // eye -> target
  const aspect = width / height;
  // Fit the bounding sphere in the tighter of the two half-angles.
  const tan_y = Math.tan(FOV_Y * 0.5);
  const tan_x = tan_y * aspect;
  let distance = (sphere.radius * spec.fit) / Math.min(tan_y, tan_x);
  let eye = target.clone().addScaledVector(forward, -distance);
  // The C++ frustum's basis, reproduced: right from the world up hint, then up from right x forward.
  const hint = new THREE.Vector3(...spec.up);
  let right = new THREE.Vector3().crossVectors(forward, hint);
  if (right.lengthSq() < 1e-8) right = new THREE.Vector3().crossVectors(forward, new THREE.Vector3(0, 1, 0));
  right.normalize();
  const up = new THREE.Vector3().crossVectors(right, forward).normalize();

  // Iterate the framing. A bounding sphere on a 21 m hull seen nearly end-on wastes most of the
  // frame, and the box centre is not the centre of what projects, so each pass re-measures the
  // drawn extent, recentres the target on it and pulls the eye in to fit. Three passes converge.
  for (let pass = 0; pass < 3; ++pass) {
    let min_x = Infinity, max_x = -Infinity, min_y = Infinity, max_y = -Infinity;
    const o = new THREE.Vector3();
    let ok = true;
    for (const t of tris) {
      for (const p of [t.a, t.b, t.c]) {
        o.copy(p).sub(eye);
        const z = o.dot(forward);
        if (z <= 1e-3) { ok = false; break; }
        const nx = o.dot(right) / (z * tan_x), ny = o.dot(up) / (z * tan_y);
        min_x = Math.min(min_x, nx); max_x = Math.max(max_x, nx);
        min_y = Math.min(min_y, ny); max_y = Math.max(max_y, ny);
      }
      if (!ok) break;
    }
    if (!ok) break;
    target.addScaledVector(right, ((min_x + max_x) / 2) * tan_x * distance)
          .addScaledVector(up, ((min_y + max_y) / 2) * tan_y * distance);
    distance *= Math.max((max_x - min_x) / 2, (max_y - min_y) / 2) * spec.fit;
    eye = target.clone().addScaledVector(forward, -distance);
  }

  const w = width * SS, h = height * SS;
  const colour = new Float32Array(w * h * 3);
  const depth = new Float32Array(w * h);          // 1/z, larger is nearer
  // Background: a cold vertical gradient, so a dark hull still has an edge against it.
  for (let y = 0; y < h; ++y) {
    const t = y / h;
    for (let x = 0; x < w; ++x) {
      const at = (y * w + x) * 3;
      colour[at] = 0.0055 + 0.004 * (1 - t);
      colour[at + 1] = 0.008 + 0.006 * (1 - t);
      colour[at + 2] = 0.013 + 0.009 * (1 - t);
    }
  }

  const project = (p) => {
    const o = p.clone().sub(eye);
    const z = o.dot(forward);
    if (z <= 1e-3) return null;
    return [
      (o.dot(right) / (z * tan_x) * 0.5 + 0.5) * w,
      (0.5 - o.dot(up) / (z * tan_y) * 0.5) * h,
      z,
    ];
  };

  for (const t of tris) {
    const pa = project(t.a), pb = project(t.b), pc = project(t.c);
    if (!pa || !pb || !pc) continue;
    const area = (pb[0] - pa[0]) * (pc[1] - pa[1]) - (pc[0] - pa[0]) * (pb[1] - pa[1]);
    if (Math.abs(area) < 1e-9) continue;
    // Two-sided: a module's inner liner is only ever seen through the mouth of the bell in front of
    // it, and back-face culling would take the bore with it.
    const n = area < 0 ? t.n.clone().negate() : t.n;
    const [r, g, b] = shade(t.material, n);
    const x0 = Math.max(0, Math.floor(Math.min(pa[0], pb[0], pc[0])));
    const x1 = Math.min(w - 1, Math.ceil(Math.max(pa[0], pb[0], pc[0])));
    const y0 = Math.max(0, Math.floor(Math.min(pa[1], pb[1], pc[1])));
    const y1 = Math.min(h - 1, Math.ceil(Math.max(pa[1], pb[1], pc[1])));
    const inv_a = 1 / pa[2], inv_b = 1 / pb[2], inv_c = 1 / pc[2];
    for (let py = y0; py <= y1; ++py) {
      for (let px = x0; px <= x1; ++px) {
        const cx = px + 0.5, cy = py + 0.5;
        const w0 = ((pb[0] - pa[0]) * (cy - pa[1]) - (cx - pa[0]) * (pb[1] - pa[1])) / area;
        const w1 = ((cx - pa[0]) * (pc[1] - pa[1]) - (pc[0] - pa[0]) * (cy - pa[1])) / area;
        const w2 = 1 - w0 - w1;
        if (w0 < 0 || w1 < 0 || w2 < 0) continue;
        // 1/z is the one quantity that interpolates linearly in screen space.
        const inv_z = w2 * inv_a + w1 * inv_b + w0 * inv_c;
        const at = py * w + px;
        if (inv_z <= depth[at]) continue;
        depth[at] = inv_z;
        colour[at * 3] = r; colour[at * 3 + 1] = g; colour[at * 3 + 2] = b;
      }
    }
  }

  // Box-downsample the supersampled buffer, then tone-map and gamma-encode.
  const out = new Uint8ClampedArray(width * height * 4);
  const n = SS * SS;
  for (let y = 0; y < height; ++y) {
    for (let x = 0; x < width; ++x) {
      let r = 0, g = 0, b = 0;
      for (let sy = 0; sy < SS; ++sy) {
        for (let sx = 0; sx < SS; ++sx) {
          const at = ((y * SS + sy) * w + (x * SS + sx)) * 3;
          r += colour[at]; g += colour[at + 1]; b += colour[at + 2];
        }
      }
      const at = (y * width + x) * 4;
      // Reinhard shoulder plus a 1/2.2 encode: the same exposure feel as the prototype's ACES pass,
      // without pretending this is a film curve.
      const map = (v) => Math.round(255 * Math.pow((v / n) / (1 + v / n) * 1.42, 1 / 2.2));
      out[at] = map(r); out[at + 1] = map(g); out[at + 2] = map(b); out[at + 3] = 255;
    }
  }
  return { data: out, width, height };
}

const args = process.argv.slice(2);
let width = 1500, height = 940;
let views = Object.keys(VIEWS);
const sources = [];
for (let i = 0; i < args.length; ++i) {
  if (args[i] === '--view') views = args[++i].split(',');
  else if (args[i] === '--size') { const [a, b] = args[++i].split('x'); width = +a; height = +b; }
  else sources.push(args[i]);
}
await mkdir('out', { recursive: true });
for (const source of sources) {
  const module = await import(pathToFileURL(source).href);
  const name = module.meta?.name ?? source;
  for (const view of views) {
    const image = render(module.build(), view, width, height);
    const file = `out/${name}_${view}.png`;
    await writeFile(file, encode_png(image.data, image.width, image.height));
    console.log(file);
  }
}
