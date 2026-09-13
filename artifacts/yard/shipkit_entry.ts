// Opra Shipkit - production section-module ships, the three approved variants assembled from
// artifacts/yard/parts/*.ts through artifacts/yard/designs/*.ts.
//
// Same shell and same cameras as prototype_entry.ts, so the production hull can be compared to the
// approved prototype frame for frame. The game view is the engine's own: render/camera.cpp's
// orbit_eye() puts the eye at (0, -cos(pitch)*r, sin(pitch)*r) with CAMERA_PITCH_DEFAULT = 32 deg,
// and three.js resolves up=(0,1,0) to the same (0, 0.53, 0.85) the C++ frustum computes.
//
// Untextured, like the prototype: every part authors standard_maps(), but the maps are baked by
// tools/export.mjs at GLB time. What is on screen here is the geometry and the material palette,
// which is exactly what the gates measure.
window.__lastErr = null;
window.addEventListener('error', (e) => {
  window.__lastErr = e.message + ' at ' + e.filename + ':' + e.lineno;
  console.error(window.__lastErr);
});

import * as THREE from 'three';
import { OrbitControls } from 'three/examples/jsm/controls/OrbitControls.js';
import { build as buildA } from './designs/variant_a_hammerhead';
import { build as buildB } from './designs/variant_b_ingot';
import { build as buildC } from './designs/variant_c_waverider';
interface VariantDef {
  key: string;
  name: string;
  subtitle: string;
  builder: () => THREE.Object3D;
  dimensions: string;
  maxSpan: string;
  silhouetteType: string;
  archetype: string;
  description: string;
  highlights: string[];
}

const VARIANTS: VariantDef[] = [
  {
    key: 'A',
    name: 'Hammerhead Submarine',
    subtitle: 'Naval Patrol Corvette',
    builder: buildA,
    dimensions: '20.7 m × 6.0 m × 3.4 m',
    maxSpan: '6.0 m (Radiator wings)',
    silhouetteType: 'Hammerhead bow → 2.6 m waist → Radiator wings → Twin torch',
    archetype: 'The Expanse: MCRN Corvette / Rocinante class',
    description: '5 modules at the 4 m station pitch. CIC sponsons cantilevered off the pressure hull, conformal saddle tanks on a cut-down waist, radiators standing clear of the reactor hull on booms, twin fusion bells in one thrust block.',
    highlights: [
      'complexity 78.3 (floor 60) · section CV 0.23 (floor 0.20) · dark 35% (cap 45%)',
      'Stations: 5.3 / 5.0 / 4.0 / 6.0 / 3.3 / 3.3 m across the plan view',
      'nose_hammerhead 384 tri · section_combat_a 288 · section_tank_saddle 324',
      'section_radiator_wing 304 · drive_twin_torch 376 (own geometry, flange excluded)',
      'Prototype scored CV 0.19 FAIL and lightArmor 63% FAIL on the same three gates',
    ],
  },
  {
    key: 'B',
    name: 'Caterpillar Ingot',
    subtitle: 'Heavy Industrial Hauler',
    builder: buildB,
    dimensions: '20.9 m × 6.8 m × 4.2 m',
    maxSpan: '6.8 m (Outrigger radiator banks)',
    silhouetteType: 'Push-knee bow → Container carriage → Outrigger banks → Quad block',
    archetype: 'Industrial utility: Caterpillar / Komatsu in space',
    description: '5 modules at the 4 m station pitch. Push-plate bow on rams with the work cab on the deck, two ISO containers on twist-locks, radiator banks held off the machinery core on short lattice arms, 2×2 drive block with amber confinement glow.',
    highlights: [
      'complexity 72.7 (floor 60) · section CV 0.21 (floor 0.20) · metal 41% (cap 45%)',
      'Stations: 4.0 / 5.3 / 6.7 / 6.7 / 4.3 / 4.3 m across the plan view',
      'nose_pushbow 284 tri · section_freight 248 · section_machinery 304',
      'section_reactor 272 · drive_quad_block 380 (own geometry, flange excluded)',
      'Amber glow via prims lamp_material(); the palette carries one glow and it is cyan',
    ],
  },
  {
    key: 'C',
    name: 'Arrowhead Waverider',
    subtitle: 'Stealth Delta Interceptor',
    builder: buildC,
    dimensions: '21.0 m × 7.2 m × 2.5 m',
    maxSpan: '7.2 m (Delta sponsons)',
    silhouetteType: 'Faceted prow → Flaring delta → Sponsons → Notched shroud',
    archetype: 'Stealth predator: Amun-Ra / Protogen black-ops',
    description: '5 modules at the 4 m station pitch. Stepped faceted prow, flush missile cells and conformal PDC bays, delta wings on carry-through pylons with the slot open, twin nozzles sunk in armoured shrouds with a 1.3 m stern notch between them.',
    highlights: [
      'complexity 82.7 (floor 60) · section CV 0.26 (floor 0.20) · dark 40% (cap 45%)',
      'Stations: 2.7 / 5.3 / 5.7 / 7.3 / 6.0 / 5.3 m across the plan view',
      'nose_stealth_needle 192 tri · section_delta_fore 204 · section_stealth_combat 248',
      'section_delta_aft 236 · drive_stealth_twin 368 (own geometry, flange excluded)',
      'Prototype scored complexity 50.0 FAIL and lightArmor 51% FAIL; the slots fixed both',
    ],
  },
];

// Initialize Scene
const canvas = document.getElementById('prototype-canvas') as HTMLCanvasElement;
const renderer = new THREE.WebGLRenderer({ canvas, antialias: true, alpha: false, preserveDrawingBuffer: true });
renderer.setPixelRatio(Math.min(window.devicePixelRatio, 2));
renderer.setSize(window.innerWidth, window.innerHeight);
renderer.toneMapping = THREE.ACESFilmicToneMapping;
renderer.toneMappingExposure = 1.15;
renderer.shadowMap.enabled = true;

const scene = new THREE.Scene();
scene.background = new THREE.Color('#0a0e13');

// Lighting rig (key light, cold fill, warm rim)
const keyLight = new THREE.DirectionalLight('#ffffff', 2.8);
keyLight.position.set(25, 20, 40);
keyLight.castShadow = true;
scene.add(keyLight);

const fillLight = new THREE.DirectionalLight('#486882', 1.2);
fillLight.position.set(-25, -20, 20);
scene.add(fillLight);

const rimLight = new THREE.DirectionalLight('#82b5d8', 0.8);
rimLight.position.set(0, -35, -20);
scene.add(rimLight);

const ambLight = new THREE.AmbientLight('#202a33', 1.0);
scene.add(ambLight);

// Reference ground plane grid (on the XY gameplay plane, recessed behind ship at -Z)
const grid = new THREE.GridHelper(50, 50, '#2d4253', '#16202c');
grid.rotation.x = Math.PI / 2;
grid.position.z = -2.5;
scene.add(grid);

// Camera & Controls
const aspect = window.innerWidth / window.innerHeight;
const camera = new THREE.PerspectiveCamera(40, aspect, 0.1, 500);
const controls = new OrbitControls(camera, canvas);
controls.enableDamping = true;
controls.dampingFactor = 0.08;

let currentModel: THREE.Object3D | null = null;
let currentIdx = 0;
let autoRotate = false;

// URL Search Params integration (?variant=A|B|C)
function getVariantFromURL(): number {
  const params = new URLSearchParams(window.location.search);
  const v = params.get('variant')?.toUpperCase();
  const idx = VARIANTS.findIndex((item) => item.key === v);
  return idx >= 0 ? idx : 0;
}

function updateURL(key: string) {
  const url = new URL(window.location.href);
  url.searchParams.set('variant', key);
  window.history.replaceState({}, '', url.toString());
}

function loadVariant(idx: number) {
  currentIdx = (idx + VARIANTS.length) % VARIANTS.length;
  const def = VARIANTS[currentIdx];

  if (currentModel) {
    scene.remove(currentModel);
  }
  currentModel = def.builder();
  scene.add(currentModel);

  updateURL(def.key);
  updateUI(def);
}

function updateUI(def: VariantDef) {
  // Switcher bar
  const pillKey = document.getElementById('variant-pill-key')!;
  const pillName = document.getElementById('variant-pill-name')!;
  pillKey.innerText = `Variant ${def.key}`;
  pillName.innerText = def.name;

  // Info sidebar
  document.getElementById('info-title')!.innerText = def.name;
  document.getElementById('info-subtitle')!.innerText = def.subtitle;
  document.getElementById('info-desc')!.innerText = def.description;
  document.getElementById('info-archetype')!.innerText = def.archetype;
  document.getElementById('info-dim')!.innerText = def.dimensions;
  document.getElementById('info-span')!.innerText = def.maxSpan;
  document.getElementById('info-silhouette')!.innerText = def.silhouetteType;

  const hlList = document.getElementById('info-highlights')!;
  hlList.innerHTML = def.highlights.map((h) => `<li>${h}</li>`).join('');

  // Update button tabs
  document.querySelectorAll('.tab-btn').forEach((btn) => {
    btn.classList.toggle('active', btn.getAttribute('data-key') === def.key);
  });
}

// Camera views
function setView(mode: string) {
  const dist = 38;
  controls.target.set(0, 3.0, 0);

  if (mode === 'game_oblique') {
    // 32 degree pitch (The exact game camera view)
    // Gameplay plane is XY (nose +Y, starboard +X). Dorsal is +Z.
    // Eye is aft (-Y) and elevated above the plane (+Z) looking forward at the ship.
    const pitchRad = 32 * (Math.PI / 180);
    camera.up.set(0, 1, 0);
    camera.position.set(0, 3.0 - dist * Math.cos(pitchRad), dist * Math.sin(pitchRad));
  } else if (mode === 'plan') {
    // 90 degree overhead plan view (looking straight down +Z onto XY plane)
    camera.up.set(0, 1, 0);
    camera.position.set(0, 3.0, dist * 1.0);
  } else if (mode === 'perspective') {
    // Dynamic isometric perspective
    camera.up.set(0, 0, 1);
    camera.position.set(dist * 0.7, -dist * 0.7, dist * 0.65);
  } else if (mode === 'side') {
    // Flank profile view (looking along X)
    camera.up.set(0, 1, 0);
    camera.position.set(dist * 1.1, 3.0, 0);
  }
  controls.update();

  document.querySelectorAll('.view-btn').forEach((btn) => {
    btn.classList.toggle('active', btn.getAttribute('data-view') === mode);
  });
}

// Setup Event Listeners
document.getElementById('btn-prev')!.addEventListener('click', () => loadVariant(currentIdx - 1));
document.getElementById('btn-next')!.addEventListener('click', () => loadVariant(currentIdx + 1));

document.querySelectorAll('.tab-btn').forEach((btn) => {
  btn.addEventListener('click', () => {
    const key = btn.getAttribute('data-key')!;
    const idx = VARIANTS.findIndex((v) => v.key === key);
    if (idx >= 0) loadVariant(idx);
  });
});

document.querySelectorAll('.view-btn').forEach((btn) => {
  btn.addEventListener('click', () => {
    setView(btn.getAttribute('data-view')!);
  });
});

const rotateToggle = document.getElementById('btn-rotate')!;
rotateToggle.addEventListener('click', () => {
  autoRotate = !autoRotate;
  controls.autoRotate = autoRotate;
  controls.autoRotateSpeed = 2.0;
  rotateToggle.classList.toggle('active', autoRotate);
});

// Keyboard navigation (Left/Right arrows)
window.addEventListener('keydown', (e) => {
  if (['input', 'textarea'].includes((document.activeElement?.tagName || '').toLowerCase())) return;
  if (e.key === 'ArrowLeft') {
    loadVariant(currentIdx - 1);
  } else if (e.key === 'ArrowRight') {
    loadVariant(currentIdx + 1);
  } else if (e.key === '1') {
    setView('game_oblique');
  } else if (e.key === '2') {
    setView('plan');
  } else if (e.key === '3') {
    setView('perspective');
  } else if (e.key === '4') {
    setView('side');
  }
});

window.addEventListener('resize', () => {
  const w = window.innerWidth, h = window.innerHeight;
  camera.aspect = w / h;
  camera.updateProjectionMatrix();
  renderer.setSize(w, h);
});

// Initial launch
loadVariant(getVariantFromURL());
setView('game_oblique'); // Default to the 32° game oblique view!

function animate() {
  requestAnimationFrame(animate);
  controls.update();
  renderer.render(scene, camera);
}
animate();

// Presentation mode: ?bare=1 drops the chrome so a screenshot is the ship and nothing else, and
// ?view=plan|game_oblique|perspective|side picks the framing without a click.
{
  const params = new URLSearchParams(window.location.search);
  if (params.get('bare') === '1') {
    for (const sel of ['.top-bar', '.info-panel', '.switcher', '.view-bar']) {
      const node = document.querySelector(sel) as HTMLElement | null;
      if (node) node.style.display = 'none';
    }
  }
  const view = params.get('view');
  if (view) setView(view);
}
