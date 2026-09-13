// Opra Shipkit — Prototype 3-Variant Explorer
// Testing 3 radically different section-module hull designs for the 32° game oblique camera.

window.__lastErr = null;
window.addEventListener('error', (e) => {
  window.__lastErr = e.message + ' at ' + e.filename + ':' + e.lineno;
  console.error(window.__lastErr);
});

import * as THREE from 'three';
import { OrbitControls } from 'three/examples/jsm/controls/OrbitControls.js';

// Reusable hard sci-fi materials matching Expanse aesthetic
const matLightArmor = new THREE.MeshStandardMaterial({
  color: '#d6d9dc', roughness: 0.45, metalness: 0.35,
});
const matDark = new THREE.MeshStandardMaterial({
  color: '#2b343d', roughness: 0.65, metalness: 0.7,
});
const matMetal = new THREE.MeshStandardMaterial({
  color: '#6e767d', roughness: 0.48, metalness: 0.85,
});
const matCopper = new THREE.MeshStandardMaterial({
  color: '#a86538', roughness: 0.55, metalness: 0.65,
});
const matBlack = new THREE.MeshStandardMaterial({
  color: '#15191d', roughness: 0.75, metalness: 0.4,
});
const matGlass = new THREE.MeshStandardMaterial({
  color: '#1c3d4a', roughness: 0.2, metalness: 0.8,
});
const matGlowCyan = new THREE.MeshBasicMaterial({
  color: '#6be7ff',
});
const matGlowAmber = new THREE.MeshBasicMaterial({
  color: '#ff9d3b',
});
const matHazard = new THREE.MeshStandardMaterial({
  color: '#d4982a', roughness: 0.5, metalness: 0.3,
});

// Helper geometry builders
function makeBox(parent: THREE.Object3D, mat: THREE.Material, size: [number, number, number], pos: [number, number, number], rot?: [number, number, number]): THREE.Mesh {
  const geo = new THREE.BoxGeometry(...size);
  const mesh = new THREE.Mesh(geo, mat);
  mesh.position.set(...pos);
  if (rot) mesh.rotation.set(...rot);
  mesh.castShadow = true;
  mesh.receiveShadow = true;
  parent.add(mesh);
  return mesh;
}

function makeCylinder(parent: THREE.Object3D, mat: THREE.Material, rTop: number, rBot: number, height: number, segs: number, pos: [number, number, number], rot?: [number, number, number]): THREE.Mesh {
  const geo = new THREE.CylinderGeometry(rTop, rBot, height, segs);
  const mesh = new THREE.Mesh(geo, mat);
  mesh.position.set(...pos);
  if (rot) mesh.rotation.set(...rot);
  mesh.castShadow = true;
  mesh.receiveShadow = true;
  parent.add(mesh);
  return mesh;
}

/* =========================================================================
   VARIANT A: "Hammerhead Submarine" (Naval Patrol Corvette)
   Philosophy: Classic Expanse gunboat (Rocinante class).
   Distinctive: Flared hammerhead bridge wings, armored waist, wide radiator
                shoulders, recessed twin fusion torch block.
   ========================================================================= */
export function buildVariantA(): THREE.Object3D {
  const root = new THREE.Group();
  root.name = 'variant_a_hammerhead';

  // --- 1. FORE NOSE & HAMMERHEAD BRIDGE (y = +9 to +13) ---
  // Bow sensor wedge
  makeBox(root, matLightArmor, [2.2, 2.5, 1.4], [0, 12.0, 0]);
  makeBox(root, matDark, [1.6, 1.0, 1.1], [0, 13.0, 0]);
  // Forward sensor array & optics
  makeBox(root, matGlass, [1.0, 0.4, 0.2], [0, 12.8, 0.65]);
  makeBox(root, matDark, [1.2, 0.5, 0.1], [0, 12.8, 0.62]);
  // Torpedo tube muzzle ports (ventral & dorsal)
  for (const sx of [-0.6, 0.6]) {
    makeCylinder(root, matBlack, 0.22, 0.22, 0.3, 8, [sx, 12.6, -0.6], [Math.PI / 2, 0, 0]);
    makeCylinder(root, matCopper, 0.25, 0.25, 0.05, 8, [sx, 12.6, -0.65], [Math.PI / 2, 0, 0]);
  }
  // Flared Hammerhead Bridge Wings (creates distinct forward silhouette)
  makeBox(root, matLightArmor, [4.8, 1.8, 1.5], [0, 10.5, 0]);
  makeBox(root, matDark, [5.0, 0.3, 1.6], [0, 10.5, 0]); // mid-girth armor belt
  // Slitted bridge observation viewports
  makeBox(root, matGlass, [1.8, 0.22, 0.08], [0, 10.8, 0.8]);
  for (const sx of [-2.0, 2.0]) {
    // Wing-tip attitude thruster pods & nav beacons
    makeBox(root, matDark, [0.35, 0.8, 0.6], [sx, 10.5, 0]);
    makeBox(root, matGlowAmber, [0.08, 0.08, 0.08], [sx * 1.15, 10.5, 0.4]);
  }

  // --- 2. FORWARD COMBAT & HAB SECTION (y = +5 to +9) ---
  // Primary pressure hull (hexagonal faceted body)
  makeCylinder(root, matLightArmor, 1.7, 1.8, 4.0, 6, [0, 7.0, 0]);
  // Heavy dorsal armor spine
  makeBox(root, matDark, [0.8, 4.0, 0.6], [0, 7.0, 1.6]);
  // Integrated PDC Turrets (dorsal port & ventral starboard)
  makeCylinder(root, matDark, 0.6, 0.7, 0.5, 8, [-1.2, 7.5, 1.3], [0, 0, 0.2]);
  makeBox(root, matMetal, [0.15, 1.4, 0.15], [-1.2, 8.2, 1.4], [0, 0, 0.2]); // Twin barrels
  makeCylinder(root, matDark, 0.6, 0.7, 0.5, 8, [1.2, 6.5, -1.3], [0, 0, -0.2]);
  makeBox(root, matMetal, [0.15, 1.4, 0.15], [1.2, 7.2, -1.4], [0, 0, -0.2]);

  // --- 3. MIDSHIP PROPELLANT HULL (y = +1 to +5) ---
  // Waist pressure hull
  makeCylinder(root, matLightArmor, 1.8, 1.8, 4.0, 6, [0, 3.0, 0]);
  // Conformal fuel saddle tanks (clamped port & starboard)
  for (const sx of [-1, 1]) {
    makeCylinder(root, matMetal, 0.65, 0.65, 3.6, 8, [sx * 1.85, 3.0, 0]);
    // Clamping bands
    makeBox(root, matDark, [0.3, 0.2, 1.4], [sx * 1.85, 4.0, 0]);
    makeBox(root, matDark, [0.3, 0.2, 1.4], [sx * 1.85, 2.0, 0]);
    // High-pressure cryogenic plumbing
    makeCylinder(root, matCopper, 0.08, 0.08, 3.2, 6, [sx * 2.2, 3.0, 0.3]);
  }
  // Dorsal crew airlock collar
  makeCylinder(root, matDark, 0.55, 0.55, 0.3, 8, [0, 3.2, 1.65]);
  makeBox(root, matHazard, [0.45, 0.45, 0.08], [0, 3.2, 1.82]);

  // --- 4. AFT RADIATOR & REACTOR SECTION (y = -3 to +1) ---
  makeCylinder(root, matLightArmor, 1.8, 1.6, 4.0, 6, [0, -1.0, 0]);
  // Swept Radiator Wings (reaches 5.8 m total span — silhouette maker!)
  for (const sx of [-1, 1]) {
    // Structural outrigger pylon
    makeBox(root, matMetal, [1.2, 0.4, 0.2], [sx * 2.0, -1.0, 0]);
    // Massive planar radiator vane (high thermal emissivity)
    makeBox(root, matDark, [0.08, 3.6, 1.4], [sx * 2.85, -1.0, 0]);
    // Flank coolant header
    makeCylinder(root, matCopper, 0.12, 0.12, 3.8, 6, [sx * 2.85, -1.0, 0.75]);
    // Heavy RCS Quad at wingtip
    makeBox(root, matMetal, [0.35, 0.35, 0.35], [sx * 2.9, 0.6, 0]);
    makeCylinder(root, matBlack, 0.08, 0.08, 0.15, 6, [sx * 3.1, 0.6, 0], [0, 0, Math.PI / 2]);
  }

  // --- 5. AFT PROPULSION BLOCK (y = -7 to -3) ---
  // Heavy boxy thrust frame
  makeBox(root, matDark, [3.4, 2.2, 2.0], [0, -4.0, 0]);
  makeBox(root, matLightArmor, [3.2, 0.6, 2.2], [0, -3.2, 0]); // Transition collar
  // Dual-fusion torch bells (side-by-side)
  for (const sx of [-0.85, 0.85]) {
    // Nozzle expansion bell (faceted cone)
    makeCylinder(root, matMetal, 0.7, 1.1, 2.2, 8, [sx, -5.8, 0]);
    // Bell exit lip
    makeCylinder(root, matBlack, 1.15, 1.15, 0.15, 8, [sx, -6.8, 0]);
    // Interior plasma confinement glow
    makeCylinder(root, matGlowCyan, 0.45, 0.45, 0.08, 8, [sx, -5.2, 0]);
  }

  return root;
}

/* =========================================================================
   VARIANT B: "Caterpillar Ingot" (Heavy Industrial Hauler / Freight Corunna)
   Philosophy: Heavy-duty workhorse. Komatsu / Caterpillar in space.
   Distinctive: Stepped modular rail-cars, blunt push-knee bow, container
                saddles, outrigger canted radiator banks, quad drive block.
   ========================================================================= */
export function buildVariantB(): THREE.Object3D {
  const root = new THREE.Group();
  root.name = 'variant_b_ingot';

  // --- 1. BLUNT PUSH-KNEE BOW (y = +9 to +13) ---
  // Heavy push-plate bow (reinforced for shoving asteroid barges)
  makeBox(root, matMetal, [3.8, 1.8, 2.2], [0, 11.5, 0]);
  makeBox(root, matLightArmor, [3.4, 1.4, 2.4], [0, 11.7, 0]);
  // Heavy pusher cushions / rubberized impact bumpers
  for (const sx of [-1.2, 1.2]) {
    makeBox(root, matBlack, [0.6, 0.4, 2.3], [sx, 12.4, 0]);
  }
  // Elevated work cab / bridge perched on dorsal nose
  makeBox(root, matDark, [1.8, 1.4, 0.8], [0, 11.0, 1.4]);
  makeBox(root, matGlass, [1.4, 0.4, 0.3], [0, 11.2, 1.7]); // High-vis bridge visor
  // Industrial high-output floodlights
  for (const sx of [-1.4, 1.4]) {
    makeBox(root, matGlowCyan, [0.25, 0.15, 0.15], [sx, 12.3, 0.8]);
  }

  // --- 2. FORE FREIGHT CARRIAGE (y = +5 to +9) ---
  // Heavy square structural spine
  makeBox(root, matLightArmor, [2.6, 4.0, 2.4], [0, 7.0, 0]);
  // Clamped ISO Shipping / Ore Containers (port and starboard)
  for (const sx of [-1, 1]) {
    makeBox(root, matDark, [1.1, 3.4, 1.4], [sx * 1.85, 7.0, 0]);
    // Cargo tie-down braces
    makeBox(root, matHazard, [1.15, 0.2, 1.45], [sx * 1.85, 7.8, 0]);
    makeBox(root, matHazard, [1.15, 0.2, 1.45], [sx * 1.85, 6.2, 0]);
  }

  // --- 3. MIDSHIP UTILITY BULKHEAD & RADIATORS (y = +1 to +5) ---
  // Heavy octagonal machinery block
  makeCylinder(root, matMetal, 1.8, 1.8, 4.0, 8, [0, 3.0, 0]);
  // Massive canted outrigger radiator banks (reaches 6.4 m total width!)
  for (const sx of [-1, 1]) {
    // Heavy support lattice arm
    makeBox(root, matMetal, [1.5, 0.5, 0.4], [sx * 2.1, 3.0, 0]);
    // Large corrugated radiator plate
    const rad = makeBox(root, matDark, [0.1, 3.2, 2.0], [sx * 3.1, 3.0, 0]);
    rad.rotation.z = sx * 0.12; // Canted angle
    // External high-flow cryo pipe
    makeCylinder(root, matCopper, 0.1, 0.1, 3.4, 6, [sx * 3.1, 3.0, 1.1]);
  }

  // --- 4. AFT REACTOR & FUEL BLOCK (y = -3 to +1) ---
  makeBox(root, matLightArmor, [3.2, 4.0, 2.6], [0, -1.0, 0]);
  // Large cylindrical fuel drum half-embedded along centerline
  makeCylinder(root, matMetal, 1.2, 1.2, 3.8, 8, [0, -1.0, 0.8], [0, 0, 0]);
  makeBox(root, matCopper, [0.3, 3.2, 0.3], [0, -1.0, 1.8]); // Central manifold

  // --- 5. QUAD-THRUSTER STERN BLOCK (y = -7 to -3) ---
  // Massive square engine transom
  makeBox(root, matDark, [4.2, 2.4, 3.2], [0, -4.0, 0]);
  // 4 heavy industrial drive bells in a 2x2 grid
  const offsets: [number, number][] = [
    [-1.0, -0.7], [1.0, -0.7],
    [-1.0, 0.7], [1.0, 0.7],
  ];
  for (const [ox, oz] of offsets) {
    makeCylinder(root, matMetal, 0.55, 0.85, 2.2, 8, [ox, -5.8, oz]);
    makeCylinder(root, matBlack, 0.9, 0.9, 0.15, 8, [ox, -6.8, oz]);
    makeCylinder(root, matGlowAmber, 0.35, 0.35, 0.08, 6, [ox, -5.2, oz]);
  }

  return root;
}

/* =========================================================================
   VARIANT C: "Arrowhead Waverider" (Stealth Delta Interceptor)
   Philosophy: Advanced radar-deflecting stealth warship (Amun-Ra style).
   Distinctive: Continuous flaring delta chines, faceted low-RCS hull,
                recessed weapon bays, wide engine sponsons (7.0 m span).
   ========================================================================= */
export function buildVariantC(): THREE.Object3D {
  const root = new THREE.Group();
  root.name = 'variant_c_waverider';

  // --- 1. SHARP STEALTH NOSE WEDGE (y = +9 to +14) ---
  // Knife-edge faceted prow
  makeBox(root, matDark, [1.4, 2.5, 0.7], [0, 12.8, 0]);
  makeBox(root, matBlack, [0.8, 1.8, 0.4], [0, 13.6, 0]);
  // Recessed radar / forward targeting optics
  makeBox(root, matGlass, [0.5, 0.6, 0.1], [0, 12.4, 0.35]);

  // --- 2. EXPANDING DELTA FORWARD HULL (y = +5 to +9) ---
  // Main forward lifting body
  makeBox(root, matLightArmor, [3.2, 4.0, 1.4], [0, 7.0, 0]);
  // Flanking razor chines (tapering outwards)
  for (const sx of [-1, 1]) {
    const strake = makeBox(root, matDark, [0.9, 3.8, 0.4], [sx * 1.8, 7.0, 0]);
    strake.rotation.z = sx * -0.15; // Sweep angle
  }
  // Low-RCS flush faceted bridge canopy
  makeBox(root, matBlack, [1.2, 1.8, 0.4], [0, 7.8, 0.85]);
  makeBox(root, matGlass, [0.9, 0.4, 0.15], [0, 8.2, 0.95]);

  // --- 3. MIDSHIP STEALTH COMBAT BODY (y = +1 to +5) ---
  // Wide armored core
  makeBox(root, matLightArmor, [4.4, 4.0, 1.6], [0, 3.0, 0]);
  // Dorsal recessed missile silos (flush stealth trap doors)
  for (const sy of [2.0, 4.0]) {
    for (const sx of [-0.9, 0.9]) {
      makeBox(root, matDark, [0.6, 0.9, 0.08], [sx, sy, 0.82]);
      makeBox(root, matCopper, [0.08, 0.08, 0.08], [sx, sy, 0.86]); // Actuator
    }
  }
  // Conformal pop-up PDC bays (retracted flush)
  for (const sx of [-1, 1]) {
    makeBox(root, matDark, [0.8, 1.2, 0.3], [sx * 2.3, 3.0, 0.5]);
    makeCylinder(root, matMetal, 0.08, 0.08, 0.8, 6, [sx * 2.3, 3.6, 0.5], [0, 0, sx * 0.2]);
  }

  // --- 4. AFT DELTA SPONSONS & HEAT SINKS (y = -3 to +1) ---
  // Flaring wing sponsons (reaches massive 7.0 m total wingspan!)
  makeBox(root, matLightArmor, [4.8, 4.0, 1.8], [0, -1.0, 0]);
  for (const sx of [-1, 1]) {
    // Delta wing extensions
    const wing = makeBox(root, matDark, [1.2, 4.0, 0.8], [sx * 2.8, -1.0, 0]);
    wing.rotation.z = sx * -0.22;
    // Internal heat-radiator vent grilles on trailing edge
    makeBox(root, matBlack, [0.8, 2.2, 0.25], [sx * 2.8, -1.5, 0.5]);
    makeBox(root, matGlowCyan, [0.06, 0.06, 0.06], [sx * 3.4, -2.5, 0]); // Wingtip beacon
  }

  // --- 5. RECESSED TWIN STEALTH ENGINES (y = -7 to -3) ---
  // Heavy armored engine shroud (hides infrared signature from flanks)
  makeBox(root, matDark, [5.2, 2.4, 2.0], [0, -4.0, 0]);
  // Recessed de Laval fusion nozzles
  for (const sx of [-1.2, 1.2]) {
    makeCylinder(root, matMetal, 0.75, 1.1, 2.2, 8, [sx, -5.8, 0]);
    makeCylinder(root, matBlack, 1.15, 1.15, 0.15, 8, [sx, -6.8, 0]);
    makeCylinder(root, matGlowCyan, 0.5, 0.5, 0.08, 8, [sx, -5.2, 0]);
  }
  // Central vectoring tail plane
  makeBox(root, matMetal, [0.2, 1.8, 1.2], [0, -5.0, 0.5]);

  return root;
}

/* =========================================================================
   PROTOTYPE APP SHELL & SWITCHER
   ========================================================================= */

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
    builder: buildVariantA,
    dimensions: '26.0 m × 5.8 m × 3.2 m',
    maxSpan: '5.8 m (Radiators)',
    silhouetteType: 'Hammerhead bow → Armored waist → Radiator wings',
    archetype: 'The Expanse: MCRN Corvette / Rocinante class',
    description: 'Classic military gunboat with distinct flared forward bridge wings, cylindrical saddle fuel pods, dorsal armor spine, and wide aft radiator wings.',
    highlights: [
      'Flared 4.8m hammerhead bridge with armored slit viewports',
      'Dual dorsal/ventral torpedo tubes & integrated PDC turrets',
      'Twin cylindrical conformal propellant saddle tanks',
      'Massive 5.8m lateral radiator wings with tip RCS quads',
      'Twin-fusion torch bells in heavy structural thrust block',
    ],
  },
  {
    key: 'B',
    name: 'Caterpillar Ingot',
    subtitle: 'Heavy Industrial Hauler',
    builder: buildVariantB,
    dimensions: '28.0 m × 6.4 m × 3.8 m',
    maxSpan: '6.4 m (Radiator Banks)',
    silhouetteType: 'Bulldozer push-bow → Box carriages → Quad drive block',
    archetype: 'Industrial utility: Caterpillar / Komatsu in space',
    description: 'Chunky, rugged freight hauler with a reinforced push-knee bow, clamped ISO shipping containers, canted outrigger radiator banks, and a brutal quad-thruster transom.',
    highlights: [
      'Reinforced blunt push-bow with heavy impact cushions & work cab',
      'Clamped ISO containers with hazard stripes on port/starboard',
      'Large 6.4m canted outrigger radiator banks with cryo conduits',
      'Heavy octagonal machinery core & central fuel drum',
      'Brutal 2×2 quad-thruster stern block with amber confinement glow',
    ],
  },
  {
    key: 'C',
    name: 'Arrowhead Waverider',
    subtitle: 'Stealth Delta Interceptor',
    builder: buildVariantC,
    dimensions: '30.0 m × 7.0 m × 2.4 m',
    maxSpan: '7.0 m (Engine Sponsons)',
    silhouetteType: 'Razor needle bow → Continuous flaring delta → Wide engine sponsons',
    archetype: 'Stealth predator: Amun-Ra / Protogen black-ops',
    description: 'Low-RCS stealth interceptor with continuous razor-sharp delta chines, faceted radar-scattering armor, flush pop-up weapon bays, and wide 7.0m engine sponsons.',
    highlights: [
      'Razor needle prow with faceted low-RCS dark armor',
      'Continuous flaring delta chines from 1.4m out to 7.0m span',
      'Flush dorsal missile cells with mechanical trapdoors',
      'Conformal pop-up PDC blisters that sit flush with armor',
      'Twin fusion nozzles deeply recessed between heat-shield tails',
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
