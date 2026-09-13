import * as THREE from 'three';
import { OrbitControls } from 'three/examples/jsm/controls/OrbitControls.js';

import { build as buildSpine } from './parts/spine_truss_m';
import { build as buildDrive } from './parts/drive_fusion_main';
import { build as buildTank } from './parts/tank_drum_1';
import { build as buildNose } from './parts/nose_ogive';
import { build as buildDesign } from './designs/truss_proof';

interface ModelDef {
  id: string;
  name: string;
  category: string;
  builder: () => THREE.Object3D;
  desc: string;
  stats: Record<string, string | number>;
}

const MODELS: ModelDef[] = [
  {
    id: 'design_truss_proof',
    name: 'Truss Proof Ship (Assembled)',
    category: 'Designs',
    builder: buildDesign,
    desc: '4-part modular assembly (§4.8): spine_truss_m + drive_fusion_main + tank_drum_1 + nose_ogive. 31.9 m length. Expanse-style faceted industrial hull.',
    stats: {
      'Length': '31.9 m',
      'Width': '6.0 m',
      'Height': '3.0 m',
      'Mass': '12.2 t',
      'Thrust': '1200 kN',
      'Propellant': '20.0 t',
      'Triangles': '7696 (<= 8000 ok)',
      'Complexity': '694.9 (>= 60 ok)',
      'Section CV': '0.30 (>= 0.20 ok)',
      'Top Material': 'metal 31% (<= 45% ok)',
    },
  },
  {
    id: 'spine_truss_m',
    name: 'Spine: Medium Truss (20 m)',
    category: 'Spines',
    builder: buildSpine,
    desc: '5-station open truss at 4 m pitch. Square cross-section with diagonal lattice bracing, central conduit, and structural hardpoint collars.',
    stats: {
      'Stations': '5 @ 4.0 m pitch',
      'Envelope': '20.0 m × 3.0 m × 3.0 m',
      'Mass': '3.8 t',
      'Mounts': '2 axial flanges (1.25 m)',
      'Family': 'truss (half-width 1.5 m)',
    },
  },
  {
    id: 'drive_fusion_main',
    name: 'Drive: Fusion Main (4 m)',
    category: 'Drives',
    builder: buildDrive,
    desc: '1 axial station. Faceted hex nozzle bell, square thrust frame, radiator panels on strut arms, RCS thruster quads. Flame effect at bell mouth.',
    stats: {
      'Envelope': '4.0 m × 5.7 m × 4.5 m',
      'Mass': '5.0 t',
      'Thrust': '1200 kN (aft)',
      'Interface': '1.25 m flange at -Y',
      'Radiators': '4 dark panels on strut arms',
    },
  },
  {
    id: 'tank_drum_1',
    name: 'Tank: Drum 1 (4 m)',
    category: 'Tanks',
    builder: buildTank,
    desc: '1 axial station. Faceted hex pressure vessel, structural banding rings, fill/drain manifold, level sensor strip.',
    stats: {
      'Envelope': '4.0 m × 2.7 m × 2.7 m',
      'Mass': '1.8 t (dry)',
      'Propellant': '20.0 t LH2',
      'Interface': 'Dual 1.25 m flanges (fore & aft)',
    },
  },
  {
    id: 'nose_ogive',
    name: 'Cap: Nose Ogive (4 m)',
    category: 'Caps & Hab',
    builder: buildNose,
    desc: '1 axial station. Blunt faceted hex wedge, flat sensor face, bridge windows, antenna mast, nav lights. No aerodynamic shaping.',
    stats: {
      'Envelope': '4.0 m × 2.6 m × 2.6 m',
      'Mass': '1.6 t',
      'Hull': '20 HP',
      'Interface': '1.25 m flange at base (-Y)',
      'Sensors': 'Forward aperture, antenna mast',
    },
  },
];

// Three.js Scene Setup
const canvas = document.getElementById('webgl-canvas') as HTMLCanvasElement;
const renderer = new THREE.WebGLRenderer({ canvas, antialias: true, alpha: false, preserveDrawingBuffer: true });
renderer.setPixelRatio(Math.min(window.devicePixelRatio, 2));
renderer.setSize(window.innerWidth, window.innerHeight);
renderer.toneMapping = THREE.ACESFilmicToneMapping;
renderer.toneMappingExposure = 1.15;
renderer.shadowMap.enabled = true;

const scene = new THREE.Scene();
scene.background = new THREE.Color('#0b0e14');

// Lighting: key sun + soft fill + subtle rim
const dirLight = new THREE.DirectionalLight('#ffffff', 2.4);
dirLight.position.set(25, 40, 35);
dirLight.castShadow = true;
scene.add(dirLight);

const fillLight = new THREE.DirectionalLight('#6080a0', 0.9);
fillLight.position.set(-30, -20, 15);
scene.add(fillLight);

const ambientLight = new THREE.AmbientLight('#263238', 1.0);
scene.add(ambientLight);

// Grid and coordinate helpers
const gridHelper = new THREE.GridHelper(50, 50, '#2d4253', '#16202c');
gridHelper.position.y = -2.5;
gridHelper.rotation.x = 0; // Grid on XZ plane
scene.add(gridHelper);

// Cameras: Perspective & Orthographic
const aspect = window.innerWidth / window.innerHeight;
const persCam = new THREE.PerspectiveCamera(45, aspect, 0.1, 500);
persCam.position.set(22, 28, 22);

const orthoSize = 25;
const orthoCam = new THREE.OrthographicCamera(
  -orthoSize * aspect, orthoSize * aspect,
  orthoSize, -orthoSize,
  0.1, 500,
);
orthoCam.position.set(0, 0, 45); // Looking down Z onto XY plane (plan view)

let activeCam: THREE.Camera = persCam;
const controls = new OrbitControls(persCam, canvas);
controls.enableDamping = true;
controls.dampingFactor = 0.08;

let currentModelGroup: THREE.Object3D | null = null;
let currentDef: ModelDef = MODELS[0];

function loadModel(def: ModelDef) {
  if (currentModelGroup) {
    scene.remove(currentModelGroup);
  }
  currentDef = def;
  currentModelGroup = def.builder();
  currentModelGroup.traverse((n) => {
    if ((n as THREE.Mesh).isMesh) {
      n.castShadow = true;
      n.receiveShadow = true;
      // Show effect nodes faintly in authoring preview
      if (n.userData.effect) {
        n.visible = true;
      }
    }
  });
  scene.add(currentModelGroup);
  updateUI();
  setView('iso');
}

function updateUI() {
  const title = document.getElementById('model-title')!;
  const desc = document.getElementById('model-desc')!;
  const statsList = document.getElementById('model-stats')!;
  title.innerText = currentDef.name;
  desc.innerText = currentDef.desc;

  statsList.innerHTML = Object.entries(currentDef.stats)
    .map(([k, v]) => `<div class="stat-row"><span class="stat-key">${k}</span><span class="stat-val">${v}</span></div>`)
    .join('');

  document.querySelectorAll('.model-btn').forEach((btn) => {
    btn.classList.toggle('active', btn.getAttribute('data-id') === currentDef.id);
  });
}

// Preset Camera Views
function setView(view: string) {
  const isShip = currentDef.id === 'design_truss_proof';
  const targetY = isShip ? 2.0 : (currentDef.id === 'spine_truss_m' ? 0.0 : 2.0);
  const dist = isShip ? 46 : (currentDef.id === 'spine_truss_m' ? 32 : 12);

  controls.target.set(0, targetY, 0);

  if (view === 'iso') {
    persCam.position.set(dist * 0.7, dist * 0.7, dist * 0.75);
  } else if (view === 'plan') {
    // Top-down Plan View (looking down +Z onto XY plane, nose +Y up)
    persCam.position.set(0, targetY + 0.001, dist * 1.15);
  } else if (view === 'side') {
    // Side view (+X to -X, showing nose to +Y and belly/dorsal)
    persCam.position.set(dist * 1.1, targetY, 0);
  } else if (view === 'aft') {
    // Looking from aft up toward drive and nose (+Y)
    persCam.position.set(0, targetY - dist * 0.9, dist * 0.4);
  }
  controls.update();
}

// Global hook for headless automation
(window as any).previewAPI = {
  selectModel: (id: string) => {
    const found = MODELS.find((m) => m.id === id);
    if (found) loadModel(found);
  },
  setView,
  models: MODELS.map((m) => ({ id: m.id, name: m.name })),
};

// UI Listeners
document.getElementById('model-selector')!.innerHTML = MODELS.map(
  (m) => `<button class="model-btn" data-id="${m.id}">${m.name}</button>`
).join('');

document.querySelectorAll('.model-btn').forEach((btn) => {
  btn.addEventListener('click', () => {
    const id = btn.getAttribute('data-id')!;
    const found = MODELS.find((m) => m.id === id);
    if (found) loadModel(found);
  });
});

document.querySelectorAll('.view-btn').forEach((btn) => {
  btn.addEventListener('click', () => {
    setView(btn.getAttribute('data-view')!);
  });
});

window.addEventListener('resize', () => {
  const w = window.innerWidth, h = window.innerHeight;
  persCam.aspect = w / h;
  persCam.updateProjectionMatrix();
  renderer.setSize(w, h);
});

// Animation Loop
loadModel(MODELS[0]);
function animate() {
  requestAnimationFrame(animate);
  controls.update();
  renderer.render(scene, activeCam);
}
animate();
