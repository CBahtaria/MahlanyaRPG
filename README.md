# Mahlanya RPG

A Swazi historical 3D RPG spanning multiple eras of Eswatini's history (pre-colonial formation ~1750 through colonial resistance ~1906).

The game's central differentiator is that its environment does not *look* realistic — it *behaves* with the physical, geological, climatological, acoustic, and social reality of the actual Kingdom of Eswatini.

**Protagonist:** Mahlanya ("the reckless/daring one") 
**Engine:** Unreal Engine 5.4+ 
**Language:** C++ + Blueprint 
**Platforms:** PC (primary), iOS, Android (pre-baked tier)

---

## Architecture

Four interlocking layers:

```
LAYER 1 — OFFLINE SCIENCE PIPELINE (Python)
  Runs before game ships. Produces world artifacts from real GIS data.
  Tools: GDAL, rasterio, NumPy, SciPy, CuPy, shapely, geopandas

LAYER 2 — UE5 RUNTIME CORE (C++ + Blueprint)
  Nanite, Lumen, World Partition, PCG Framework, Chaos Physics, MetaSounds, GAS

LAYER 3 — CUSTOM ALGORITHM PLUGINS (9 UE5 C++ Plugins)
  Exact science where UE5 built-ins approximate

LAYER 4 — MOBILE ARTIFACT CONSUMER
  Consumes pre-baked artifacts from Layer 1. No heavy runtime compute.
```

## Custom Plugins (9)

| Plugin | Purpose | Load Phase |
|---|---|---|
| `SimulationBusPlugin` | Typed multicast event bus (no deps) | **PreDefault** |
| `ErosionRuntimePlugin` | Runtime terrain erosion compute shader | Default |
| `SibayaEngine` | Umuti settlement Voronoi tessellation | Default |
| `LocomotionPhysicsPlugin` | Anisotropic friction, slip/recovery | Default |
| `MicroclimateEngine` | Orographic rain, valley fog, lightning | Default |
| `GeometricAudioPlugin` | 64-ray acoustic ray-tracer | Default |
| `KnowledgeGraphPlugin` | Swazi historical knowledge graph runtime | Default |
| `EconomySimulatorPlugin` | Cattle-based political economy sim | Default |
| `EmergentNarrativePlugin` | Simulation-driven quest generation | PostDefault |

---

## Simulation Systems (10)

1. **Geophysical Terrain Synthesis** — Full erosion stack on Copernicus DEM GLO-10 real data
2. **Umuti Settlement Engine** — Constrained Centroidal Voronoi with Swazi social geometry
3. **Kinetic Locomotion** — Anisotropic terrain friction tensors, biomechanical fatigue, Chaos slip
4. **Atmospheric Microclimates** — Hosek-Wilkie sky, orographic rainfall, valley inversion
5. **Acoustic Wave Ray-Tracing** — 64-ray geometric audio with per-material 8-octave absorption
6. **Historical Knowledge Graph** — Neo4j-compatible, CI/CD enforced accuracy gate
7. **Living Political Economy** — Cattle-based economy with colonial concession infection spread
8. **Language & Cultural Protocol** — siSwati morphology, Inhlonipho avoidance vocabulary
9. **Ecological Simulation** — Lotka-Volterra ODEs, seasonal fauna migration
10. **Emergent Narrative Engine** — Simulation-threshold-triggered quest generation

---

## Quick Start

### Python Pipeline

```bash
# Requires Python 3.11+ and system GDAL for terrain stages
cd pipeline

# Install core dependencies
pip install -r requirements.txt

# Run all tests
make test

# Full pipeline (terrain → history, ~22 hours on RTX 4090)
make full-pipeline

# Individual stages
make terrain-acquire    # Download Copernicus DEM GLO-10 tiles
make terrain-erode      # 5000-iteration erosion stack (GPU-accelerated)
make terrain-rivers     # D-infinity river network extraction
make terrain-export     # 1009×1009 UE5 heightmap tile export
make settlements-voronoi
make history-graph
make history-validate   # CI/CD content accuracy gate

# Help
make help
```

### GPU Acceleration (NVIDIA)

```bash
pip install cupy-cuda12x  # For RTX GPU erosion (≈4h vs ≈18h CPU)
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
- `pipeline/history/validate_content.py` — CI/CD gate that exits code 1 on any anachronism
- GitHub Actions `content-validate.yml` — runs on every PR touching `Content/Dialogue/**`

```bash
# Validate all dialogue locally
python pipeline/history/validate_content.py \
    Content/Dialogue \
    pipeline/history/data/knowledge_graph.ndjson
```

Historical span: **1750–1906** (pre-colonial formation through colonial resistance)

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

| Feature | PC Ultra | Mobile High | Mobile Low |
|---|---|---|---|
| Terrain erosion | Runtime GPU | Pre-baked | Pre-baked |
| Nanite | Enabled | Disabled | Disabled |
| Lumen | Enabled | Disabled | Disabled |
| Volumetric clouds | Full sim | Static mesh | Skybox |
| Settlement Voronoi | Runtime | Static JSON | Static JSON |
| Geometric audio | 64 rays | Pre-baked IR | Stereo bus |
| Shadow quality | Ray-traced | CSM 4 | CSM 2 |
| Draw distance | 8km | 2km | 1km |

---

## CI/CD

| Workflow | Trigger | Purpose |
|---|---|---|
| `pipeline-ci.yml` | `pipeline/**` changes | Python test suite + coverage gate |
| `content-validate.yml` | `Content/Dialogue/**` changes | Historical accuracy gate |
| `ue5-build.yml` | `Source/**` or `Plugins/**` changes | UE5 compile + automation tests |

UE5 build requires a self-hosted runner with UE5.4+ at `$UE5_ROOT`. Set the `UE5_ROOT` repository variable to your installation path.

---

## Implementation Phases

| Phase | Duration | Deliverable |
|---|---|---|
| **0 — Foundation** *(current)* | 8 weeks | Scaffold, CI/CD, Python env |
| 1 — Terrain | 16 weeks | Eroded Eswatini heightmap in UE5 |
| 2 — Locomotion | 12 weeks | Physics-accurate movement on terrain |
| 3 — Settlement | 12 weeks | Procedural Umuti villages |
| 4 — Atmosphere | 12 weeks | Full atmospheric physics + bioacoustics |
| 5 — Audio | 10 weeks | Geometric acoustic system |
| 6 — Historical World | 16 weeks | Knowledge graph, economy, NPC protocols |
| 7 — Narrative | 16 weeks | Emergent quest engine |
| 8 — Mobile | 12 weeks | Pre-baked artifact mobile build |
| 9 — Co-op | 16 weeks | 2–4 player co-op layer |
| 10 — Ship | 8 weeks | Platform certification |

---

## Project Structure

```
MahlanyaRPG/
├── pipeline/                    # Python offline science pipeline
│   ├── terrain/                 # DEM → erosion → UE5 export
│   ├── settlements/             # Voronoi + political graph
│   ├── atmosphere/              # Sky LUTs + weather tables
│   ├── audio/                   # Acoustic IRs + propagation
│   ├── history/                 # Knowledge graph + validator
│   │   └── data/               # Historical data JSON files
│   ├── tests/                   # pytest test suite
│   ├── Makefile                 # make full-pipeline
│   └── requirements.txt
├── Plugins/                     # 9 custom UE5 plugins
│   ├── SimulationBusPlugin/     # PreDefault event bus
│   ├── SibayaEngine/            # Settlement Voronoi
│   ├── LocomotionPhysicsPlugin/ # Terrain friction physics
│   └── ...
├── Source/MahlanyaRPG/          # UE5 game module
├── Content/                     # UE5 assets (Git LFS)
├── Config/                      # Engine/game/scalability config
├── .github/workflows/           # CI/CD pipelines
├── docs/superpowers/
│   ├── specs/                   # Design specifications
│   └── plans/                   # Implementation plans
├── .gitignore
├── .gitattributes               # Git LFS for .uasset, .r16, .exr, .wav
└── MahlanyaRPG.uproject
```

---

*"Mahlanya" — reckless, daring one. The name carries its history in its morphology.*
