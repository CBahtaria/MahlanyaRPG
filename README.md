# Mahlanya RPG

[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)
[![Python](https://img.shields.io/badge/Python-3.11%2B-blue)](pipeline/requirements.txt)
[![Zig](https://img.shields.io/badge/Zig-0.13%2B-orange)](pipeline/compute/)
[![UE5](https://img.shields.io/badge/Unreal%20Engine-5.4%2B-black)](MahlanyaRPG.uproject)

A Swazi historical 3D RPG spanning multiple eras of Eswatini's history — pre-colonial formation (~1750) through colonial resistance (~1906).

The game's central differentiator is that its environment does not *look* realistic — it *behaves* with the physical, geological, climatological, acoustic, and social reality of the actual Kingdom of Eswatini.

**Protagonist:** Mahlanya ("the reckless/daring one") 
**Engine:** Unreal Engine 5.4+ 
**Language:** C++ + Blueprint 
**Platforms:** PC (primary), iOS, Android (pre-baked tier), Console (planned)

---

## Architecture

Four interlocking layers:

```
LAYER 1 — OFFLINE SCIENCE PIPELINE (Trilingual)
  Python  — I/O orchestration, network, GDAL, content validation
  Zig     — Native SIMD compute: erosion, D-infinity, Voronoi (libmahlanya_compute.so)
  C++ UE5 Commandlets — Pipeline stages with direct UE5 asset output (MahlanyaPipelinePlugin)

LAYER 2 — UE5 RUNTIME CORE (C++ + Blueprint)
  Nanite, Lumen, World Partition, PCG Framework, Chaos Physics, MetaSounds, GAS

LAYER 3 — CUSTOM ALGORITHM PLUGINS (11 UE5 C++ Plugins)
  Exact science where UE5 built-ins approximate

LAYER 4 — MOBILE ARTIFACT CONSUMER
  Consumes pre-baked artifacts from Layer 1. No heavy runtime compute.
```

## Custom Plugins (11)

| Plugin | Purpose | Load Phase |
|---|---|---|
| `SimulationBusPlugin` | Typed multicast event bus (no deps) | **PreDefault** |
| `MahlanyaPipelinePlugin` | Editor-only C++ commandlets wrapping Zig kernels | Default (Editor) |
| `ErosionRuntimePlugin` | Runtime terrain erosion compute shader (ErosionCS.usf) | Default |
| `SibayaEngine` | Umuti settlement Voronoi tessellation + political graph | Default |
| `LocomotionPhysicsPlugin` | Anisotropic friction tensors, altitude fatigue, Chaos slip | Default |
| `MicroclimateEngine` | Orographic rain, valley fog, lightning, stealth visibility | Default |
| `GeometricAudioPlugin` | 64-ray acoustic ray-tracer, atmospheric propagation | Default |
| `KnowledgeGraphPlugin` | Swazi historical knowledge graph runtime queries | Default |
| `EconomySimulatorPlugin` | Cattle-based political economy, concession spread | Default |
| `CulturalProtocolPlugin` | siSwati morphology, protocol state machine, Inhlonipho | Default |
| `EcologySimulatorPlugin` | Lotka-Volterra ODE fauna grid, flora distribution | Default |
| `EmergentNarrativePlugin` | Simulation-threshold-triggered quest generation | **PostDefault** |

---

## Simulation Systems (10)

1. **Geophysical Terrain Synthesis** — Full geomorphological erosion stack on Copernicus DEM GLO-10 real data (thermal, fluvial, aeolian, mass-wasting); D-infinity river extraction; 1009×1009 UE5 heightmap tiles
2. **Umuti Settlement Engine** — Constrained Centroidal Voronoi with Swazi social geometry (Chief faces East, taboo West arc, wife seniority encodes distance); runtime recomputation on demographic events
3. **Kinetic Locomotion** — Anisotropic terrain friction tensors per material × wetness state, biomechanical altitude fatigue, Chaos slip/recovery, footprint depth physics
4. **Atmospheric Microclimates** — Hosek-Wilkie spectral sky LUTs, barometric pressure state machine, orographic rainfall, valley temperature inversion fog, lightning charge accumulation
5. **Acoustic Wave Ray-Tracing** — 64-ray geometric audio with per-material 8-octave absorption coefficients; atmospheric propagation tables; MetaSounds integration
6. **Historical Knowledge Graph** — Neo4j-compatible; CI/CD enforced accuracy gate; prevents historically inconsistent NPC dialogue at build time
7. **Living Political Economy** — Cattle-based economy with drought stress, lobola transactions, cattle raid emergence, colonial concession infection spread
8. **Language & Cultural Protocol** — siSwati agglutinative morphology engine; Inhlonipho avoidance vocabulary; protocol state machine with relationship impact
9. **Ecological Simulation** — Lotka-Volterra ODE system for 6 prey-predator pairs on a 10×10km grid; seasonal fauna migration; medicinal plant registry
10. **Emergent Narrative Engine** — Simulation-threshold-triggered quest generation + historical calendar BFS event dependency tree + procedural Imbongi praise poet

---

## Phase 10 — Production Optimization Systems

Completed in the most recent development phase. Addresses six production-readiness gaps:

| Component | Class | Purpose |
|---|---|---|
| `UHardwareAdaptiveScaler` | `UGameInstanceSubsystem` | Detects hardware tier (LowEnd/MidRange/HighEnd/Ultra) and writes simulation CVars |
| `USimulationDeviceSettings` | `UDeveloperSettings` | Per-platform config overrides (PS5, XSX, Switch, Steam Deck, Android tiers, iOS) |
| `FSimulationTrustMatrix` | USTRUCT | Thread-safe replication trust tracking with decay, extrapolation, and severity states |
| `ASimulationReplicationManager` | `AActor` (replicated) | Bandwidth-token-bucket co-op sync with trust degradation delegates |
| `UDynamicRuntimeThrottle` | `UWorldSubsystem` | Rolling 60-frame performance monitor; 4-state throttle with hysteresis de-escalation |
| `UYearChangeOrchestrator` | `UWorldSubsystem` + `FTickableGameObject` | Spreads historical BFS events across ticks within a ms budget; fixed-size event pool |
| `FSimulationTracer` | Static helpers | Unreal Insights trace channel (`SimulationChannel`) with CPU stat groups per sim phase |

**New pipeline tools:**
- `pipeline/advanced_validator.py` — Parallel referential integrity, genealogy cycle detection, cultural constraint validation (exogamy rule)
- `pipeline/validate_configs.py` — Build-time config validator; exits 1 if CVars/log channels/device profiles are below minimum counts
- `pipeline/check_regression.py` — Performance regression gate; fails if any metric regresses >15% from baseline

---

## Quick Start

### Python Pipeline

```bash
# Requires Python 3.11+ and system GDAL for terrain stages
cd pipeline

# Install core dependencies
pip install -r requirements.txt

# Build Zig compute kernels (required for terrain + settlements)
cd compute && zig build && cd ..

# Run all tests
make test

# Full pipeline (terrain → history, ~22h on RTX 4090)
make full-pipeline

# Individual stages
make terrain-acquire     # Download Copernicus DEM GLO-10 tiles
make terrain-erode       # 5000-iteration erosion stack (GPU-accelerated)
make terrain-rivers      # D-infinity river network extraction
make terrain-export      # 1009×1009 UE5 heightmap tile export
make settlements-voronoi
make history-graph
make history-validate    # CI/CD content accuracy gate

# Build-time config validation (run before UE5 compile)
python pipeline/validate_configs.py

# Performance regression check
python pipeline/check_regression.py baseline.json current.json

make help
```

### GPU Acceleration (NVIDIA)

```bash
pip install cupy-cuda12x  # RTX GPU erosion: ~4h vs ~18h CPU
```

### UE5 Project

1. Install Unreal Engine 5.4+
2. Open `MahlanyaRPG.uproject`
3. Plugin load order is enforced: `SimulationBusPlugin` → all simulation plugins → `EmergentNarrativePlugin`
4. Run `make full-pipeline` to generate terrain artifacts before cooking

---

## Historical Accuracy

Historical integrity is structurally enforced — not via manual review:

- `pipeline/history/data/` — Swazi historical knowledge graph (persons, places, battles, events, relations)
- `pipeline/history/validate_content.py` — CI/CD gate; exits code 1 on any anachronism or unsupported claim
- `pipeline/advanced_validator.py` — Referential integrity, genealogy cycles, same-clan marriage detection
- GitHub Actions `content-validate.yml` — runs on every PR touching `Content/Dialogue/**`

```bash
# Validate all dialogue locally
python pipeline/history/validate_content.py \
    Content/Dialogue \
    pipeline/history/data/knowledge_graph.ndjson

# Run advanced validator on historical data
python pipeline/advanced_validator.py pipeline/history/data/
```

Historical span: **1750–1906** (pre-colonial Swazi formation through colonial resistance)

---

## GIS Data Sources

| Dataset | Source | Resolution |
|---|---|---|
| Digital Elevation Model | Copernicus DEM GLO-10 | 10m/pixel |
| Geology (rock hardness) | Council for Geoscience South Africa | Vector |
| Climatology | SAWS (South African Weather Service) | Station data |
| Coordinate system | UTM Zone 36S (EPSG:32736) | Metric |

Eswatini bounding box: 30.79°E–32.14°E, 27.32°S–25.72°S

---

## Mobile Scalability

| Feature | PC Ultra | High End | Mobile High | Mobile Low |
|---|---|---|---|---|
| Terrain erosion | Runtime GPU | Runtime GPU | Pre-baked | Pre-baked |
| Nanite | Enabled | Enabled | Disabled | Disabled |
| Lumen | Enabled | Enabled | Disabled | Disabled |
| Volumetric clouds | Full sim | Full sim | Static mesh | Skybox |
| Settlement Voronoi | Runtime | Runtime | Static JSON | Static JSON |
| Geometric audio | 64 rays | 32 rays | Pre-baked IR | Stereo bus |
| Shadow quality | Ray-traced | CSM 4 | CSM 4 | CSM 2 |
| Draw distance | 8km | 6km | 2km | 1km |
| Max active NPCs | 200 | 100 | 35–50 | 12–25 |
| Replication frequency | 60 Hz | 30 Hz | 20–25 Hz | 8 Hz |

Hardware tier detection and CVar writes handled automatically by `UHardwareAdaptiveScaler` on startup.

---

## CI/CD

| Workflow | Trigger | Purpose |
|---|---|---|
| `pipeline-ci.yml` | `pipeline/**` changes | Python test suite (296+ tests) + coverage gate |
| `content-validate.yml` | `Content/Dialogue/**` changes | Historical accuracy gate |
| `ue5-build.yml` | `Source/**` or `Plugins/**` changes | UE5 compile + automation tests |
| `phase10-validation.yml` | `Source/MahlanyaRPG/Performance/**`, `Config/**` | CVar count, log channel count, device profile count, subsystem registration, JSON validity |

UE5 build requires a self-hosted runner with UE5.4+ at `$UE5_ROOT`. Set the `UE5_ROOT` repository variable to your installation path.

---

## Project Status

**This is a technical foundation, not yet a playable game.**

The offline science pipeline, Zig compute kernels, C++ plugin source, and UE5 project configuration are complete. No game content exists yet (no maps, character meshes, animations, or playable level). Getting to a playable vertical slice requires a machine with UE5.4+, NVIDIA GPU, ~22h to run the terrain pipeline, and significant content production work.

### What exists

| Component | State |
|---|---|
| Python science pipeline | ✅ 310 tests pass |
| Zig SIMD compute kernels | ✅ `libmahlanya_compute.so` builds |
| C++ plugin source (11 plugins) | ✅ Written, not yet compiled in UE5 |
| UE5 project file (`MahlanyaRPG.uproject`) | ✅ Written |
| CI/CD workflows | ✅ Active |
| Config files (scalability, device profiles) | ✅ Written |
| Historical data (persons, relations, events) | ✅ Validated |

### What does not exist yet

| Component | Needed for |
|---|---|
| UE5 project compiled and opened | Everything |
| Terrain heightmap (make full-pipeline output) | Any world geometry |
| Character skeletal mesh / animations | Protagonist Mahlanya |
| Any maps or levels | Playable content |
| NPC meshes, settlement assets, flora/fauna | World population |
| Game UI | Menus, HUD, dialogue |

### Milestones to first playable

1. **Bootstrap** — Compile C++ source in UE5.4 on a development machine
2. **Terrain** — Run `make full-pipeline` on a machine with NVIDIA GPU + GDAL; import heightmap tiles into a UE5 Landscape
3. **Character** — Create or licence skeletal mesh + animations for Mahlanya; wire ABP, Control Rig, locomotion plugin
4. **First level** — Assemble one playable area (Middleveld valley) with terrain, lighting, and basic movement
5. **Settlement** — Spawn one Umuti via PCG + SibayaEngine; verify Voronoi layout
6. **Content production** — 2–3 years of asset, level, narrative, and audio production work

## Implementation Phases

| Phase | Deliverable | Status |
|---|---|---|
| **0 — Foundation** | Scaffold, Git LFS, CI/CD, Python env | ✅ Source complete |
| **1 — Terrain** | Trilingual pipeline (Python + Zig + C++ commandlets) | ✅ Source complete |
| **2 — Locomotion** | Anisotropic friction, altitude fatigue, Chaos slip, footprint physics | ✅ Source complete |
| **3 — Settlement** | SibayaEngine; Voronoi Umuti villages; political graph | ✅ Source complete |
| **4 — Atmosphere** | Hosek-Wilkie sky, MicroclimateEngine, lightning, controlled burns | ✅ Source complete |
| **5 — Audio** | 64-ray geometric audio, acoustic IRs, bioacoustics, MetaSounds | ✅ Source complete |
| **6 — Historical World** | Knowledge graph, economy simulator, emergent narrative | ✅ Source complete |
| **7 — Ecology & Culture** | Lotka-Volterra fauna grid, siSwati protocol engine | ✅ Source complete |
| **8 — Mobile** | Artifact packager, `UMahlanyaScalabilitySubsystem`, mobile tiers | ✅ Source complete |
| **9 — Co-op** | `AMahlanyaGameState`, `UMahlanyaCoopBridgeComponent`, GAS replication | ✅ Source complete |
| **10 — Production** | Hardware scaler, runtime throttle, trust matrix, year-change orchestrator, Insights tracing | ✅ Source complete |
| **11 — River & Cinematics** | Usuthu canoe traversal, `ACinematicDialogueDirector`, bilingual subtitle renderer, emabutfo armor pipeline | 🔲 Specced |
| **Bootstrap UE5** | Compile + open project; import first terrain tile | 🔲 Next |
| **First Playable** | Mahlanya moves through one Middleveld area | 🔲 Planned |
| **Ship** | Platform certification | 🔲 Planned |

---

## Project Structure

```
MahlanyaRPG/
├── pipeline/                      # Offline science pipeline
│   ├── compute/                   # Zig SIMD library (libmahlanya_compute.so)
│   │   ├── build.zig
│   │   └── src/
│   │       ├── erosion.zig        # 4 erosion passes with @Vector SIMD
│   │       ├── dinf.zig           # Tarboton D-infinity flow accumulation
│   │       ├── voronoi.zig        # Lloyd relaxation + Centroidal Voronoi
│   │       └── root.zig           # C ABI exports
│   ├── terrain/                   # DEM → erosion → river extraction → UE5 export
│   ├── settlements/               # Voronoi + political graph
│   ├── atmosphere/                # Hosek-Wilkie LUTs + SAWS weather tables
│   ├── audio/                     # Acoustic IRs + bioacoustic library
│   ├── history/                   # Knowledge graph + content validator
│   │   └── data/                  # persons.json, relations.json, events.json …
│   ├── advanced_validator.py      # Parallel referential integrity + genealogy checks
│   ├── validate_configs.py        # Build-time CVar/channel/profile count gate
│   ├── check_regression.py        # Performance regression detector
│   ├── tests/                     # pytest suite (296+ tests)
│   ├── Makefile
│   └── requirements.txt
├── Plugins/                       # 11 custom UE5 plugins
│   ├── SimulationBusPlugin/       # PreDefault event bus
│   ├── MahlanyaPipelinePlugin/    # Editor-only commandlets + ZigComputeBridge
│   ├── SibayaEngine/              # Settlement Voronoi
│   ├── LocomotionPhysicsPlugin/   # Terrain friction physics
│   ├── MicroclimateEngine/        # Weather + atmospheric simulation
│   ├── GeometricAudioPlugin/      # Acoustic ray-tracer
│   ├── KnowledgeGraphPlugin/      # Historical graph runtime
│   ├── EconomySimulatorPlugin/    # Cattle economy
│   ├── CulturalProtocolPlugin/    # siSwati language + protocols
│   ├── EcologySimulatorPlugin/    # Fauna/flora simulation
│   └── EmergentNarrativePlugin/   # Quest generation + Imbongi
├── Source/
│   ├── MahlanyaRPG.Target.cs      # Game target
│   ├── MahlanyaRPGEditor.Target.cs# Editor target
│   └── MahlanyaRPG/               # UE5 game module
│       ├── MahlanyaRPG.cpp        # IMPLEMENT_PRIMARY_GAME_MODULE
│       ├── MahlanyaRPG.Build.cs
│       ├── Core/MahlanyaLogChannels.h  # 8 log categories
│       ├── Performance/           # Phase 10: hardware scaler, throttle, tracer
│       └── GameFramework/         # Trust matrix, replication manager, year orchestrator
├── Content/                       # UE5 assets (Git LFS)
├── Config/                        # DefaultEngine / DefaultGame / DefaultScalability
│   ├── DefaultEngine.ini          # Subsystem auto-registration
│   ├── DefaultGame.ini            # 11 device profiles
│   ├── DefaultScalability.ini     # 4 MahlanyaSimulation quality groups
│   ├── DefaultInput.ini
│   └── DefaultEditor.ini
├── .github/workflows/             # 4 CI/CD pipelines
├── docs/superpowers/
│   ├── specs/                     # Design specifications
│   └── plans/                     # Implementation plans
├── .gitignore
├── .gitattributes                 # Git LFS for .uasset, .r16, .exr, .wav
├── NOTICE.md                      # Third-party licence attributions
├── LICENSE                        # Proprietary source-available
└── MahlanyaRPG.uproject           # UE5 project descriptor
```

---

## Licence

MIT — see [LICENSE](LICENSE).

Third-party components (Copernicus DEM, Hosek-Wilkie model, Zig, Python libraries, Unreal Engine) retain their own licences. See [NOTICE.md](NOTICE.md) for full attributions.

---

*"Mahlanya" — reckless, daring one. The name carries its history in its morphology.*
