// Shared geometry helpers and the material palette. The palette of record is what the references
// actually contain (plan-04 A2, s3.5): board and hulls both came back greyer and less blue-green
// than the prompts asked, so the materials moved to the observed values. The UI inks in
// ui/tokens.h follow the same bands (plan-04 A2), so ships and HUD belong to one world.
// Copied verbatim from AstraWars
// src/models.ts (box, cylinder, hull, palette) and src/fleet.ts (plate, drive, thrusters).
// Axes: nose +Y, dorsal +Z, starboard +X. Units are metres.
import * as THREE from 'three';
import { RoundedBoxGeometry } from 'three/addons/geometries/RoundedBoxGeometry.js';
import * as zlib from 'node:zlib';

export const armor = new THREE.MeshStandardMaterial({ color: '#a3a7a4', roughness: 0.64, metalness: 0.55 });
export const lightArmor = new THREE.MeshStandardMaterial({ color: '#ededeA', roughness: 0.52, metalness: 0.4 });
export const dark = new THREE.MeshStandardMaterial({ color: '#263c43', roughness: 0.7, metalness: 0.85 });
export const metal = new THREE.MeshStandardMaterial({ color: '#646a6c', roughness: 0.5, metalness: 0.86 });
export const copper = new THREE.MeshStandardMaterial({ color: '#a56233', roughness: 0.65, metalness: 0.6 });
export const black = new THREE.MeshStandardMaterial({ color: '#171b1e', roughness: 0.7, metalness: 0.5 });
export const glass = new THREE.MeshStandardMaterial({ color: '#376c7c', roughness: 0.25, metalness: 0.7, emissive: '#306675', emissiveIntensity: 0.6 });
export const oreShell = new THREE.MeshStandardMaterial({ color: '#6a6258', roughness: 0.95, metalness: 0.12 });
export const oreVein = new THREE.MeshBasicMaterial({ color: '#efb879' });
for (const material of [armor, lightArmor, dark, metal, copper, black, glass, oreShell, oreVein]) material.userData.shared = true;

// Fleet geometry is authored in metres, nose +Y, dorsal +Z. Static plates are batched by material;
// engines and RCS remain separate so their transforms can follow the simulation.
export const teal = new THREE.MeshStandardMaterial({ color: '#275459', metalness: 0.55, roughness: 0.42 });
export const ochre = new THREE.MeshStandardMaterial({ color: '#ae7040', metalness: 0.45, roughness: 0.68 });
export const ceramic = new THREE.MeshStandardMaterial({ color: '#8299a9', metalness: 0.65, roughness: 0.35 });
export const exhaust = new THREE.ShaderMaterial({
  transparent: true, depthWrite: false, blending: THREE.AdditiveBlending, side: THREE.DoubleSide,
  vertexShader: 'varying vec2 vUv; void main(){vUv=uv;gl_Position=projectionMatrix*modelViewMatrix*vec4(position,1.);}',
  fragmentShader: 'varying vec2 vUv; void main(){float a=pow(1.-vUv.y,1.65);float edge=pow(sin(vUv.x*3.14159),.4);gl_FragColor=vec4(mix(vec3(.12,.38,1.),vec3(.78,.97,1.),a),a*edge*.8);}',
});
export const glow = new THREE.MeshBasicMaterial({ color: '#a3e9ff', toneMapped: false });
export const jetMaterial = new THREE.MeshBasicMaterial({ color: '#c2e9ff', transparent: true, opacity: 0.6, blending: THREE.AdditiveBlending, depthWrite: false });
for (const material of [teal, ochre, ceramic, exhaust, glow, jetMaterial]) material.userData.shared = true;

export function box(parent: THREE.Object3D, material: THREE.Material, size: number[], pos: number[], rotation = 0) {
  const mesh = new THREE.Mesh(new THREE.BoxGeometry(size[0], size[1], size[2]), material);
  mesh.position.set(pos[0], pos[1], pos[2]);
  mesh.rotation.z = rotation;
  mesh.castShadow = true;
  mesh.receiveShadow = true;
  parent.add(mesh);
  return mesh;
}

export function cylinder(parent: THREE.Object3D, material: THREE.Material, top: number, bottom: number, height: number, pos: number[], segments = 12) {
  const mesh = new THREE.Mesh(new THREE.CylinderGeometry(top, bottom, height, segments), material);
  mesh.position.set(pos[0], pos[1], pos[2]);
  mesh.castShadow = true;
  parent.add(mesh);
  return mesh;
}

export function hull(parent: THREE.Object3D, width: number, length: number, depth: number, material: THREE.Material, x = 0, y = 0, z = 0) {
  const shape = new THREE.Shape();
  shape.moveTo(-width * 0.34, -length / 2);
  shape.lineTo(-width / 2, -length * 0.3);
  shape.lineTo(-width / 2, length * 0.18);
  shape.lineTo(-width * 0.22, length / 2);
  shape.lineTo(width * 0.22, length / 2);
  shape.lineTo(width / 2, length * 0.18);
  shape.lineTo(width / 2, -length * 0.3);
  shape.lineTo(width * 0.34, -length / 2);
  shape.closePath();
  const geometry = new THREE.ExtrudeGeometry(shape, { depth, bevelEnabled: true, bevelSize: 1.4, bevelThickness: 1.2, bevelSegments: 1, steps: 1 });
  const mesh = new THREE.Mesh(geometry, material);
  mesh.position.set(x, y, z - depth / 2);
  mesh.castShadow = true;
  mesh.receiveShadow = true;
  parent.add(mesh);
  return mesh;
}

/** Extruded outline plate: the fleet ships are built from these. */
export function plate(root: THREE.Group, outline: number[][], depth: number, z: number, material: THREE.Material) {
  const shape = new THREE.Shape(outline.map(([x, y]) => new THREE.Vector2(x, y)));
  const geometry = new THREE.ExtrudeGeometry(shape, { depth, bevelEnabled: true, bevelSize: 0.7, bevelThickness: 0.6, bevelSegments: 1 });
  const mesh = new THREE.Mesh(geometry, material);
  mesh.position.z = z; root.add(mesh);
}

/** One engine bell plus its authoring-hidden flame cone. */
export function drive(root: THREE.Group, x: number, y: number, radius: number, flames: THREE.Mesh[]) {
  // Segment counts and thicknesses hold to plan 05 s3.2: the nozzle furniture is 1.5 m thick at
  // the thinnest, and eight segments turn a bell for a twentieth of the old triangle cost.
  cylinder(root, metal, radius * 0.65, radius, 13, [x, y + 4, 0], 8);
  cylinder(root, dark, radius * 0.9, radius * 1.1, 6, [x, y - 5, 0], 8);
  cylinder(root, black, radius, radius, 1.5, [x, y - 8.2, 0], 8);
  cylinder(root, glow, radius * 0.72, radius * 0.72, 1.5, [x, y - 9.0, 0], 8);
  // The base is at the nozzle; scaling length never pulls the flame off its engine.
  const geometry = new THREE.ConeGeometry(radius * 0.83, 36, 8, 1, true);
  geometry.rotateZ(Math.PI); geometry.translate(0, -18, 0);
  const flame = new THREE.Mesh(geometry, exhaust);
  flame.name = 'flame'; flame.userData.effect = true;
  flame.position.set(x, y - 9, 0); flame.visible = false;
  root.add(flame); flames.push(flame);
}

/** Docking port anchor: named `dock.<id>`, its local +Y facing out of the berth. No geometry, so
 *  nothing draws it — the exporter carries it to the sidecar as a port (PLAN-02 §5.2).
 *  `tilt` rotates about X before the Z rotation, which is how a port faces local up: the exporter's
 *  planar `normal` then reads zero (there is no in-plane component) and `normal3` carries [0,0,1]. */
export function dock(root: THREE.Object3D, id: string, pos: number[], rotation = 0, cls = 'M', tilt = 0) {
  const port = new THREE.Object3D();
  port.name = `dock.${id}`;
  port.position.set(pos[0], pos[1], pos[2]);
  port.rotation.set(tilt, 0, rotation);
  port.userData.class = cls;
  root.add(port);
  return port;
}

/** Four RCS pods with their authoring-hidden jet cones. */
export function thrusters(root: THREE.Group, width: number, front: number, back: number, rcs: THREE.Mesh[]) {
  for (const side of [-1, 1]) for (const y of [front, back]) {
    box(root, dark, [3.3, 4.5, 3], [side * width, y, 2]);
    box(root, metal, [0.7, 3, 3.2], [side * (width + 1.8), y, 2]);
    const geometry = new THREE.ConeGeometry(1.2, 9, 6);
    geometry.rotateZ(-side * Math.PI / 2); geometry.translate(side * 4.5, 0, 0);
    const jet = new THREE.Mesh(geometry, jetMaterial);
    jet.position.set(side * (width + 2), y, 2); jet.visible = false;
    jet.name = 'rcs-jet'; jet.userData.effect = true;
    root.add(jet); rcs.push(jet);
  }
}

// =================================================================================================
// F8 additions (PLAN-03 §7): a canvas-like raster for the procedural maps, mid-poly part builders,
// and the map kit every module composes.
// =================================================================================================

/**
 * Bun ships no canvas (no `document`, no `OffscreenCanvas`, verified), so the maps are drawn with
 * this: a `Uint8ClampedArray` with the slice of the 2D canvas API the brief calls for - lines,
 * rectangles, arcs, polygon fills, and noise. Colours are `#rrggbb` / `#rgb` / `[r,g,b(,a)]`.
 * Coordinates are canvas pixels, y down; the exporter maps UV v=0 to the first row, so what a
 * module draws at y=0 is what glTF calls v=0 and nothing flips (uploads and mipmaps included).
 */
export type Color = string | number[] | readonly number[];

function parse_color(value: Color): [number, number, number, number] {
  if (Array.isArray(value)) {
    const [r, g, b, a = 1] = value as number[];
    return [r, g, b, Math.max(0, Math.min(1, a)) * 255];
  }
  const text = String(value).trim();
  if (text.startsWith('#')) {
    const hex = text.slice(1);
    const narrow = hex.length === 3;
    const at = (index: number) => {
      const pair = narrow ? hex[index] + hex[index] : hex.slice(index * 2, index * 2 + 2);
      return parseInt(pair, 16);
    };
    return [at(0), at(1), at(2), 255];
  }
  const parts = text.replace(/[^0-9.,]/g, ' ').trim().split(/[\s,]+/).map(Number);
  return [parts[0] ?? 0, parts[1] ?? 0, parts[2] ?? 0, (parts[3] ?? 1) * 255];
}

/** Integer hash -> [0,1). The only randomness in the pipeline, and it is seeded, never `Math.random`. */
function hash2(x: number, y: number, seed: number): number {
  let h = (x | 0) * 0x27d4eb2d ^ (y | 0) * 0x165667b1 ^ (seed | 0) * 0x9e3779b1;
  h = Math.imul(h ^ (h >>> 15), 0x85ebca6b);
  h = Math.imul(h ^ (h >>> 13), 0xc2b2ae35);
  return ((h ^ (h >>> 16)) >>> 0) / 4294967296;
}

/** Deterministic PRNG for scattering (rivet jitter, damage). Same seed, same layout, forever. */
export function rng(seed: number): () => number {
  let state = (seed | 0) || 1;
  return () => {
    state = (state + 0x6d2b79f5) | 0;
    let t = Math.imul(state ^ (state >>> 15), 1 | state);
    t = (t + Math.imul(t ^ (t >>> 7), 61 | t)) ^ t;
    return ((t ^ (t >>> 14)) >>> 0) / 4294967296;
  };
}

/** Value noise on a unit lattice; smooth, tileable enough at these scales. */
export function noise(x: number, y: number, seed = 0): number {
  const xi = Math.floor(x), yi = Math.floor(y);
  const ease = (t: number) => t * t * (3 - 2 * t);                    // smoothstep: no lattice creases
  const xf = ease(x - xi), yf = ease(y - yi);
  const a = hash2(xi, yi, seed), b = hash2(xi + 1, yi, seed);
  const c = hash2(xi, yi + 1, seed), d = hash2(xi + 1, yi + 1, seed);
  return (a + (b - a) * xf) * (1 - yf) + (c + (d - c) * xf) * yf;
}

/** Summed octaves: the same shape PLAN-03 §3.2 gives the terrain, at texture scale. */
export function fbm(x: number, y: number, octaves = 4, seed = 0): number {
  let sum = 0, amplitude = 0.5, total = 0, fx = x, fy = y;
  for (let i = 0; i < octaves; i++) {
    sum += noise(fx, fy, seed + i * 131) * amplitude;
    total += amplitude;
    amplitude *= 0.5;
    fx *= 2.03; fy *= 2.03;
  }
  return sum / total;
}

export interface DrawState {
  a: number; b: number; c: number; d: number; e: number; f: number;
  fillStyle: Color; strokeStyle: Color; lineWidth: number; alpha: number;
}

/** The subset of CanvasRenderingContext2D the map kit uses, over a straight RGBA buffer. */
export class RasterContext {
  readonly data: Uint8ClampedArray;
  readonly width: number;
  readonly height: number;
  fillStyle: Color = '#000000';
  strokeStyle: Color = '#000000';
  lineWidth = 1;
  globalAlpha = 1;
  private stack: DrawState[] = [];
  private path: number[][][] = [];
  private current: number[][] | null = null;
  private coverage = new Float32Array(0);
  private touched: number[] = [];
  private m = { a: 1, b: 0, c: 0, d: 1, e: 0, f: 0 };

  constructor(data: Uint8ClampedArray, width: number, height: number) {
    this.data = data; this.width = width; this.height = height;
  }

  save(): void {
    this.stack.push({
      ...this.m, fillStyle: this.fillStyle, strokeStyle: this.strokeStyle,
      lineWidth: this.lineWidth, alpha: this.globalAlpha,
    });
  }

  restore(): void {
    const state = this.stack.pop();
    if (!state) return;
    this.m = { a: state.a, b: state.b, c: state.c, d: state.d, e: state.e, f: state.f };
    this.fillStyle = state.fillStyle; this.strokeStyle = state.strokeStyle;
    this.lineWidth = state.lineWidth; this.globalAlpha = state.alpha;
  }

  translate(x: number, y: number): void {
    this.m = multiply(this.m, { a: 1, b: 0, c: 0, d: 1, e: x, f: y });
  }

  scale(x: number, y: number): void {
    this.m = multiply(this.m, { a: x, b: 0, c: 0, d: y, e: 0, f: 0 });
  }

  rotate(angle: number): void {
    const cos = Math.cos(angle), sin = Math.sin(angle);
    this.m = multiply(this.m, { a: cos, b: sin, c: -sin, d: cos, e: 0, f: 0 });
  }

  /** Unit scale of the current transform, for turning a user-space line width into pixels. */
  private get unit(): number {
    return Math.sqrt(Math.abs(this.m.a * this.m.d - this.m.b * this.m.c)) || 1;
  }

  private at(x: number, y: number): number[] {
    return [this.m.a * x + this.m.c * y + this.m.e, this.m.b * x + this.m.d * y + this.m.f];
  }

  beginPath(): void {
    this.path = []; this.current = null;
  }

  moveTo(x: number, y: number): void {
    this.current = [this.at(x, y)];
    this.path.push(this.current);
  }

  lineTo(x: number, y: number): void {
    if (!this.current) { this.moveTo(x, y); return; }
    this.current.push(this.at(x, y));
  }

  closePath(): void {
    if (this.current && this.current.length > 1) this.current.push([...this.current[0]]);
  }

  rect(x: number, y: number, w: number, h: number): void {
    this.moveTo(x, y); this.lineTo(x + w, y); this.lineTo(x + w, y + h);
    this.lineTo(x, y + h); this.closePath();
  }

  arc(cx: number, cy: number, radius: number, start = 0, end = Math.PI * 2): void {
    const steps = Math.max(8, Math.ceil(Math.abs(end - start) / (Math.PI / 16)));
    for (let i = 0; i <= steps; i++) {
      const angle = start + (end - start) * (i / steps);
      const x = cx + Math.cos(angle) * radius, y = cy + Math.sin(angle) * radius;
      if (i === 0 && !this.current) this.moveTo(x, y); else this.lineTo(x, y);
    }
  }

  fill(): void {
    const [r, g, b, alpha] = parse_color(this.fillStyle);
    const a = (alpha / 255) * this.globalAlpha;
    for (const subpath of this.path) {
      if (subpath.length >= 3) this.scanline(subpath, [r, g, b], a);
    }
  }

  stroke(): void {
    const [r, g, b, alpha] = parse_color(this.strokeStyle);
    const a = (alpha / 255) * this.globalAlpha;
    const half = Math.max(0.5, (this.lineWidth * this.unit) / 2);
    for (const subpath of this.path) {
      for (let i = 0; i + 1 < subpath.length; i++) {
        const [x0, y0] = subpath[i], [x1, y1] = subpath[i + 1];
        const dx = x1 - x0, dy = y1 - y0;
        const len = Math.hypot(dx, dy);
        if (len < 1e-6) continue;
        const nx = (-dy / len) * half, ny = (dx / len) * half;
        this.scanline([[x0 + nx, y0 + ny], [x1 + nx, y1 + ny], [x1 - nx, y1 - ny], [x0 - nx, y0 - ny]], [r, g, b], a);
        this.scanline(ring(x0, y0, half), [r, g, b], a);   // round join: what a weld bead looks like
      }
      if (subpath.length === 1) this.scanline(ring(subpath[0][0], subpath[0][1], half), [r, g, b], a);
    }
  }

  fillRect(x: number, y: number, w: number, h: number): void {
    this.beginPath(); this.rect(x, y, w, h); this.fill();
  }

  strokeRect(x: number, y: number, w: number, h: number): void {
    this.beginPath(); this.rect(x, y, w, h); this.stroke();
  }

  clearRect(x: number, y: number, w: number, h: number): void {
    const [x0, y0] = this.at(x, y), [x1, y1] = this.at(x + w, y + h);
    const left = Math.max(0, Math.floor(Math.min(x0, x1))), right = Math.min(this.width, Math.ceil(Math.max(x0, x1)));
    const top = Math.max(0, Math.floor(Math.min(y0, y1))), bottom = Math.min(this.height, Math.ceil(Math.max(y0, y1)));
    for (let py = top; py < bottom; py++) this.data.fill(0, (py * this.width + left) * 4, (py * this.width + right) * 4);
  }

  /**
   * Straight-alpha source-over of one device-space span. Two vertical subsamples accumulate into a
   * coverage row that is applied once: compositing the two subsamples separately would leave a
   * fully covered pixel at 75% instead of 100% (1 - 0.5 x 0.5), which shows up as a dirty grey
   * veil over every fill.
   */
  private scanline(points: number[][], rgb: number[], alpha: number): void {
    if (alpha <= 0) return;
    if (this.coverage.length !== this.width) this.coverage = new Float32Array(this.width);
    let minY = Infinity, maxY = -Infinity;
    for (const [, y] of points) { if (y < minY) minY = y; if (y > maxY) maxY = y; }
    const first = Math.max(0, Math.floor(minY)), last = Math.min(this.height - 1, Math.ceil(maxY));
    for (let y = first; y <= last; y++) {
      this.touched.length = 0;
      for (const sy of [y + 0.25, y + 0.75]) {
        const spans: number[] = [];
        for (let i = 0; i < points.length; i++) {
          const [x0, y0] = points[i], [x1, y1] = points[(i + 1) % points.length];
          if ((y0 <= sy && y1 > sy) || (y1 <= sy && y0 > sy)) {
            spans.push(x0 + ((sy - y0) / (y1 - y0)) * (x1 - x0));
          }
        }
        spans.sort((a, b) => a - b);
        for (let i = 0; i + 1 < spans.length; i += 2) this.addCoverage(spans[i], spans[i + 1], 0.5);
      }
      const row = y * this.width * 4;
      for (const x of this.touched) {
        const a = Math.min(1, alpha * this.coverage[x]);
        const at = row + x * 4;
        this.data[at] = this.data[at] * (1 - a) + rgb[0] * a;
        this.data[at + 1] = this.data[at + 1] * (1 - a) + rgb[1] * a;
        this.data[at + 2] = this.data[at + 2] * (1 - a) + rgb[2] * a;
        this.data[at + 3] = Math.min(255, this.data[at + 3] + a * 255);
        this.coverage[x] = 0;
      }
    }
  }

  private addCoverage(x0: number, x1: number, weight: number): void {
    const left = Math.max(0, x0), right = Math.min(this.width, x1);
    if (right <= left) return;
    const from = Math.max(0, Math.ceil(left - 0.5)), to = Math.min(this.width - 1, Math.floor(right - 0.5));
    for (let x = from; x <= to; x++) {
      const part = Math.min(right, x + 0.5) - Math.max(left, x - 0.5);
      if (part <= 0) continue;
      if (this.coverage[x] === 0) this.touched.push(x);
      this.coverage[x] += part * weight;
    }
  }

  /** Writes pixels through a callback that returns [r,g,b(,a)] or null to leave the pixel alone. */
  pixels(x: number, y: number, w: number, h: number, shade: (px: number, py: number) => number[] | null): void {
    const left = Math.max(0, Math.floor(x)), top = Math.max(0, Math.floor(y));
    const right = Math.min(this.width, Math.ceil(x + w)), bottom = Math.min(this.height, Math.ceil(y + h));
    for (let py = top; py < bottom; py++) {
      for (let px = left; px < right; px++) {
        const rgb = shade(px, py);
        if (!rgb) continue;
        const a = (rgb.length > 3 ? rgb[3] : 1) * this.globalAlpha;
        const at = (py * this.width + px) * 4;
        this.data[at] = this.data[at] * (1 - a) + rgb[0] * a;
        this.data[at + 1] = this.data[at + 1] * (1 - a) + rgb[1] * a;
        this.data[at + 2] = this.data[at + 2] * (1 - a) + rgb[2] * a;
        this.data[at + 3] = Math.min(255, this.data[at + 3] + a * 255);
      }
    }
  }

  /** Device-space read, as the canvas spec says (unaffected by the current transform). */
  getImageData(x: number, y: number, w: number, h: number): ImageDataLike {
    const out = new Uint8ClampedArray(w * h * 4);
    for (let row = 0; row < h; row++) {
      const from = ((y + row) * this.width + x) * 4;
      if (y + row < 0 || y + row >= this.height) continue;
      out.set(this.data.subarray(from, from + w * 4), row * w * 4);
    }
    return { data: out, width: w, height: h };
  }

  /**
   * Replaces the pixels under `imageData`, honouring the transform the way a resized canvas does:
   * the exporter flips an image by scaling -1 about the buffer height, and this is what makes that
   * flip land in the bytes. Straight copy, nearest sample, no blending - `putImageData` semantics.
   */
  putImageData(image: ImageDataLike, dx: number, dy: number): void {
    for (let row = 0; row < image.height; row++) {
      for (let column = 0; column < image.width; column++) {
        const [x, y] = this.at(dx + column, dy + row).map(Math.round);
        if (x < 0 || y < 0 || x >= this.width || y >= this.height) continue;
        const to = (y * this.width + x) * 4;
        const from = (row * image.width + column) * 4;
        this.data[to] = image.data[from];
        this.data[to + 1] = image.data[from + 1];
        this.data[to + 2] = image.data[from + 2];
        this.data[to + 3] = image.data[from + 3];
      }
    }
  }

  /** Source-over blit of an image-like (a `DataTexture`'s image, or another raster), scaled. */
  drawImage(image: ImageDataLike, dx: number, dy: number, dw = image.width, dh = image.height): void {
    const [x0, y0] = this.at(dx, dy);
    const [x1, y1] = this.at(dx + dw, dy + dh);
    const left = Math.round(Math.min(x0, x1)), right = Math.round(Math.max(x0, x1));
    const top = Math.round(Math.min(y0, y1)), bottom = Math.round(Math.max(y0, y1));
    for (let y = top; y < bottom; y++) {
      for (let x = left; x < right; x++) {
        if (x < 0 || y < 0 || x >= this.width || y >= this.height) continue;
        const sx = Math.min(image.width - 1, Math.max(0, Math.floor(((x - left) / Math.max(1, right - left)) * image.width)));
        const sy = Math.min(image.height - 1, Math.max(0, Math.floor(((y - top) / Math.max(1, bottom - top)) * image.height)));
        const from = (sy * image.width + sx) * 4;
        const to = (y * this.width + x) * 4;
        const a = (image.data[from + 3] / 255) * this.globalAlpha;
        this.data[to] = this.data[to] * (1 - a) + image.data[from] * a;
        this.data[to + 1] = this.data[to + 1] * (1 - a) + image.data[from + 1] * a;
        this.data[to + 2] = this.data[to + 2] * (1 - a) + image.data[from + 2] * a;
        this.data[to + 3] = Math.min(255, this.data[to + 3] + a * 255);
      }
    }
  }
}

/** Shape of the canvas image payloads the raster reads: also what a `DataTexture.image` is. */
export interface ImageDataLike {
  data: Uint8ClampedArray | Uint8Array;
  width: number;
  height: number;
}

function multiply(a: DrawState, b: DrawState): DrawState {
  return {
    a: a.a * b.a + a.c * b.b, b: a.b * b.a + a.d * b.b,
    c: a.a * b.c + a.c * b.d, d: a.b * b.c + a.d * b.d,
    e: a.a * b.e + a.c * b.f + a.e, f: a.b * b.e + a.d * b.f + a.f,
  };
}

function ring(cx: number, cy: number, radius: number): number[][] {
  const points: number[][] = [];
  for (let i = 0; i < 12; i++) {
    const angle = (i / 12) * Math.PI * 2;
    points.push([cx + Math.cos(angle) * radius, cy + Math.sin(angle) * radius]);
  }
  return points;
}

export interface Raster { size: number; data: Uint8ClampedArray; ctx: RasterContext }

/** A fresh opaque black buffer with a context over it: what a `maps` function is handed. */
export function createRaster(size = 512, fill: Color = '#000000'): Raster {
  const data = new Uint8ClampedArray(size * size * 4);
  const ctx = new RasterContext(data, size, size);
  ctx.fillStyle = fill;
  ctx.fillRect(0, 0, size, size);
  return { size, data, ctx };
}

let crcTable: Uint32Array | null = null;
function crc32(bytes: Uint8Array): number {
  if (!crcTable) {
    crcTable = new Uint32Array(256);
    for (let i = 0; i < 256; i++) {
      let c = i;
      for (let bit = 0; bit < 8; bit++) c = c & 1 ? 0xedb88320 ^ (c >>> 1) : c >>> 1;
      crcTable[i] = c >>> 0;
    }
  }
  let crc = 0xffffffff;
  for (const byte of bytes) crc = crcTable[(crc ^ byte) & 0xff] ^ (crc >>> 8);
  return (crc ^ 0xffffffff) >>> 0;
}

/** Minimal PNG (IHDR/IDAT/IEND, no ancillary chunks): deterministic bytes, no timestamps. */
export function encode_png(data: Uint8ClampedArray, width: number, height: number): Uint8Array {
  const stride = width * 4;
  const raw = new Uint8Array((stride + 1) * height);
  for (let y = 0; y < height; y++) {
    raw[y * (stride + 1)] = 0;                                        // filter: none
    raw.set(data.subarray(y * stride, (y + 1) * stride), y * (stride + 1) + 1);
  }
  const deflated = zlib.deflateSync(raw, { level: 6 });
  const chunks: Uint8Array[] = [new Uint8Array([0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a])];
  const chunk = (type: string, body: Uint8Array) => {
    const out = new Uint8Array(12 + body.length);
    const view = new DataView(out.buffer);
    view.setUint32(0, body.length);
    for (let i = 0; i < 4; i++) out[4 + i] = type.charCodeAt(i);
    out.set(body, 8);
    const crcInput = new Uint8Array(4 + body.length);
    crcInput.set(out.subarray(4, 8), 0);
    crcInput.set(body, 4);
    view.setUint32(8 + body.length, crc32(crcInput));
    return out;
  };
  const header = new Uint8Array(13);
  const view = new DataView(header.buffer);
  view.setUint32(0, width); view.setUint32(4, height);
  header[8] = 8; header[9] = 6;                                       // 8-bit RGBA
  chunks.push(chunk('IHDR', header), chunk('IDAT', deflated), chunk('IEND', new Uint8Array(0)));
  const total = chunks.reduce((sum, part) => sum + part.length, 0);
  const out = new Uint8Array(total);
  let at = 0;
  for (const part of chunks) { out.set(part, at); at += part.length; }
  return out;
}

// ---- the atlas the UVs and the maps agree on ----------------------------------------------------

/**
 * One 512² map serves the whole model, so the merged meshes get *planar* UVs projected from model
 * space: three axis-aligned tiles in the top-left 2x2 quadrants (0: starboard-facing, 1: up,
 * 2: nose-facing). `export.mjs` writes the UVs from `atlas_cell`; the maps draw the same tile
 * content into each, so a panel line is the same number of metres everywhere on the hull instead
 * of being stretched per authored primitive.
 */
export const ATLAS_CELLS = [0, 1, 2];

export function atlas_cell(size: number, cell: number): { x: number; y: number; w: number; h: number } {
  const half = size / 2;
  const pad = Math.max(1, Math.round(size / 64));                    // mip bleeding stays in its tile
  return { x: (cell % 2) * half + pad, y: Math.floor(cell / 2) * half + pad, w: half - 2 * pad, h: half - 2 * pad };
}

/** A tile: the pixel rect of one atlas cell, with (0,0) at its own top-left corner. */
export interface Tile {
  ctx: RasterContext; x: number; y: number; w: number; h: number; cell: number; size: number;
}

export function each_tile(ctx: RasterContext, size: number, fn: (tile: Tile) => void): void {
  for (const cell of ATLAS_CELLS) {
    const rect = atlas_cell(size, cell);
    ctx.save();
    ctx.translate(rect.x, rect.y);
    fn({ ctx, x: 0, y: 0, w: rect.w, h: rect.h, cell, size });
    ctx.restore();
  }
}

// ---- the map kit --------------------------------------------------------------------------------

/** A groove across a tile: two lips whose normals lean toward each other. Reads as a panel line. */
export function panel_grid(tile: Tile, opts: { pitch?: number; width?: number; lean?: number; color?: Color; normal?: boolean; seed?: number } = {}): void {
  const { ctx } = tile;
  const pitch = opts.pitch ?? 0.14;
  const width = opts.width ?? 0.006;
  const lean = opts.lean ?? 44;
  const color = opts.color ?? '#c9cfd1';
  const lines = Math.max(1, Math.round(1 / pitch));
  ctx.lineWidth = Math.max(1, width * tile.w);
  for (let i = 0; i <= lines; i++) {
    const at = i / lines;
    const jitter = (hash2(i * 977, 31, opts.seed ?? 3) - 0.5) * pitch * 0.25;
    if (opts.normal) {
      ctx.strokeStyle = [128 - lean, 128, 255]; ctx.beginPath();
      ctx.moveTo(at * tile.w - ctx.lineWidth, 0); ctx.lineTo(at * tile.w - ctx.lineWidth, tile.h); ctx.stroke();
      ctx.strokeStyle = [128 + lean, 128, 255]; ctx.beginPath();
      ctx.moveTo(at * tile.w + ctx.lineWidth, 0); ctx.lineTo(at * tile.w + ctx.lineWidth, tile.h); ctx.stroke();
    }
    ctx.strokeStyle = color;
    ctx.beginPath();
    ctx.moveTo(at * tile.w, 0); ctx.lineTo(at * tile.w, tile.h); ctx.stroke();
    const row = Math.min(1, Math.max(0, at + jitter));
    if (opts.normal) {
      ctx.strokeStyle = [128, 128 - lean, 255]; ctx.beginPath();
      ctx.moveTo(0, row * tile.h - ctx.lineWidth); ctx.lineTo(tile.w, row * tile.h - ctx.lineWidth); ctx.stroke();
      ctx.strokeStyle = [128, 128 + lean, 255]; ctx.beginPath();
      ctx.moveTo(0, row * tile.h + ctx.lineWidth); ctx.lineTo(tile.w, row * tile.h + ctx.lineWidth); ctx.stroke();
    }
    ctx.strokeStyle = color;
    ctx.beginPath();
    ctx.moveTo(0, row * tile.h); ctx.lineTo(tile.w, row * tile.h); ctx.stroke();
  }
}

/** Rivet rows along the panel edges: small domes in the normal map, dots in the albedo. */
export function rivets(tile: Tile, opts: { pitch?: number; radius?: number; color?: Color; normal?: boolean; seed?: number } = {}): void {
  const { ctx } = tile;
  const pitch = opts.pitch ?? 0.07;
  const radius = (opts.radius ?? 0.008) * tile.w;
  const color = opts.color ?? '#aeb6b8';
  const count = Math.max(1, Math.round(1 / pitch));
  for (let i = 0; i <= count; i++) {
    for (let j = 0; j <= count; j++) {
      const x = (i / count) * tile.w, y = (j / count) * tile.h;
      if (opts.normal) {
        ctx.fillStyle = [128 + 30, 128 + 22, 255];
        ctx.beginPath(); ctx.arc(x - radius * 0.3, y - radius * 0.3, radius, 0, Math.PI * 2); ctx.fill();
        ctx.fillStyle = [128 - 26, 128 - 20, 255];
        ctx.beginPath(); ctx.arc(x + radius * 0.3, y + radius * 0.3, radius * 0.8, 0, Math.PI * 2); ctx.fill();
      } else {
        ctx.fillStyle = color;
        ctx.beginPath(); ctx.arc(x, y, radius * 0.75, 0, Math.PI * 2); ctx.fill();
      }
    }
  }
}

/** Beaded weld seams: a ridge in the normal map, a pale streak in the albedo, a rough one in wear. */
export function weld_seams(tile: Tile, opts: { count?: number; width?: number; bead?: number; color?: Color; normal?: boolean; seed?: number; axis?: 'x' | 'y' } = {}): void {
  const { ctx } = tile;
  const count = opts.count ?? 3;
  const width = (opts.width ?? 0.02) * tile.w;
  const bead = (opts.bead ?? 0.014) * tile.w;
  const color = opts.color ?? '#9aa3a6';
  for (let i = 0; i < count; i++) {
    const wander = (hash2(i * 53, 7, opts.seed ?? 11) - 0.5) * 0.3;
    const at = (i + 0.5 + wander * 0.5) / count;
    ctx.lineWidth = width;
    if (opts.normal) {
      ctx.strokeStyle = [128 - 52, 128, 255]; ctx.beginPath();
      if ((opts.axis ?? 'x') === 'x') { ctx.moveTo(0, at * tile.h - width); ctx.lineTo(tile.w, at * tile.h - width); }
      else { ctx.moveTo(at * tile.w - width, 0); ctx.lineTo(at * tile.w - width, tile.h); }
      ctx.stroke();
      ctx.strokeStyle = [128 + 52, 128, 255]; ctx.beginPath();
      if ((opts.axis ?? 'x') === 'x') { ctx.moveTo(0, at * tile.h + width); ctx.lineTo(tile.w, at * tile.h + width); }
      else { ctx.moveTo(at * tile.w + width, 0); ctx.lineTo(at * tile.w + width, tile.h); }
      ctx.stroke();
    }
    ctx.strokeStyle = color;
    ctx.lineWidth = Math.max(1, bead);
    ctx.beginPath();
    if ((opts.axis ?? 'x') === 'x') { ctx.moveTo(0, at * tile.h); ctx.lineTo(tile.w, at * tile.h); }
    else { ctx.moveTo(at * tile.w, 0); ctx.lineTo(at * tile.w, tile.h); }
    ctx.stroke();
  }
}

/** Hazard stripes, at whatever rect and angle the module wants them. */
export function hazard_stripes(tile: Tile, rect: { x: number; y: number; w: number; h: number }, opts: { pitch?: number; colors?: Color[]; angle?: number; alpha?: number } = {}): void {
  const { ctx } = tile;
  const colors = opts.colors ?? ['#c8a33c', '#20232a'];
  const pitch = (opts.pitch ?? 0.05) * tile.w;
  ctx.save();
  ctx.globalAlpha = opts.alpha ?? 1;
  ctx.translate(rect.x * tile.w, rect.y * tile.h);
  ctx.rotate(opts.angle ?? -Math.PI / 4);
  const span = Math.hypot(rect.w * tile.w, rect.h * tile.h);
  for (let i = -4; i * pitch < span * 2; i++) {
    ctx.fillStyle = colors[Math.abs(i) % colors.length];
    ctx.fillRect(i * pitch, -span, pitch / 2, span * 2);
  }
  ctx.restore();
}

/** 3x5 stencil glyphs, mirrored like a real spray mask. Enough for "K-7", "FUEL", "DANGER". */
const GLYPHS: Record<string, string> = {
  A: '25755', B: '65656', C: '34443', D: '65556', E: '74647', F: '74644', G: '34553', H: '55755',
  I: '72227', J: '11152', K: '55655', L: '44447', M: '57755', N: '57555', O: '25552', P: '65644',
  Q: '25563', R: '65655', S: '34216', T: '72222', U: '55557', V: '55552', W: '55775', X: '55255',
  Y: '55222', Z: '71247', 0: '75557', 1: '26227', 2: '71747', 3: '71317', 4: '55711', 5: '74616',
  6: '34757', 7: '71222', 8: '75757', 9: '75716', '-': '00700', '.': '00002', '/': '11244', ' ': '00000',
};

export function stencil_text(tile: Tile, text: string, opts: { x: number; y: number; scale?: number; color?: Color; alpha?: number; mirror?: boolean } = { x: 0.5, y: 0.5 }): void {
  const { ctx } = tile;
  const cell = (opts.scale ?? 0.03) * tile.w;
  const color = opts.color ?? '#3a4046';
  if (opts.mirror) { ctx.save(); ctx.translate((opts.x * 2) * tile.w, 0); ctx.scale(-1, 1); }
  ctx.fillStyle = color;
  ctx.globalAlpha = opts.alpha ?? 1;
  let pen = opts.x * tile.w;
  for (const raw of text.toUpperCase()) {
    const glyph = GLYPHS[raw] ?? GLYPHS['-'];
    for (let row = 0; row < 5; row++) {
      const bits = Number(glyph[row]);
      for (let col = 0; col < 3; col++) {
        if ((bits >> (2 - col)) & 1) ctx.fillRect(pen + col * cell, opts.y * tile.h + row * cell, cell * 0.92, cell * 0.92);
      }
    }
    pen += cell * 4;
  }
  if (opts.mirror) ctx.restore();
  ctx.globalAlpha = 1;
}

/** A soot blotch: noise-thresholded radial falloff, densest at the centre. */
export function scorch(tile: Tile, opts: { x: number; y: number; radius: number; seed?: number; color?: Color; alpha?: number } = { x: 0.5, y: 0.5, radius: 0.2 }): void {
  const { ctx } = tile;
  const [r, g, b] = parse_color(opts.color ?? '#141210');
  const peak = opts.alpha ?? 0.85;
  const seed = opts.seed ?? 5;
  const cx = opts.x * tile.w, cy = opts.y * tile.h, radius = opts.radius * tile.w;
  ctx.pixels(cx - radius, cy - radius, radius * 2, radius * 2, (px, py) => {
    const d = Math.hypot(px - cx, py - cy) / radius;
    if (d >= 1) return null;
    const grain = fbm(px * 0.09, py * 0.09, 4, seed) * 0.7 + fbm(px * 0.4, py * 0.4, 2, seed + 9) * 0.3;
    const density = (1 - d) ** 1.6 * (grain * 1.35 - 0.35);
    if (density <= 0.02) return null;
    return [r, g, b, Math.min(peak, Math.round(density * 7) / 7)];
  });
}

/** Paint mottling and grime: a low-frequency wash between two colours. */
export function noise_wash(tile: Tile, opts: { color?: Color; alpha?: number; scale?: number; octaves?: number; seed?: number; bias?: number; rect?: { x: number; y: number; w: number; h: number } } = {}): void {
  const { ctx } = tile;
  const [r, g, b] = parse_color(opts.color ?? '#2a2c2e');
  const scale = opts.scale ?? 0.12;
  const seed = opts.seed ?? 21;
  const rect = opts.rect ?? { x: 0, y: 0, w: 1, h: 1 };
  const peak = opts.alpha ?? 0.25;
  const bias = opts.bias ?? 0.45;
  ctx.pixels(rect.x * tile.w, rect.y * tile.h, rect.w * tile.w, rect.h * tile.h, (px, py) => {
    const value = fbm(px * scale, py * scale, opts.octaves ?? 2, seed);
    const amount = (value - bias) * 1.6 * peak;
    if (amount <= 0.004) return null;
    return [r, g, b, Math.min(peak, Math.round(amount * 6) / 6)];    // 6 levels: PNG-friendly, still mottled
  });
}

/** Edge wear and scuffs: bright grit in the albedo, rougher grit in the roughness. */
export function wear_speckle(tile: Tile, opts: { color?: Color; alpha?: number; density?: number; seed?: number; scale?: number } = {}): void {
  const { ctx } = tile;
  const [r, g, b] = parse_color(opts.color ?? '#d8d4c8');
  const scale = opts.scale ?? 0.14;
  const seed = opts.seed ?? 33;
  const gate = opts.density ?? 0.22;
  const peak = opts.alpha ?? 0.5;
  ctx.pixels(0, 0, tile.w, tile.h, (px, py) => {
    const value = fbm(px * scale, py * scale, 2, seed);
    if (value < 1 - gate) return null;
    return [r, g, b, Math.min(peak, Math.round(((value - (1 - gate)) / gate) * peak * 5) / 5)];
  });
}

/** What a module exports as `maps`: any subset, drawn into a `size` x `size` buffer. */
export interface Maps {
  normal?: (ctx: RasterContext, size: number) => void;
  roughness?: (ctx: RasterContext, size: number) => void;
  albedo?: (ctx: RasterContext, size: number) => void;
}

export interface MapOptions {
  seed?: number;
  panel?: number;            // panel pitch, tile fraction; 0 to skip the grid
  rivets?: number;           // rivet pitch; 0 to skip
  seams?: number;            // weld seam count
  hazard?: { x: number; y: number; w: number; h: number; angle?: number };
  stencils?: [string, number, number][];
  scorch?: number;           // blotch count
  damage?: number;           // torn-panel fraction of the normal/albedo, 0..1
  base?: Color;              // albedo base; near-white keeps the material's own colour
  roughness?: number;        // base roughness in the green channel (multiplies the material factor)
  wear?: number;             // wear speckle density
  grime?: number;            // mottling strength
}

/**
 * The shared map kit: panels, rivets, welds and wear; a module adds its own stencils, hazard bands
 * and damage through the options. Modules that need more draw their own functions instead - the
 * `maps` contract is just the three callbacks.
 */
export function standard_maps(options: MapOptions = {}): Maps {
  const seed = options.seed ?? 7;
  const panel = options.panel ?? 0.16;
  const rivet = options.rivets ?? 0.08;
  const seams = options.seams ?? 2;
  const rough = options.roughness ?? 0.92;

  const normal = (ctx: RasterContext, size: number) => {
    each_tile(ctx, size, (tile) => {
      tile.ctx.fillStyle = [128, 128, 255];
      tile.ctx.fillRect(0, 0, tile.w, tile.h);
      noise_wash(tile, { color: [140, 132, 255], alpha: 0.35, scale: 0.2, seed: seed + 4 });
      if (seams > 0) weld_seams(tile, { count: seams, normal: true, seed: seed + 2 });
      if (panel > 0) panel_grid(tile, { pitch: panel, normal: true, seed: seed + 3 });
      if (rivet > 0) rivets(tile, { pitch: rivet, normal: true, seed: seed + 5 });
      if (options.damage) damage_relief(tile, { amount: options.damage, seed: seed + 17, normal: true });
    });
  };

  const roughness = (ctx: RasterContext, size: number) => {
    each_tile(ctx, size, (tile) => {
      rounded_fill(tile, rough);
      if (options.grime) noise_wash(tile, { color: '#9a9a9a', alpha: options.grime, scale: 0.05, seed: seed + 12 });
      if (panel > 0) panel_grid(tile, { pitch: panel, width: 0.004, color: '#f2f2f2', seed: seed + 3 });
      if (seams > 0) weld_seams(tile, { count: seams, width: 0.014, bead: 0.02, color: '#e8e8e8', seed: seed + 2 });
      if (rivet > 0) rivets(tile, { pitch: rivet, radius: 0.006, color: '#dcdcdc', seed: seed + 5 });
      wear_speckle(tile, { color: '#ffffff', density: options.wear ?? 0.2, alpha: 0.55, seed: seed + 8 });
      if (options.damage) damage_relief(tile, { amount: options.damage, seed: seed + 17, roughness: true });
    });
  };

  const albedo = (ctx: RasterContext, size: number) => {
    each_tile(ctx, size, (tile) => {
      const [r, g, b] = parse_color(options.base ?? '#f2f0ea');
      tile.ctx.fillStyle = [r, g, b];
      tile.ctx.fillRect(0, 0, tile.w, tile.h);
      noise_wash(tile, { color: '#6d6a62', alpha: options.grime ?? 0.18, scale: 0.08, seed: seed + 9 });
      wear_speckle(tile, { color: '#e6e2d6', density: options.wear ?? 0.2, alpha: 0.35, seed: seed + 8, scale: 0.7 });
      if (seams > 0) weld_seams(tile, { count: seams, width: 0.014, bead: 0.016, color: '#b9b3a6', seed: seed + 2 });
      if (panel > 0) panel_grid(tile, { pitch: panel, width: 0.005, color: '#a9a49a', seed: seed + 3 });
      if (rivet > 0) rivets(tile, { pitch: rivet, radius: 0.006, color: '#cbc6ba', seed: seed + 5 });
      if (options.hazard) hazard_stripes(tile, options.hazard, { angle: options.hazard.angle });
      for (const [text, x, y] of options.stencils ?? []) stencil_text(tile, text, { x, y, scale: 0.035, color: '#31363b' });
      for (let i = 0; i < (options.scorch ?? 0); i++) {
        const spot = hash2(i * 71 + 13, 5, seed + 11);
        const spot2 = hash2(i * 29 + 3, 91, seed + 23);
        scorch(tile, { x: 0.15 + spot * 0.7, y: 0.15 + spot2 * 0.7, radius: 0.1 + spot * 0.12, seed: seed + i * 7, alpha: 0.7 });
      }
      if (options.damage) damage_relief(tile, { amount: options.damage, seed: seed + 17, albedo: true });
    });
  };
  return { normal, roughness, albedo };
}

/** Fill with a mid grey carrying the roughness value in green; blue stays 255 so metalness holds. */
function rounded_fill(tile: Tile, value: number): void {
  const level = Math.max(0, Math.min(255, Math.round(value * 255)));
  tile.ctx.fillStyle = [0, level, 255];
  tile.ctx.fillRect(0, 0, tile.w, tile.h);
}

/**
 * Damage: bright torn edges in the normal map, bare metal and shadowed rents in the albedo. Pure
 * geometry-free, so a wreck reads as wrecked from any angle the texture atlas can see.
 */
export function damage_relief(tile: Tile, opts: { amount: number; seed?: number; normal?: boolean; albedo?: boolean; roughness?: boolean }): void {
  const { ctx } = tile;
  const seed = opts.seed ?? 1;
  const count = Math.max(1, Math.round(opts.amount * 14));
  for (let i = 0; i < count; i++) {
    const cx = hash2(i * 101 + 9, 17, seed) * tile.w;
    const cy = hash2(i * 37 + 4, 61, seed + 3) * tile.h;
    const size = (0.05 + hash2(i * 13, 7, seed + 5) * 0.16) * tile.w;
    const points: number[][] = [];
    const corners = 5 + Math.floor(hash2(i * 19, 23, seed + 9) * 4);
    for (let k = 0; k < corners; k++) {
      const angle = (k / corners) * Math.PI * 2;
      const reach = size * (0.45 + hash2(i * 31 + k, 3, seed + 13) * 0.85);
      points.push([cx + Math.cos(angle) * reach, cy + Math.sin(angle) * reach]);
    }
    if (opts.normal) {
      ctx.strokeStyle = [128 + 74, 128 + 30, 255]; ctx.lineWidth = Math.max(1, size * 0.09);
      ctx.beginPath();
      points.forEach(([x, y], index) => (index ? ctx.lineTo(x, y) : ctx.moveTo(x, y)));
      ctx.closePath(); ctx.stroke();
    }
    if (opts.albedo) {
      ctx.fillStyle = [26, 24, 22, 0.75];
      ctx.beginPath();
      points.forEach(([x, y], index) => (index ? ctx.lineTo(x, y) : ctx.moveTo(x, y)));
      ctx.closePath(); ctx.fill();
    }
    if (opts.roughness) {
      ctx.fillStyle = [255, 250, 255, 0.6];
      ctx.beginPath();
      points.forEach(([x, y], index) => (index ? ctx.lineTo(x, y) : ctx.moveTo(x, y)));
      ctx.closePath(); ctx.fill();
    }
  }
}

// ---- mid-poly part builders ---------------------------------------------------------------------

/** Surface of revolution about local +Y from a [radius, y] profile: noses, bells, tanks, masts. */
export function lathe(parent: THREE.Object3D, material: THREE.Material, profile: number[][], segments = 28, pos: number[] = [0, 0, 0], rot: number[] = [0, 0, 0]): THREE.Mesh {
  const points = profile.map(([radius, y]) => new THREE.Vector2(Math.max(1e-4, radius), y));
  const geometry = new THREE.LatheGeometry(points, segments);
  const mesh = new THREE.Mesh(geometry, material);
  mesh.position.set(pos[0], pos[1], pos[2]);
  mesh.rotation.set(rot[0], rot[1], rot[2]);
  mesh.castShadow = true; mesh.receiveShadow = true;
  parent.add(mesh);
  return mesh;
}

/** A cylinder with an orientation; smooth-shaded side wall, real caps. */
export function tube(parent: THREE.Object3D, material: THREE.Material, top: number, bottom: number, height: number, pos: number[], segments = 24, rot: number[] = [0, 0, 0]): THREE.Mesh {
  const geometry = new THREE.CylinderGeometry(top, bottom, height, segments, 1, false);
  const mesh = new THREE.Mesh(geometry, material);
  mesh.position.set(pos[0], pos[1], pos[2]);
  mesh.rotation.set(rot[0], rot[1], rot[2]);
  mesh.castShadow = true; mesh.receiveShadow = true;
  parent.add(mesh);
  return mesh;
}

/** A box with bevelled structural edges: what every piece of hull framing should have been. */
export function bevelled(parent: THREE.Object3D, material: THREE.Material, size: number[], pos: number[], rot: number[] = [0, 0, 0], options: { radius?: number; segments?: number } = {}): THREE.Mesh {
  const radius = Math.min(options.radius ?? 0.55, Math.min(size[0], size[1], size[2]) * 0.42);
  // A roundover under half a metre is sub-pixel at any range the part is seen from, and structure
  // fields place hundreds of these - so small-radius, unrefined bevels degrade to plain boxes
  // (plan 05 s3's budget). A real roundover still costs a RoundedBox.
  if (radius < 0.5 && (options.segments ?? 0) <= 1) {
    const mesh = new THREE.Mesh(new THREE.BoxGeometry(size[0], size[1], size[2]), material);
    mesh.position.set(pos[0], pos[1], pos[2]);
    mesh.rotation.set(rot[0], rot[1], rot[2]);
    mesh.castShadow = true; mesh.receiveShadow = true;
    parent.add(mesh);
    return mesh;
  }
  const geometry = new RoundedBoxGeometry(size[0], size[1], size[2], Math.min(options.segments ?? 0, 1), radius);
  const mesh = new THREE.Mesh(geometry, material);
  mesh.position.set(pos[0], pos[1], pos[2]);
  mesh.rotation.set(rot[0], rot[1], rot[2]);
  mesh.castShadow = true; mesh.receiveShadow = true;
  parent.add(mesh);
  return mesh;
}

export function sphere(parent: THREE.Object3D, material: THREE.Material, radius: number, pos: number[], width_segments = 24, height_segments = 16): THREE.Mesh {
  const mesh = new THREE.Mesh(new THREE.SphereGeometry(radius, width_segments, height_segments), material);
  mesh.position.set(pos[0], pos[1], pos[2]);
  mesh.castShadow = true; mesh.receiveShadow = true;
  parent.add(mesh);
  return mesh;
}

/** A hemispherical blister, facing local +Y before `rot`: hatches, sensor domes, tank ends. */
export function dome(parent: THREE.Object3D, material: THREE.Material, radius: number, pos: number[], rot: number[] = [0, 0, 0], segments = 24): THREE.Mesh {
  const geometry = new THREE.SphereGeometry(radius, segments, Math.max(6, segments / 2), 0, Math.PI * 2, 0, Math.PI / 2);
  const mesh = new THREE.Mesh(geometry, material);
  mesh.position.set(pos[0], pos[1], pos[2]);
  mesh.rotation.set(rot[0], rot[1], rot[2]);
  mesh.castShadow = true; mesh.receiveShadow = true;
  parent.add(mesh);
  return mesh;
}

export function torus(parent: THREE.Object3D, material: THREE.Material, radius: number, thickness: number, pos: number[], rot: number[] = [0, 0, 0], radial = 10, tubular = 64): THREE.Mesh {
  const mesh = new THREE.Mesh(new THREE.TorusGeometry(radius, thickness, radial, tubular), material);
  mesh.position.set(pos[0], pos[1], pos[2]);
  mesh.rotation.set(rot[0], rot[1], rot[2]);
  mesh.castShadow = true; mesh.receiveShadow = true;
  parent.add(mesh);
  return mesh;
}

/** A paraboloid dish of the given aperture and depth, opening along local +Y before `rot`. */
export function dish(parent: THREE.Object3D, material: THREE.Material, radius: number, depth: number, pos: number[], rot: number[] = [0, 0, 0], segments = 28): THREE.Mesh {
  const curve = (radius * radius + depth * depth) / (2 * depth);
  const profile: number[][] = [];
  const steps = 9;
  for (let i = 0; i <= steps; i++) {
    const span = radius * (i / steps);
    profile.push([span, curve - Math.sqrt(Math.max(0, curve * curve - span * span))]);
  }
  const mesh = lathe(parent, material, profile, segments, pos, rot);
  mesh.geometry.computeVertexNormals();
  return mesh;
}

/**
 * A parabolic dish on a yoke, boresight along local +Y: the scanner on a mast, a ground station's
 * antenna, a relay's high-gain. Returns the group so a module can point it, and so `hp.<id>` can
 * sit on the boresight (`[0, 0, 0]` in the group's frame, rot 0, is the vertex looking +Y).
 */
export function sensor_dish(parent: THREE.Object3D, opts: { radius?: number; depth?: number; pos?: number[]; rot?: number[]; segments?: number; material?: THREE.Material; horn?: boolean; yoke?: boolean } = {}): THREE.Group {
  const radius = opts.radius ?? 3.2;
  const depth = opts.depth ?? 1.1;
  const group = new THREE.Group();
  group.name = 'sensor-dish';
  const pos = opts.pos ?? [0, 0, 0];
  const rot = opts.rot ?? [0, 0, 0];
  group.position.set(pos[0], pos[1], pos[2]);
  group.rotation.set(rot[0], rot[1], rot[2]);
  // Bowl: vertex at the group origin, rim at y = depth, concave side facing the +Y boresight.
  dish(group, opts.material ?? insulation, radius, depth, [0, 0, 0], [0, 0, 0], opts.segments ?? 26);
  torus(group, metal, radius * 0.98, Math.max(0.05, radius * 0.05), [0, depth, 0], [Math.PI / 2, 0, 0], 6, 26);
  if (opts.horn !== false) {
    // Feed horn on the axis, held by three struts off the rim: the focus, where the beam starts.
    const focus = depth + radius * 0.55;
    for (let i = 0; i < 3; i++) {
      const angle = (i / 3) * Math.PI * 2 + 0.4;
      strut(group, metal, [Math.cos(angle) * radius * 0.94, depth, Math.sin(angle) * radius * 0.94], [0, focus, 0], Math.max(0.05, radius * 0.03), 4);
    }
    dome(group, metal, Math.max(0.2, radius * 0.16), [0, focus, 0], [0, 0, 0], 10);
    lathe(group, copper, [[0.16, 0], [0.16, radius * 0.42]], 8, [0, focus - radius * 0.42, 0]);
  }
  if (opts.yoke !== false) {
    for (const side of [-1, 1]) {
      strut(group, metal, [side * radius * 0.8, depth * 0.35, 0], [side * radius * 0.62, -radius * 0.9, 0], Math.max(0.06, radius * 0.045), 5);
    }
    tube(group, dark, radius * 0.2, radius * 0.2, radius * 0.5, [0, -radius * 0.95, 0], 10);
  }
  parent.add(group);
  return group;
}

/** A cylinder spanning two points: the truss primitive. */
export function strut(parent: THREE.Object3D, material: THREE.Material, from: number[], to: number[], radius = 0.5, segments = 6): THREE.Mesh {
  const a = new THREE.Vector3(...from), b = new THREE.Vector3(...to);
  const length = a.distanceTo(b);
  const mesh = new THREE.Mesh(new THREE.CylinderGeometry(radius, radius, Math.max(1e-3, length), segments, 1, false), material);
  mesh.position.copy(a).add(b).multiplyScalar(0.5);
  mesh.quaternion.setFromUnitVectors(new THREE.Vector3(0, 1, 0), b.clone().sub(a).normalize());
  mesh.castShadow = true; mesh.receiveShadow = true;
  parent.add(mesh);
  return mesh;
}

/**
 * A lattice truss along local +Y: four longerons on a rectangular section, a ring of battens and
 * one diagonal per bay per face. This is what makes a structure read as built rather than extruded.
 */
export function truss_box(parent: THREE.Object3D, material: THREE.Material, length: number, width: number, depth: number, bays: number, radius = 0.45): THREE.Group {
  const group = new THREE.Group();
  group.name = 'truss';
  const hw = width / 2, hd = depth / 2;
  const corners = [[-hw, -hd], [hw, -hd], [hw, hd], [-hw, hd]];
  const at = (corner: number[], y: number) => [corner[0], y, corner[1]];
  for (let bay = 0; bay < bays; bay++) {
    const y0 = -length / 2 + (bay / bays) * length;
    const y1 = -length / 2 + ((bay + 1) / bays) * length;
    for (let c = 0; c < 4; c++) {
      // Four segments a strut: a truss is read in silhouette, and a fat prism reads the same as a
      // fat cylinder at every range the truss is seen from (plan 05 s3's budget).
      strut(group, material, at(corners[c], y1), at(corners[(c + 1) % 4], y1), radius * 0.75, 4);
      strut(group, material, at(corners[c], y0), at(corners[(c + 1) % 4], y0), radius * 0.75, 4);
      strut(group, material, at(corners[c], y0), at(corners[(c + 1) % 4], y1), radius * 0.55, 4);
    }
  }
  for (let c = 0; c < 4; c++) strut(group, material, at(corners[c], -length / 2), at(corners[c], length / 2), radius, 4);
  parent.add(group);
  return group;
}

/** Raised panel relief on a flat face: the panel lines that read as geometry at close range. */
export function panel_relief(parent: THREE.Object3D, material: THREE.Material, size: number[], pos: number[], rot: number[] = [0, 0, 0], options: { cols?: number; rows?: number; thickness?: number; depth?: number } = {}): THREE.Group {
  const group = new THREE.Group();
  group.name = 'panel';
  const cols = options.cols ?? 3, rows = options.rows ?? 3;
  const thickness = options.thickness ?? 0.5, depth = options.depth ?? 0.35;
  for (let i = 1; i < cols; i++) {
    const at = -size[0] / 2 + (i / cols) * size[0];
    bevelled(group, material, [thickness, size[1], depth], [at, 0, size[2] / 2], [0, 0, 0], { radius: thickness * 0.35, segments: 1 });
  }
  for (let j = 1; j < rows; j++) {
    const at = -size[1] / 2 + (j / rows) * size[1];
    bevelled(group, material, [size[0], thickness, depth], [0, at, size[2] / 2], [0, 0, 0], { radius: thickness * 0.35, segments: 1 });
  }
  group.position.set(pos[0], pos[1], pos[2]);
  group.rotation.set(rot[0], rot[1], rot[2]);
  parent.add(group);
  return group;
}

/** Lamps share a material per colour: one merged draw call per lit colour, not one per lamp. */
const lampMaterials = new Map<string, THREE.MeshBasicMaterial>();
export function lamp_material(color: string): THREE.MeshBasicMaterial {
  let material = lampMaterials.get(color);
  if (!material) {
    material = new THREE.MeshBasicMaterial({ color });
    material.userData.shared = true;
    lampMaterials.set(color, material);
  }
  return material;
}

/** A run of marker lamps at model-space points. Approach lighting, ring strobes, runway edges. */
export function lamps(parent: THREE.Object3D, color: string, points: number[][], radius = 0.9, segments = 8): THREE.Group {
  const group = new THREE.Group();
  group.name = 'lamps';
  const material = lamp_material(color);
  for (const point of points) {
    const mesh = new THREE.Mesh(new THREE.SphereGeometry(radius, segments, Math.max(4, segments / 2)), material);
    mesh.position.set(point[0], point[1], point[2]);
    group.add(mesh);
  }
  parent.add(group);
  return group;
}

/** `hp.<id>`: an anchor with no geometry, carried to the sidecar and drawn by the viewer's overlay. */
export function hp(parent: THREE.Object3D, id: string, pos: number[], rot: number[] = [0, 0, 0]): THREE.Object3D {
  const node = new THREE.Object3D();
  node.name = `hp.${id}`;
  node.position.set(pos[0], pos[1], pos[2]);
  node.rotation.set(rot[0], rot[1], rot[2]);
  parent.add(node);
  return node;
}

/** An effect cone along local -Y (a flame) or +Y: hidden, additive, driven by the simulation. */
export function effect_cone(parent: THREE.Object3D, effects: THREE.Mesh[], opts: { name: string; radius: number; length: number; pos: number[]; rot?: number[]; material?: THREE.Material; flip?: boolean }): THREE.Mesh {
  const geometry = new THREE.ConeGeometry(opts.radius, opts.length, 12, 1, true);
  geometry.rotateZ(opts.flip === false ? 0 : Math.PI);
  geometry.translate(0, (opts.flip === false ? 1 : -1) * opts.length / 2, 0);
  const mesh = new THREE.Mesh(geometry, opts.material ?? exhaust);
  mesh.name = opts.name;
  mesh.userData.effect = true;
  mesh.position.set(opts.pos[0], opts.pos[1], opts.pos[2]);
  const rot = opts.rot ?? [0, 0, 0];
  mesh.rotation.set(rot[0], rot[1], rot[2]);
  mesh.visible = false;
  parent.add(mesh);
  effects.push(mesh);
  return mesh;
}

/** A jet nozzle plus its effect child: RCS quads, vernier clusters. */
export function jet_nozzle(parent: THREE.Object3D, effects: THREE.Mesh[], opts: { name: string; radius: number; pos: number[]; direction: number[]; length?: number; material?: THREE.Material }): THREE.Mesh {
  const length = opts.length ?? 7;
  const dir = new THREE.Vector3(...opts.direction).normalize();
  const bell = new THREE.Mesh(new THREE.CylinderGeometry(opts.radius * 0.55, opts.radius, length, 14, 1, false), dark);
  bell.position.set(opts.pos[0], opts.pos[1], opts.pos[2]);
  bell.quaternion.setFromUnitVectors(new THREE.Vector3(0, 1, 0), dir);
  bell.castShadow = true;
  parent.add(bell);
  const geometry = new THREE.ConeGeometry(opts.radius * 0.8, length * 2.6, 12, 1, true);
  geometry.rotateZ(Math.PI);
  geometry.translate(0, -length * 1.3, 0);
  const jet = new THREE.Mesh(geometry, opts.material ?? jetMaterial);
  jet.name = opts.name;
  jet.userData.effect = true;
  jet.position.set(opts.pos[0] + dir.x * length * 0.5, opts.pos[1] + dir.y * length * 0.5, opts.pos[2] + dir.z * length * 0.5);
  jet.quaternion.copy(bell.quaternion);
  jet.visible = false;
  parent.add(jet);
  effects.push(jet);
  return jet;
}
/**
 * The modular flange (plan-04 A6, §2.5): the one interface every stackable component carries at
 * both ends. 2.5 m diameter, ~0.18 m ring, 8 raised bolt bosses on equal stations, two copper
 * alignment keys at 0° and 180°. Built about local +Y at `pos`, facing `normal`, so a module on
 * any axis gets the identical part by passing its stack direction.
 */
export const FLANGE_RADIUS = 1.25;
export function flange(root: THREE.Object3D, material: THREE.Material, pos: number[], normal: number[] = [0, 1, 0]): THREE.Group {
  const site = new THREE.Group();
  site.position.set(pos[0], pos[1], pos[2]);
  site.quaternion.setFromUnitVectors(
    new THREE.Vector3(0, 1, 0),
    new THREE.Vector3(normal[0], normal[1], normal[2]).normalize());
  root.add(site);
  // Ring plate and its rolled edge: the plate lands the bolts, the torus is the lip that catches
  // light. A three.js torus lies in the XY plane, so the lip yaws 90° about X to lie flat
  // about the +Y stack axis.
  cylinder(site, material, FLANGE_RADIUS, FLANGE_RADIUS, 0.1, [0, 0, 0], 32);
  torus(site, material, FLANGE_RADIUS - 0.09, 0.09, [0, 0.05, 0], [Math.PI / 2, 0, 0], 8, 32);
  // Eight bolt bosses, raised proud of the face on the ring centreline.
  for (let i = 0; i < 8; i++) {
    const angle = (i / 8) * Math.PI * 2;
    cylinder(site, material, 0.09, 0.09, 0.18,
      [Math.cos(angle) * (FLANGE_RADIUS - 0.09), 0.06, Math.sin(angle) * (FLANGE_RADIUS - 0.09)], 8);
  }
  // Copper alignment keys at 0° and 180°: the only warm-coloured thing on the interface, so a
  // loader can see the clocking without reading a stencil.
  for (const side of [1, -1]) box(site, copper, [0.22, 0.16, 0.22], [side * (FLANGE_RADIUS - 0.09), 0.06, 0]);
  return site;
}

/** Bare-metal grey used by wreck plating and structural framing across the new assets. */
export const frame = new THREE.MeshStandardMaterial({ color: '#5d6a72', roughness: 0.55, metalness: 0.9 });
export const hullPaint = new THREE.MeshStandardMaterial({ color: '#c3ccca', roughness: 0.6, metalness: 0.35 });
export const hazardPaint = new THREE.MeshStandardMaterial({ color: '#c19a3e', roughness: 0.78, metalness: 0.2 });
export const deckPlate = new THREE.MeshStandardMaterial({ color: '#6a7076', roughness: 0.82, metalness: 0.5 });
export const solarCell = new THREE.MeshStandardMaterial({ color: '#16324f', roughness: 0.32, metalness: 0.55, emissive: '#0b1d31', emissiveIntensity: 0.35 });
export const insulation = new THREE.MeshStandardMaterial({ color: '#cfc7ae', roughness: 0.9, metalness: 0.08 });
export const rock = new THREE.MeshStandardMaterial({ color: '#6f6a63', roughness: 0.96, metalness: 0.06 });
for (const material of [frame, hullPaint, hazardPaint, deckPlate, solarCell, insulation, rock]) material.userData.shared = true;

/**
 * Material names. `meta.untextured` in a module lists the ones the procedural maps must skip -
 * windows, solar cells, lamp faces - and it can only name them if they are named.
 */
const PALETTE: Record<string, THREE.Material> = {
  armor, lightArmor, dark, metal, copper, black, glass, oreShell, oreVein, teal, ochre, ceramic,
  frame, hullPaint, hazardPaint, deckPlate, solarCell, insulation, rock,
};
for (const [name, material] of Object.entries(PALETTE)) material.name = name;

/**
 * Plan 05 s3.2: the authoring-time backstop. A feature under the 1.5 m floor is noise at the home
 * framing - it averages into the hull - so the helper throws while authoring, and the exporter's
 * audit (s3.3) stays the second line of defence instead of the first.
 */
export const MIN_FEATURE_M = 1.5;

/** Throws when any dimension of  is under the minimum feature floor. */
export function min_feature(size: number[], name: string): void {
  const smallest = Math.min(...size.map((v) => Math.abs(v)));
  if (smallest < MIN_FEATURE_M) {
    throw new Error(
      name + ': feature ' + smallest.toFixed(2) + ' m is under the ' + MIN_FEATURE_M +
        ' m floor (plan 05 s3.2) - delete it, widen it, or move it to a normal map',
    );
  }
}
