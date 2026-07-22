'use client';

import { useEffect, useRef } from 'react';
import * as THREE from 'three';

const TERRAIN_SIZE = 1000;
const TERRAIN_SEGMENTS = 128;
const TERRAIN_COLOUR = 0x4a7c59;
const FOG_LAYER_COLOUR = 0x88ccff;
const FOG_LAYER_OPACITY = 0.3;
const FOG_LAYER_HEIGHT = 50;
const CAMERA_FOV_DEG = 60;
const CAMERA_NEAR = 0.1;
const CAMERA_FAR = 5000;
const CAMERA_RADIUS = 1200;
const CAMERA_MIN_PHI = 0.1;
const CAMERA_MAX_PHI = Math.PI / 2 - 0.05;
const MOUSE_SENSITIVITY = 0.005;
const AMBIENT_INTENSITY = 0.4;
const DIRECTIONAL_INTENSITY = 0.9;
const DIRECTIONAL_POS = { x: 800, y: 1200, z: 600 };
const CLEAR_COLOUR = 0x0a0a0a;

export default function HeightmapViewer() {
  const mountRef = useRef<HTMLDivElement | null>(null);

  useEffect(() => {
    const mount = mountRef.current;
    if (!mount) {
      return;
    }

    const renderer = new THREE.WebGLRenderer({ antialias: true });
    renderer.setPixelRatio(window.devicePixelRatio);
    renderer.setSize(mount.clientWidth, mount.clientHeight);
    renderer.setClearColor(CLEAR_COLOUR, 1);
    mount.appendChild(renderer.domElement);

    const scene = new THREE.Scene();
    const camera = new THREE.PerspectiveCamera(
      CAMERA_FOV_DEG,
      mount.clientWidth / mount.clientHeight,
      CAMERA_NEAR,
      CAMERA_FAR,
    );

    // Spherical camera state.
    let theta = Math.PI / 4;
    let phi = Math.PI / 3;
    const updateCameraFromSpherical = () => {
      const r = CAMERA_RADIUS;
      camera.position.set(
        r * Math.sin(phi) * Math.cos(theta),
        r * Math.cos(phi),
        r * Math.sin(phi) * Math.sin(theta),
      );
      camera.lookAt(0, 0, 0);
    };
    updateCameraFromSpherical();

    const terrainGeometry = new THREE.PlaneGeometry(
      TERRAIN_SIZE,
      TERRAIN_SIZE,
      TERRAIN_SEGMENTS,
      TERRAIN_SEGMENTS,
    );
    terrainGeometry.rotateX(-Math.PI / 2);
    const terrainMaterial = new THREE.MeshPhongMaterial({
      color: TERRAIN_COLOUR,
      wireframe: false,
      flatShading: true,
    });
    const terrainMesh = new THREE.Mesh(terrainGeometry, terrainMaterial);
    scene.add(terrainMesh);

    const fogGeometry = new THREE.PlaneGeometry(TERRAIN_SIZE, TERRAIN_SIZE);
    fogGeometry.rotateX(-Math.PI / 2);
    const fogMaterial = new THREE.MeshBasicMaterial({
      color: FOG_LAYER_COLOUR,
      opacity: FOG_LAYER_OPACITY,
      transparent: true,
      side: THREE.DoubleSide,
    });
    const fogMesh = new THREE.Mesh(fogGeometry, fogMaterial);
    fogMesh.position.y = FOG_LAYER_HEIGHT;
    scene.add(fogMesh);

    scene.add(new THREE.AmbientLight(0xffffff, AMBIENT_INTENSITY));
    const directional = new THREE.DirectionalLight(0xffffff, DIRECTIONAL_INTENSITY);
    directional.position.set(DIRECTIONAL_POS.x, DIRECTIONAL_POS.y, DIRECTIONAL_POS.z);
    scene.add(directional);

    // Minimal orbit-style drag control.
    let dragging = false;
    let lastX = 0;
    let lastY = 0;
    const onMouseDown = (event: MouseEvent) => {
      dragging = true;
      lastX = event.clientX;
      lastY = event.clientY;
    };
    const onMouseMove = (event: MouseEvent) => {
      if (!dragging) {
        return;
      }
      const dx = event.clientX - lastX;
      const dy = event.clientY - lastY;
      lastX = event.clientX;
      lastY = event.clientY;
      theta -= dx * MOUSE_SENSITIVITY;
      phi = Math.min(CAMERA_MAX_PHI, Math.max(CAMERA_MIN_PHI, phi - dy * MOUSE_SENSITIVITY));
      updateCameraFromSpherical();
    };
    const onMouseUp = () => {
      dragging = false;
    };
    renderer.domElement.addEventListener('mousedown', onMouseDown);
    window.addEventListener('mousemove', onMouseMove);
    window.addEventListener('mouseup', onMouseUp);

    const onResize = () => {
      if (!mount) {
        return;
      }
      const w = mount.clientWidth;
      const h = mount.clientHeight;
      camera.aspect = w / h;
      camera.updateProjectionMatrix();
      renderer.setSize(w, h);
    };
    window.addEventListener('resize', onResize);

    let frameId = 0;
    const renderLoop = () => {
      frameId = requestAnimationFrame(renderLoop);
      renderer.render(scene, camera);
    };
    renderLoop();

    return () => {
      cancelAnimationFrame(frameId);
      renderer.domElement.removeEventListener('mousedown', onMouseDown);
      window.removeEventListener('mousemove', onMouseMove);
      window.removeEventListener('mouseup', onMouseUp);
      window.removeEventListener('resize', onResize);
      terrainGeometry.dispose();
      terrainMaterial.dispose();
      fogGeometry.dispose();
      fogMaterial.dispose();
      renderer.dispose();
      if (renderer.domElement.parentNode === mount) {
        mount.removeChild(renderer.domElement);
      }
    };
  }, []);

  return (
    <div ref={mountRef} style={{ width: '100%', height: '100%', position: 'relative' }}>
      <div
        style={{
          position: 'absolute',
          top: 12,
          left: 12,
          padding: '8px 12px',
          background: 'rgba(0,0,0,0.55)',
          color: '#eaeaea',
          font: '13px system-ui, sans-serif',
          borderRadius: 4,
          pointerEvents: 'none',
        }}
      >
        <div>Mahlanya World Viewer &mdash; Eswatini DEM</div>
        <div>Fog layer: live UAV data</div>
      </div>
    </div>
  );
}
