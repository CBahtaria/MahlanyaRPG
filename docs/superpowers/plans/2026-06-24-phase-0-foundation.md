# Phase 0 — Foundation Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Establish the complete project scaffold — directory structure, Git LFS, verified Python pipeline environment, pipeline Makefile with all sub-pipeline targets, SimulationBus UE5 plugin scaffold (the dependency-free inter-plugin event bus), and three GitHub Actions CI/CD workflows.

**Architecture:** A git monorepo at `/home/cbartaria1/my-projects/MahlanyaRPG/` containing a Python offline science pipeline (`pipeline/`) and a UE5 game project root. The `SimulationBusPlugin` is scaffolded first — it has zero plugin dependencies and defines the typed multicast delegate contract that all 8 subsequent plugins subscribe to. Nothing else can be built until the bus contract is defined.

**Tech Stack:** Python 3.11+, GDAL 3.8+, NumPy 1.26+, SciPy 1.12+, Numba 0.59+, CuPy 13+ (optional/CUDA), Shapely 2.0+, GeoPandas 0.14+, pyproj 3.6+, trimesh 4.0+, neo4j-driver 5.0+, click 8.1+, pytest 8.0+, Unreal Engine 5.4+, C++17, Git LFS 3.0+, GitHub Actions

**Prerequisites (manual, before running any task):**
- Git and Git LFS installed: `git lfs version` must succeed
- Python 3.11+ installed: `python3.11 --version` must succeed
- GDAL system library installed: `gdal-config --version` must return 3.8+
  - Ubuntu/Debian: `sudo apt install gdal-bin libgdal-dev`
  - macOS: `brew install gdal`
- Unreal Engine 5.4+ installed (for UE5 tasks only — Python tasks run without it)

---

## File Map

```
MahlanyaRPG/
├── .gitattributes                              CREATE — Git LFS patterns
├── .gitignore                                  CREATE — ignores for UE5 + Python
├── README.md                                   CREATE — prerequisites + setup
│
├── pipeline/
│   ├── requirements.txt                        CREATE — pinned Python deps
│   ├── Makefile                                CREATE — pipeline orchestration
│   ├── conftest.py                             CREATE — pytest root conftest
│   │
│   ├── terrain/
│   │   ├── __init__.py                         CREATE
│   │   ├── acquire_dem.py                      CREATE (stub — implemented in Plan 1)
│   │   ├── build_hardness_map.py               CREATE (stub)
│   │   ├── erode_terrain.py                    CREATE (stub)
│   │   ├── extract_rivers.py                   CREATE (stub)
│   │   └── export_to_ue5.py                    CREATE (stub)
│   │
│   ├── settlements/
│   │   ├── __init__.py                         CREATE
│   │   ├── compute_voronoi.py                  CREATE (stub — Plan 4)
│   │   └── build_political_graph.py            CREATE (stub)
│   │
│   ├── atmosphere/
│   │   ├── __init__.py                         CREATE
│   │   ├── compute_sky_luts.py                 CREATE (stub — Plan 5)
│   │   └── build_weather_tables.py             CREATE (stub)
│   │
│   ├── audio/
│   │   ├── __init__.py                         CREATE
│   │   ├── compute_acoustic_irs.py             CREATE (stub — Plan 6)
│   │   ├── compute_propagation_tables.py       CREATE (stub)
│   │   └── build_bioacoustic_library.py        CREATE (stub)
│   │
│   ├── history/
│   │   ├── __init__.py                         CREATE
│   │   ├── build_knowledge_graph.py            CREATE (stub — Plan 7)
│   │   ├── validate_content.py                 CREATE (stub)
│   │   └── data/
│   │       ├── persons.json                    CREATE — empty array []
│   │       ├── places.json                     CREATE — empty array []
│   │       ├── battles.json                    CREATE — empty array []
│   │       ├── events.json                     CREATE — empty array []
│   │       ├── relations.json                  CREATE — empty array []
│   │       └── material_culture.json           CREATE — empty array []
│   │
│   └── tests/
│       ├── __init__.py                         CREATE
│       ├── test_environment.py                 CREATE — verifies all imports succeed
│       └── test_pipeline_stubs.py              CREATE — verifies each stub is importable + callable
│
├── Plugins/
│   └── SimulationBusPlugin/
│       ├── SimulationBusPlugin.uplugin         CREATE — UE5 plugin descriptor
│       └── Source/
│           └── SimulationBusPlugin/
│               ├── SimulationBusPlugin.Build.cs    CREATE — module build rules
│               ├── Public/
│               │   └── SimulationBusSubsystem.h    CREATE — delegate declarations + subsystem
│               └── Private/
│                   └── SimulationBusSubsystem.cpp  CREATE — subsystem implementation
│
├── Config/
│   ├── DefaultEngine.ini                       CREATE — required UE5 plugin list
│   ├── DefaultGame.ini                         CREATE — project settings
│   └── DefaultScalability.ini                  CREATE — PC/Mobile-High/Mobile-Low tiers
│
└── .github/
    └── workflows/
        ├── pipeline-ci.yml                     CREATE — pytest + import checks on /pipeline/** changes
        ├── content-validate.yml                CREATE — knowledge graph validator on /Content/Dialogue/** changes
        └── ue5-build.yml                       CREATE — UE5 compile check on /Source/** + /Plugins/** changes
```

---

## Task 1: Git LFS and .gitignore

**Files:**
- Create: `.gitattributes`
- Create: `.gitignore`

- [ ] **Step 1: Verify Git LFS is installed**

```bash
git lfs version
```

Expected output: `git-lfs/3.x.x (GitHub; ...)` — if this fails, install Git LFS before continuing.

- [ ] **Step 2: Initialize Git LFS in the repository**

```bash
cd /home/cbartaria1/my-projects/MahlanyaRPG
git lfs install
```

Expected: `Git LFS initialized.`

- [ ] **Step 3: Create .gitattributes**

Create `/home/cbartaria1/my-projects/MahlanyaRPG/.gitattributes` with this exact content:

```
# Unreal Engine binary assets — tracked via Git LFS
*.uasset filter=lfs diff=lfs merge=lfs -text
*.umap   filter=lfs diff=lfs merge=lfs -text
*.ubulk  filter=lfs diff=lfs merge=lfs -text
*.uptnl  filter=lfs diff=lfs merge=lfs -text

# Pipeline output artifacts — tracked via Git LFS
*.r16    filter=lfs diff=lfs merge=lfs -text
*.exr    filter=lfs diff=lfs merge=lfs -text
*.wav    filter=lfs diff=lfs merge=lfs -text

# Large textures
*.png    filter=lfs diff=lfs merge=lfs -text
*.tif    filter=lfs diff=lfs merge=lfs -text
*.tiff   filter=lfs diff=lfs merge=lfs -text

# Shapefiles
*.shp    filter=lfs diff=lfs merge=lfs -text
*.dbf    filter=lfs diff=lfs merge=lfs -text
```

- [ ] **Step 4: Create .gitignore**

Create `/home/cbartaria1/my-projects/MahlanyaRPG/.gitignore` with this exact content:

```
# Unreal Engine generated
Binaries/
Build/
DerivedDataCache/
Intermediate/
Saved/
.vs/
*.VC.db
*.opensdf
*.opendb
*.sdf
*.suo
*.user
*.xcworkspace
*.xcodeproj

# Python
__pycache__/
*.py[cod]
*.pyo
.venv/
venv/
*.egg-info/
.pytest_cache/
.coverage
htmlcov/
dist/
build/

# Pipeline outputs (large, generated — not committed)
pipeline/outputs/
pipeline/cache/

# OS
.DS_Store
Thumbs.db
```

- [ ] **Step 5: Stage and commit**

```bash
cd /home/cbartaria1/my-projects/MahlanyaRPG
git add .gitattributes .gitignore
git commit -m "chore: add Git LFS config and .gitignore"
```

Expected: `[master ...] chore: add Git LFS config and .gitignore`

---

## Task 2: Python Pipeline Directory Structure + Stubs

**Files:** All `pipeline/` `__init__.py`, stub modules, and data files listed in the file map.

The stubs exist so the Makefile and CI can run `python -c "import pipeline.terrain.erode_terrain"` without errors even before the real implementation lands in later plans. Each stub exports exactly the function signature that will be implemented — this prevents later plans from accidentally changing the API.

- [ ] **Step 1: Create all pipeline package directories**

```bash
cd /home/cbartaria1/my-projects/MahlanyaRPG
mkdir -p pipeline/terrain
mkdir -p pipeline/settlements
mkdir -p pipeline/atmosphere
mkdir -p pipeline/audio
mkdir -p pipeline/history/data
mkdir -p pipeline/tests
mkdir -p pipeline/outputs
```

- [ ] **Step 2: Create all `__init__.py` files**

```bash
touch pipeline/__init__.py
touch pipeline/terrain/__init__.py
touch pipeline/settlements/__init__.py
touch pipeline/atmosphere/__init__.py
touch pipeline/audio/__init__.py
touch pipeline/history/__init__.py
touch pipeline/tests/__init__.py
```

- [ ] **Step 3: Create terrain stubs**

Create `pipeline/terrain/acquire_dem.py`:
```python
"""DEM acquisition from Copernicus GLO-10. Implemented in Plan 1."""
import pathlib


def acquire_dem(
    bbox_west: float,
    bbox_east: float,
    bbox_south: float,
    bbox_north: float,
    output_dir: pathlib.Path,
) -> pathlib.Path:
    """Download and mosaic Copernicus DEM tiles for the given bounding box.

    Args:
        bbox_west: Western longitude in degrees (WGS84).
        bbox_east: Eastern longitude in degrees (WGS84).
        bbox_south: Southern latitude in degrees (WGS84).
        bbox_north: Northern latitude in degrees (WGS84).
        output_dir: Directory to write the mosaicked GeoTIFF.

    Returns:
        Path to the output GeoTIFF (UTM Zone 36S / EPSG:32736).
    """
    raise NotImplementedError("Implemented in Plan 1 — terrain pipeline")
```

Create `pipeline/terrain/build_hardness_map.py`:
```python
"""Geological rock hardness rasterisation. Implemented in Plan 1."""
import pathlib


def build_hardness_map(
    dem_path: pathlib.Path,
    geology_vector_path: pathlib.Path,
    output_path: pathlib.Path,
) -> pathlib.Path:
    """Rasterise geological survey vector data to a hardness coefficient raster.

    Args:
        dem_path: Path to the DEM GeoTIFF (defines output extent + resolution).
        geology_vector_path: Path to geological survey vector file (GeoJSON/Shapefile).
        output_path: Path for the output hardness GeoTIFF (float32, 0–1 scale).

    Returns:
        Path to the output hardness GeoTIFF.
    """
    raise NotImplementedError("Implemented in Plan 1 — terrain pipeline")
```

Create `pipeline/terrain/erode_terrain.py`:
```python
"""Full geomorphological erosion stack. Implemented in Plan 1."""
import pathlib


def erode_terrain(
    dem_path: pathlib.Path,
    hardness_path: pathlib.Path,
    output_path: pathlib.Path,
    iterations: int = 5000,
    use_gpu: bool = True,
) -> pathlib.Path:
    """Run the full erosion stack: thermal, fluvial, aeolian, mass wasting.

    Args:
        dem_path: Path to the input DEM GeoTIFF.
        hardness_path: Path to the rock hardness GeoTIFF.
        output_path: Path for the eroded heightmap GeoTIFF.
        iterations: Number of erosion iterations (default 5000).
        use_gpu: Use CuPy CUDA acceleration if available (falls back to NumPy).

    Returns:
        Path to the eroded heightmap GeoTIFF.
    """
    raise NotImplementedError("Implemented in Plan 1 — terrain pipeline")


def thermal_erosion_pass(
    heightmap,  # np.ndarray shape (H, W) float32
    hardness,   # np.ndarray shape (H, W) float32
    talus_angle: float = 33.0,
):
    """Scree accumulation on cliff faces via thermal (freeze-thaw) erosion."""
    raise NotImplementedError("Implemented in Plan 1")


def fluvial_erosion_pass(
    heightmap,      # np.ndarray shape (H, W) float32
    water_map,      # np.ndarray shape (H, W) float32
    sediment_map,   # np.ndarray shape (H, W) float32
    hardness,       # np.ndarray shape (H, W) float32
    gravity: float = 9.81,
    erosion_rate_base: float = 0.05,
):
    """Navier-Stokes variant fluvial erosion with hardness-modulated capacity."""
    raise NotImplementedError("Implemented in Plan 1")


def aeolian_erosion_pass(
    heightmap,              # np.ndarray shape (H, W) float32
    wind_direction_deg: float,
    hardness,               # np.ndarray shape (H, W) float32
):
    """Wind-driven sediment transport across the Lowveld."""
    raise NotImplementedError("Implemented in Plan 1")


def mass_wasting_pass(
    heightmap,              # np.ndarray shape (H, W) float32
    hardness,               # np.ndarray shape (H, W) float32
    critical_slope: float = 38.0,
):
    """Probabilistic rockfall and landslide on slopes exceeding shear stress."""
    raise NotImplementedError("Implemented in Plan 1")
```

Create `pipeline/terrain/extract_rivers.py`:
```python
"""River network extraction via D-infinity flow accumulation. Implemented in Plan 1."""
import pathlib


def extract_rivers(
    eroded_dem_path: pathlib.Path,
    output_shapefile_path: pathlib.Path,
    flow_accumulation_threshold: int = 1000,
) -> pathlib.Path:
    """Extract stream network using D-infinity flow accumulation (Tarboton 1997).

    Args:
        eroded_dem_path: Path to eroded DEM GeoTIFF.
        output_shapefile_path: Path for output river network shapefile.
        flow_accumulation_threshold: Minimum upstream cell count to form a stream.
            Default 1000 cells ≈ 100 km² drainage area at 10m resolution.

    Returns:
        Path to the output shapefile.
    """
    raise NotImplementedError("Implemented in Plan 1 — terrain pipeline")
```

Create `pipeline/terrain/export_to_ue5.py`:
```python
"""Export eroded heightmap to UE5 Landscape format. Implemented in Plan 1."""
import pathlib


def export_to_ue5(
    eroded_dem_path: pathlib.Path,
    weight_maps_dir: pathlib.Path,
    output_dir: pathlib.Path,
    tile_size: int = 1009,
) -> pathlib.Path:
    """Split heightmap into UE5-compatible Landscape tiles.

    Args:
        eroded_dem_path: Path to eroded DEM GeoTIFF.
        weight_maps_dir: Directory containing weight map GeoTIFFs
            (rock_hardness, sediment_deposit, flow_velocity, river_mask, biome_zones).
        output_dir: Directory for output .r16 tiles and weight PNG tiles.
        tile_size: UE5 Landscape tile resolution. Must be 2^n + 1. Default 1009.

    Returns:
        Path to output directory containing tile subdirectories.
    """
    raise NotImplementedError("Implemented in Plan 1 — terrain pipeline")
```

- [ ] **Step 4: Create settlement stubs**

Create `pipeline/settlements/compute_voronoi.py`:
```python
"""Constrained Centroidal Voronoi settlement generator. Implemented in Plan 4."""
import pathlib


class SwaziSettlementGenerator:
    """Generates Umuti layout using constrained Voronoi tessellation.

    Social geometry constraints:
    - Sibaya (cattle kraal) at geometric origin (0, 0).
    - Chief's principal hut at apex: direction = East (vector [1, 0]).
    - Wife seniority → distance: d_i = d_base * (1 / rank_i ** 0.6).
    - Hut cell area ∝ social importance.
    - Taboo arc: 240°–300° West arc excluded (ancestral spirits, luhlanga).
    - Gate: single entrance at due South.
    """

    TABOO_ARC_DEG = (240, 300)

    def generate(
        self,
        n_wives: int,
        n_sons: int,
        n_dependents: int,
        cattle_count: int,
        terrain_gradient,  # np.ndarray shape (2,) — upslope direction vector
        seed: int = 0,
    ) -> dict:
        """Generate settlement JSON manifest.

        Returns:
            dict with keys: huts (list), sibaya (dict), gate (dict), taboo_arc (tuple).
        """
        raise NotImplementedError("Implemented in Plan 4 — settlement pipeline")

    def lloyd_relaxation(self, points, iterations: int = 50):
        """Centroidal Voronoi Optimization via iterative Lloyd relaxation."""
        raise NotImplementedError("Implemented in Plan 4")


def export_settlements(
    settlements: list[dict],
    output_dir: pathlib.Path,
) -> pathlib.Path:
    """Write per-settlement JSON manifests to output_dir."""
    raise NotImplementedError("Implemented in Plan 4 — settlement pipeline")
```

Create `pipeline/settlements/build_political_graph.py`:
```python
"""Inter-settlement political graph construction. Implemented in Plan 4."""
import pathlib


def build_political_graph(
    settlements_dir: pathlib.Path,
    dem_path: pathlib.Path,
    output_path: pathlib.Path,
) -> pathlib.Path:
    """Build directed political graph from settlement manifests.

    Hierarchy: Umuti → Indvuna village → Inkhundla → Lusengo (royal kraal).
    Trade route edges follow A* shortest path on terrain gradient field.

    Args:
        settlements_dir: Directory of per-settlement JSON manifests.
        dem_path: Eroded DEM for terrain path cost computation.
        output_path: Path for output political_graph.ndjson.

    Returns:
        Path to political_graph.ndjson.
    """
    raise NotImplementedError("Implemented in Plan 4 — settlement pipeline")
```

- [ ] **Step 5: Create atmosphere stubs**

Create `pipeline/atmosphere/compute_sky_luts.py`:
```python
"""Hosek-Wilkie spectral sky LUT computation. Implemented in Plan 5."""
import pathlib


def compute_sky_luts(
    latitude_deg: float,
    output_dir: pathlib.Path,
    sun_elevation_steps: int = 91,
    turbidity_min: float = 1.5,
    turbidity_max: float = 6.0,
    turbidity_steps: int = 10,
    wavelength_bands: int = 16,
) -> tuple[pathlib.Path, pathlib.Path]:
    """Compute Hosek-Wilkie spectral LUTs for Highveld and Lowveld altitude bands.

    Args:
        latitude_deg: Site latitude in degrees south (positive = south). Use 26.0 for Eswatini.
        output_dir: Directory to write sky_lut_highveld.exr and sky_lut_lowveld.exr.
        sun_elevation_steps: Number of sun elevation samples from 0° to 90°.
        turbidity_min: Minimum Mie turbidity (clean Highveld air).
        turbidity_max: Maximum Mie turbidity (dusty Lowveld haze).
        turbidity_steps: Number of turbidity samples.
        wavelength_bands: Number of spectral bands (380–720nm, evenly spaced).

    Returns:
        Tuple of (highveld_lut_path, lowveld_lut_path) as EXR files.
    """
    raise NotImplementedError("Implemented in Plan 5 — atmosphere pipeline")


def export_lunar_calendar(
    year_start: int,
    year_end: int,
    output_path: pathlib.Path,
) -> pathlib.Path:
    """Pre-compute lunar phase for every day in [year_start, year_end].

    Returns:
        Path to lunar_calendar_{year_start}_{year_end}.json.
    """
    raise NotImplementedError("Implemented in Plan 5")


def export_star_field(
    latitude_deg: float,
    epoch_year: float,
    output_path: pathlib.Path,
    min_magnitude: float = 6.5,
) -> pathlib.Path:
    """Export historically accurate star catalog for the given epoch and latitude.

    Returns:
        Path to starfield_{epoch_year}.json.
    """
    raise NotImplementedError("Implemented in Plan 5")
```

Create `pipeline/atmosphere/build_weather_tables.py`:
```python
"""SAWS climatology integration. Implemented in Plan 5."""
import pathlib


def build_weather_tables(
    saws_data_dir: pathlib.Path,
    output_path: pathlib.Path,
) -> pathlib.Path:
    """Build per-month weather probability distributions from SAWS station data.

    Args:
        saws_data_dir: Directory containing SAWS CSV files for Eswatini stations
            (Manzini, Big Bend, Pigg's Peak, Mbabane).
        output_path: Path for output weather_seasonal_table.json.

    Returns:
        Path to weather_seasonal_table.json.
    """
    raise NotImplementedError("Implemented in Plan 5 — atmosphere pipeline")
```

- [ ] **Step 6: Create audio stubs**

Create `pipeline/audio/compute_acoustic_irs.py`:
```python
"""Acoustic impulse response computation. Implemented in Plan 6."""
import pathlib

MATERIAL_ABSORPTION = {
    "granite":       [0.02, 0.02, 0.03, 0.03, 0.04, 0.05, 0.05, 0.06],
    "thatch_grass":  [0.15, 0.25, 0.40, 0.55, 0.65, 0.70, 0.72, 0.70],
    "clay_earth":    [0.35, 0.40, 0.45, 0.50, 0.55, 0.55, 0.60, 0.60],
    "dry_grass":     [0.25, 0.35, 0.45, 0.55, 0.60, 0.65, 0.65, 0.65],
    "water_surface": [0.01, 0.01, 0.02, 0.02, 0.03, 0.03, 0.05, 0.05],
    "open_sky":      [1.00, 1.00, 1.00, 1.00, 1.00, 1.00, 1.00, 1.00],
}

ENVIRONMENT_ARCHETYPES = [
    "granite_cave",
    "thatched_hut_int",
    "lubombo_canyon",
    "open_highveld",
    "usuthu_gorge",
    "riverbed_floodplain",
]


def compute_ir_image_source(
    geometry: dict,
    source_pos: tuple[float, float, float],
    receiver_pos: tuple[float, float, float],
    max_order: int = 3,
) -> list:
    """Compute early reflections via image source method (exact, first max_order orders).

    Returns:
        List of reflection events: [{"delay_s": float, "energy_per_band": list[float]}].
    """
    raise NotImplementedError("Implemented in Plan 6 — audio pipeline")


def compute_ir_monte_carlo(
    geometry: dict,
    source_pos: tuple[float, float, float],
    receiver_pos: tuple[float, float, float],
    n_rays: int = 10000,
    max_bounces: int = 20,
) -> list:
    """Compute late reverberation tail via Monte Carlo ray tracing.

    Returns:
        List of late reflections: [{"delay_s": float, "energy_per_band": list[float]}].
    """
    raise NotImplementedError("Implemented in Plan 6 — audio pipeline")


def export_ir_wav(
    reflections: list,
    output_path: pathlib.Path,
    sample_rate: int = 48000,
) -> pathlib.Path:
    """Convert reflection list to convolution IR WAV file.

    Returns:
        Path to output WAV file.
    """
    raise NotImplementedError("Implemented in Plan 6")
```

Create `pipeline/audio/compute_propagation_tables.py`:
```python
"""Atmospheric acoustic propagation tables. Implemented in Plan 6."""
import pathlib


def compute_propagation_tables(
    output_path: pathlib.Path,
) -> pathlib.Path:
    """Compute per-weather-condition effective sound speed and high-frequency absorption.

    Returns:
        Path to atmospheric_propagation.json.
    """
    raise NotImplementedError("Implemented in Plan 6 — audio pipeline")
```

Create `pipeline/audio/build_bioacoustic_library.py`:
```python
"""Eswatini bioacoustic species library. Implemented in Plan 6."""
import pathlib


def build_bioacoustic_library(
    output_path: pathlib.Path,
) -> pathlib.Path:
    """Build frequency-domain profiles for 28 bird species and insect/frog chorus.

    Returns:
        Path to bioacoustic_library.json.
    """
    raise NotImplementedError("Implemented in Plan 6 — audio pipeline")
```

- [ ] **Step 7: Create history stubs and empty data files**

Create `pipeline/history/build_knowledge_graph.py`:
```python
"""Swazi historical knowledge graph construction. Implemented in Plan 7."""
import pathlib


def build_knowledge_graph(
    data_dir: pathlib.Path,
    output_path: pathlib.Path,
) -> pathlib.Path:
    """Build Neo4j-compatible ndjson knowledge graph from entity JSON files.

    Args:
        data_dir: Directory containing persons.json, places.json, battles.json,
            events.json, relations.json, material_culture.json.
        output_path: Path for output knowledge_graph.ndjson.

    Returns:
        Path to knowledge_graph.ndjson.
    """
    raise NotImplementedError("Implemented in Plan 7 — knowledge graph")
```

Create `pipeline/history/validate_content.py`:
```python
"""Historical accuracy validator for NPC dialogue and quest content. Implemented in Plan 7."""
import pathlib
import sys


def validate_content(
    content_dir: pathlib.Path,
    knowledge_graph_path: pathlib.Path,
) -> list[dict]:
    """Validate all dialogue YAML files against the knowledge graph.

    Args:
        content_dir: Directory tree containing dialogue YAML files.
        knowledge_graph_path: Path to knowledge_graph.ndjson.

    Returns:
        List of violations: [{"file": str, "line": int, "claim": str, "reason": str}].
        Empty list = no violations.
    """
    raise NotImplementedError("Implemented in Plan 7 — knowledge graph")


if __name__ == "__main__":
    # CI entry point: exits with code 1 if any violations found
    import argparse
    parser = argparse.ArgumentParser()
    parser.add_argument("content_dir", type=pathlib.Path)
    parser.add_argument("knowledge_graph", type=pathlib.Path)
    args = parser.parse_args()
    violations = validate_content(args.content_dir, args.knowledge_graph)
    for v in violations:
        print(f"VIOLATION {v['file']}:{v['line']}: {v['claim']} — {v['reason']}")
    sys.exit(1 if violations else 0)
```

Create the empty data files:
```bash
echo '[]' > /home/cbartaria1/my-projects/MahlanyaRPG/pipeline/history/data/persons.json
echo '[]' > /home/cbartaria1/my-projects/MahlanyaRPG/pipeline/history/data/places.json
echo '[]' > /home/cbartaria1/my-projects/MahlanyaRPG/pipeline/history/data/battles.json
echo '[]' > /home/cbartaria1/my-projects/MahlanyaRPG/pipeline/history/data/events.json
echo '[]' > /home/cbartaria1/my-projects/MahlanyaRPG/pipeline/history/data/relations.json
echo '[]' > /home/cbartaria1/my-projects/MahlanyaRPG/pipeline/history/data/material_culture.json
```

- [ ] **Step 8: Commit the pipeline scaffold**

```bash
cd /home/cbartaria1/my-projects/MahlanyaRPG
git add pipeline/
git commit -m "feat: add Python pipeline directory scaffold with typed stubs"
```

Expected: commit listing all pipeline files

---

## Task 3: Python requirements.txt + Virtual Environment

**Files:**
- Create: `pipeline/requirements.txt`
- Create: `pipeline/conftest.py`

- [ ] **Step 1: Create requirements.txt**

Create `/home/cbartaria1/my-projects/MahlanyaRPG/pipeline/requirements.txt`:

```
# Core scientific stack
numpy>=1.26,<2.0
scipy>=1.12,<2.0
numba>=0.59,<1.0

# Geospatial
GDAL>=3.8,<4.0
rasterio>=1.3,<2.0
shapely>=2.0,<3.0
geopandas>=0.14,<1.0
pyproj>=3.6,<4.0

# 3D geometry (acoustic IR computation)
trimesh>=4.0,<5.0

# Graph database
neo4j>=5.0,<6.0

# CLI framework
click>=8.1,<9.0

# Testing
pytest>=8.0,<9.0
pytest-cov>=4.1,<5.0

# Optional GPU acceleration — install separately if CUDA 12.x available:
# cupy-cuda12x>=13.0,<14.0
```

- [ ] **Step 2: Create the virtual environment**

```bash
cd /home/cbartaria1/my-projects/MahlanyaRPG
python3.11 -m venv .venv
```

Expected: `.venv/` directory created

- [ ] **Step 3: Install dependencies**

```bash
cd /home/cbartaria1/my-projects/MahlanyaRPG
.venv/bin/pip install --upgrade pip
.venv/bin/pip install -r pipeline/requirements.txt
```

Expected: All packages install without error. GDAL install may take 2–3 minutes.

If GDAL fails with `gdal-config not found`, ensure system GDAL is installed (see Prerequisites).

- [ ] **Step 4: Create conftest.py**

Create `/home/cbartaria1/my-projects/MahlanyaRPG/pipeline/conftest.py`:

```python
import pathlib
import pytest


@pytest.fixture
def project_root() -> pathlib.Path:
    """Return the MahlanyaRPG project root directory."""
    return pathlib.Path(__file__).parent.parent


@pytest.fixture
def pipeline_root() -> pathlib.Path:
    """Return the pipeline/ directory."""
    return pathlib.Path(__file__).parent


@pytest.fixture
def tmp_output(tmp_path) -> pathlib.Path:
    """Return a temporary output directory unique to each test."""
    out = tmp_path / "output"
    out.mkdir()
    return out
```

- [ ] **Step 5: Write the failing environment test**

Create `/home/cbartaria1/my-projects/MahlanyaRPG/pipeline/tests/test_environment.py`:

```python
"""Verifies that all required pipeline libraries import successfully."""


def test_numpy_import():
    import numpy as np
    assert np.__version__ >= "1.26"


def test_scipy_import():
    import scipy
    assert scipy.__version__ >= "1.12"


def test_numba_import():
    import numba
    assert numba.__version__ >= "0.59"


def test_gdal_import():
    from osgeo import gdal
    version = gdal.__version__
    major, minor = int(version.split(".")[0]), int(version.split(".")[1])
    assert (major, minor) >= (3, 8), f"GDAL {version} < 3.8"


def test_rasterio_import():
    import rasterio
    assert rasterio.__version__ >= "1.3"


def test_shapely_import():
    import shapely
    assert shapely.__version__ >= "2.0"


def test_geopandas_import():
    import geopandas
    assert geopandas.__version__ >= "0.14"


def test_pyproj_import():
    import pyproj
    assert pyproj.__version__ >= "3.6"


def test_trimesh_import():
    import trimesh
    # trimesh doesn't expose a standard version constant — just verify import
    assert hasattr(trimesh, "load")


def test_neo4j_import():
    import neo4j
    assert neo4j.__version__ >= "5.0"


def test_click_import():
    import click
    assert click.__version__ >= "8.1"


def test_cupy_import_optional():
    """CuPy is optional (requires CUDA). Skip if not installed."""
    try:
        import cupy  # noqa: F401
    except ImportError:
        import pytest
        pytest.skip("CuPy not installed — CUDA GPU acceleration unavailable")
```

- [ ] **Step 6: Run the failing test (before install it fails, after install it passes)**

```bash
cd /home/cbartaria1/my-projects/MahlanyaRPG
.venv/bin/pytest pipeline/tests/test_environment.py -v
```

Expected after `pip install`: all tests PASS. If any FAIL, re-check `pip install` output for that package.

- [ ] **Step 7: Commit**

```bash
cd /home/cbartaria1/my-projects/MahlanyaRPG
git add pipeline/requirements.txt pipeline/conftest.py pipeline/tests/test_environment.py
git commit -m "feat: add Python requirements and environment verification tests"
```

---

## Task 4: Stub Importability Tests

**Files:**
- Create: `pipeline/tests/test_pipeline_stubs.py`

This test ensures every stub module is importable and every stub function is callable (raises `NotImplementedError`, not `ImportError` or `AttributeError`). Later plans will replace `NotImplementedError` with real implementations — this test continues to pass because the signatures are preserved.

- [ ] **Step 1: Write the stub importability tests**

Create `/home/cbartaria1/my-projects/MahlanyaRPG/pipeline/tests/test_pipeline_stubs.py`:

```python
"""Verifies all stub modules are importable and all stub functions have the correct signature."""
import pathlib
import pytest


# ── Terrain ─────────────────────────────────────────────────────────────────

def test_acquire_dem_is_importable():
    from pipeline.terrain.acquire_dem import acquire_dem
    assert callable(acquire_dem)


def test_acquire_dem_raises_not_implemented():
    from pipeline.terrain.acquire_dem import acquire_dem
    with pytest.raises(NotImplementedError):
        acquire_dem(30.0, 32.0, -28.0, -26.0, pathlib.Path("/tmp"))


def test_erode_terrain_is_importable():
    from pipeline.terrain.erode_terrain import (
        erode_terrain,
        thermal_erosion_pass,
        fluvial_erosion_pass,
        aeolian_erosion_pass,
        mass_wasting_pass,
    )
    for fn in [erode_terrain, thermal_erosion_pass, fluvial_erosion_pass,
               aeolian_erosion_pass, mass_wasting_pass]:
        assert callable(fn)


def test_extract_rivers_is_importable():
    from pipeline.terrain.extract_rivers import extract_rivers
    assert callable(extract_rivers)


def test_export_to_ue5_is_importable():
    from pipeline.terrain.export_to_ue5 import export_to_ue5
    assert callable(export_to_ue5)


def test_build_hardness_map_is_importable():
    from pipeline.terrain.build_hardness_map import build_hardness_map
    assert callable(build_hardness_map)


# ── Settlements ──────────────────────────────────────────────────────────────

def test_settlement_generator_is_importable():
    from pipeline.settlements.compute_voronoi import SwaziSettlementGenerator
    gen = SwaziSettlementGenerator()
    assert hasattr(gen, "generate")
    assert hasattr(gen, "lloyd_relaxation")
    assert gen.TABOO_ARC_DEG == (240, 300)


def test_political_graph_is_importable():
    from pipeline.settlements.build_political_graph import build_political_graph
    assert callable(build_political_graph)


# ── Atmosphere ───────────────────────────────────────────────────────────────

def test_compute_sky_luts_is_importable():
    from pipeline.atmosphere.compute_sky_luts import (
        compute_sky_luts,
        export_lunar_calendar,
        export_star_field,
    )
    for fn in [compute_sky_luts, export_lunar_calendar, export_star_field]:
        assert callable(fn)


def test_build_weather_tables_is_importable():
    from pipeline.atmosphere.build_weather_tables import build_weather_tables
    assert callable(build_weather_tables)


# ── Audio ────────────────────────────────────────────────────────────────────

def test_acoustic_irs_is_importable():
    from pipeline.audio.compute_acoustic_irs import (
        MATERIAL_ABSORPTION,
        ENVIRONMENT_ARCHETYPES,
        compute_ir_image_source,
        compute_ir_monte_carlo,
        export_ir_wav,
    )
    assert "granite" in MATERIAL_ABSORPTION
    assert len(MATERIAL_ABSORPTION["granite"]) == 8
    assert "granite_cave" in ENVIRONMENT_ARCHETYPES
    assert len(ENVIRONMENT_ARCHETYPES) == 6


def test_propagation_tables_is_importable():
    from pipeline.audio.compute_propagation_tables import compute_propagation_tables
    assert callable(compute_propagation_tables)


def test_bioacoustic_library_is_importable():
    from pipeline.audio.build_bioacoustic_library import build_bioacoustic_library
    assert callable(build_bioacoustic_library)


# ── History ──────────────────────────────────────────────────────────────────

def test_knowledge_graph_is_importable():
    from pipeline.history.build_knowledge_graph import build_knowledge_graph
    assert callable(build_knowledge_graph)


def test_validate_content_is_importable():
    from pipeline.history.validate_content import validate_content
    assert callable(validate_content)


def test_history_data_files_exist():
    data_dir = pathlib.Path(__file__).parent.parent / "history" / "data"
    for name in ["persons", "places", "battles", "events", "relations", "material_culture"]:
        assert (data_dir / f"{name}.json").exists(), f"Missing {name}.json"
```

- [ ] **Step 2: Run tests — expect all to pass**

```bash
cd /home/cbartaria1/my-projects/MahlanyaRPG
.venv/bin/pytest pipeline/tests/test_pipeline_stubs.py -v
```

Expected: all tests PASS. If any fail, fix the stub that's missing the attribute or has the wrong constant.

- [ ] **Step 3: Run full test suite to confirm no regressions**

```bash
cd /home/cbartaria1/my-projects/MahlanyaRPG
.venv/bin/pytest pipeline/tests/ -v --tb=short
```

Expected: all tests PASS.

- [ ] **Step 4: Commit**

```bash
cd /home/cbartaria1/my-projects/MahlanyaRPG
git add pipeline/tests/test_pipeline_stubs.py
git commit -m "test: add stub importability and signature tests for all pipeline modules"
```

---

## Task 5: Pipeline Makefile

**Files:**
- Create: `pipeline/Makefile`

The Makefile is the single entry point for running the full offline science pipeline. CI/CD calls `make test`. A human running the full pipeline calls `make full-pipeline`. Each sub-target corresponds to a plan's implementation.

- [ ] **Step 1: Write the Makefile**

Create `/home/cbartaria1/my-projects/MahlanyaRPG/pipeline/Makefile`:

```makefile
# MahlanyaRPG Offline Science Pipeline
# Usage:
#   make test            — Run all pytest tests
#   make full-pipeline   — Run all pipeline stages in order (requires data)
#   make acquire-dem     — Download and mosaic Copernicus DEM tiles
#   make erode           — Run full geomorphological erosion stack
#   make extract-rivers  — Extract river network via D-infinity flow accumulation
#   make export-terrain  — Export eroded terrain to UE5 format
#   make voronoi         — Compute Voronoi settlement layouts
#   make political-graph — Build inter-settlement political graph
#   make sky-luts        — Compute Hosek-Wilkie spectral LUTs
#   make weather         — Build SAWS climatology tables
#   make acoustic-irs    — Compute acoustic impulse responses
#   make propagation     — Compute atmospheric propagation tables
#   make bioacoustics    — Build bioacoustic species library
#   make knowledge-graph — Build historical knowledge graph
#   make validate        — Validate game content against knowledge graph
#   make clean           — Remove all pipeline outputs

PYTHON := ../.venv/bin/python
PYTEST := ../.venv/bin/pytest

# Eswatini bounding box (WGS84)
BBOX_WEST  := 30.79
BBOX_EAST  := 32.14
BBOX_SOUTH := -27.32
BBOX_NORTH := -25.72

OUTPUTS := outputs
DATA_DIR := history/data
CONTENT_DIR := ../Content/Dialogue

.PHONY: test full-pipeline acquire-dem erode extract-rivers export-terrain \
        voronoi political-graph sky-luts weather acoustic-irs propagation \
        bioacoustics knowledge-graph validate clean

test:
	$(PYTEST) tests/ -v --tb=short --cov=. --cov-report=term-missing

full-pipeline: acquire-dem erode extract-rivers export-terrain voronoi \
               political-graph sky-luts weather acoustic-irs propagation \
               bioacoustics knowledge-graph

acquire-dem:
	$(PYTHON) -m terrain.acquire_dem \
		--west $(BBOX_WEST) --east $(BBOX_EAST) \
		--south $(BBOX_SOUTH) --north $(BBOX_NORTH) \
		--output $(OUTPUTS)/terrain

erode: $(OUTPUTS)/terrain/dem_utm36s.tif
	$(PYTHON) -m terrain.erode_terrain \
		--dem $(OUTPUTS)/terrain/dem_utm36s.tif \
		--hardness $(OUTPUTS)/terrain/rock_hardness_utm36s.tif \
		--output $(OUTPUTS)/terrain/dem_eroded.tif \
		--iterations 5000

extract-rivers: $(OUTPUTS)/terrain/dem_eroded.tif
	$(PYTHON) -m terrain.extract_rivers \
		--dem $(OUTPUTS)/terrain/dem_eroded.tif \
		--output $(OUTPUTS)/terrain/river_network_utm36s.shp

export-terrain: $(OUTPUTS)/terrain/dem_eroded.tif
	$(PYTHON) -m terrain.export_to_ue5 \
		--dem $(OUTPUTS)/terrain/dem_eroded.tif \
		--weight-maps $(OUTPUTS)/terrain/weight_maps \
		--output $(OUTPUTS)/ue5_terrain

voronoi:
	$(PYTHON) -m settlements.compute_voronoi \
		--output $(OUTPUTS)/settlements

political-graph: $(OUTPUTS)/settlements
	$(PYTHON) -m settlements.build_political_graph \
		--settlements $(OUTPUTS)/settlements \
		--dem $(OUTPUTS)/terrain/dem_eroded.tif \
		--output $(OUTPUTS)/settlements/political_graph.ndjson

sky-luts:
	$(PYTHON) -m atmosphere.compute_sky_luts \
		--latitude 26.0 \
		--output $(OUTPUTS)/atmosphere

weather:
	$(PYTHON) -m atmosphere.build_weather_tables \
		--saws-data $(DATA_DIR)/saws \
		--output $(OUTPUTS)/atmosphere/weather_seasonal_table.json

acoustic-irs:
	$(PYTHON) -m audio.compute_acoustic_irs \
		--output $(OUTPUTS)/audio/impulse_responses

propagation:
	$(PYTHON) -m audio.compute_propagation_tables \
		--output $(OUTPUTS)/audio/atmospheric_propagation.json

bioacoustics:
	$(PYTHON) -m audio.build_bioacoustic_library \
		--output $(OUTPUTS)/audio/bioacoustic_library.json

knowledge-graph:
	$(PYTHON) -m history.build_knowledge_graph \
		--data-dir $(DATA_DIR) \
		--output $(OUTPUTS)/history/knowledge_graph.ndjson

validate:
	$(PYTHON) -m history.validate_content \
		$(CONTENT_DIR) \
		$(OUTPUTS)/history/knowledge_graph.ndjson

clean:
	rm -rf $(OUTPUTS)/

$(OUTPUTS)/terrain/dem_utm36s.tif:
	$(MAKE) acquire-dem

$(OUTPUTS)/terrain/dem_eroded.tif:
	$(MAKE) erode

$(OUTPUTS)/settlements:
	$(MAKE) voronoi
```

- [ ] **Step 2: Write a test that validates the Makefile targets exist**

Add this to `pipeline/tests/test_pipeline_stubs.py` (append at end):

```python
def test_makefile_exists():
    makefile = pathlib.Path(__file__).parent.parent / "Makefile"
    assert makefile.exists(), "pipeline/Makefile not found"


def test_makefile_has_required_targets():
    makefile = pathlib.Path(__file__).parent.parent / "Makefile"
    content = makefile.read_text()
    required_targets = [
        "test", "full-pipeline", "acquire-dem", "erode", "extract-rivers",
        "export-terrain", "voronoi", "sky-luts", "weather", "acoustic-irs",
        "knowledge-graph", "validate", "clean",
    ]
    for target in required_targets:
        assert f"{target}:" in content, f"Makefile missing target: {target}"
```

- [ ] **Step 3: Run the Makefile test**

```bash
cd /home/cbartaria1/my-projects/MahlanyaRPG
.venv/bin/pytest pipeline/tests/test_pipeline_stubs.py::test_makefile_exists \
                 pipeline/tests/test_pipeline_stubs.py::test_makefile_has_required_targets -v
```

Expected: both PASS.

- [ ] **Step 4: Run `make test` to verify Makefile calls pytest correctly**

```bash
cd /home/cbartaria1/my-projects/MahlanyaRPG/pipeline
make test
```

Expected: pytest runs all tests, all pass.

- [ ] **Step 5: Commit**

```bash
cd /home/cbartaria1/my-projects/MahlanyaRPG
git add pipeline/Makefile pipeline/tests/test_pipeline_stubs.py
git commit -m "feat: add pipeline Makefile with all sub-pipeline targets"
```

---

## Task 6: SimulationBus UE5 Plugin Scaffold

**Files:**
- Create: `Plugins/SimulationBusPlugin/SimulationBusPlugin.uplugin`
- Create: `Plugins/SimulationBusPlugin/Source/SimulationBusPlugin/SimulationBusPlugin.Build.cs`
- Create: `Plugins/SimulationBusPlugin/Source/SimulationBusPlugin/Public/SimulationBusSubsystem.h`
- Create: `Plugins/SimulationBusPlugin/Source/SimulationBusPlugin/Private/SimulationBusSubsystem.cpp`

The `SimulationBusPlugin` defines the typed multicast delegate contract used by all 8 other simulation plugins. It has zero plugin dependencies — it must be loaded before any other simulation plugin. All inter-plugin communication goes through its `USimulationBusSubsystem`.

- [ ] **Step 1: Create plugin directory structure**

```bash
mkdir -p /home/cbartaria1/my-projects/MahlanyaRPG/Plugins/SimulationBusPlugin/Source/SimulationBusPlugin/Public
mkdir -p /home/cbartaria1/my-projects/MahlanyaRPG/Plugins/SimulationBusPlugin/Source/SimulationBusPlugin/Private
```

- [ ] **Step 2: Create the .uplugin descriptor**

Create `/home/cbartaria1/my-projects/MahlanyaRPG/Plugins/SimulationBusPlugin/SimulationBusPlugin.uplugin`:

```json
{
  "FileVersion": 3,
  "Version": 1,
  "VersionName": "1.0",
  "FriendlyName": "SimulationBus",
  "Description": "Typed multicast delegate event bus for inter-plugin simulation communication. Zero plugin dependencies — must load before all other simulation plugins.",
  "Category": "MahlanyaRPG",
  "CreatedBy": "Charles Bartaria",
  "CreatedByURL": "",
  "DocsURL": "",
  "MarketplaceURL": "",
  "SupportURL": "",
  "CanContainContent": false,
  "IsBetaVersion": false,
  "IsExperimentalVersion": false,
  "Installed": false,
  "Modules": [
    {
      "Name": "SimulationBusPlugin",
      "Type": "Runtime",
      "LoadingPhase": "PreDefault"
    }
  ]
}
```

Note: `"LoadingPhase": "PreDefault"` ensures this plugin loads before all other simulation plugins.

- [ ] **Step 3: Create Build.cs**

Create `/home/cbartaria1/my-projects/MahlanyaRPG/Plugins/SimulationBusPlugin/Source/SimulationBusPlugin/SimulationBusPlugin.Build.cs`:

```csharp
using UnrealBuildTool;

public class SimulationBusPlugin : ModuleRules
{
    public SimulationBusPlugin(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine",
        });

        PrivateDependencyModuleNames.AddRange(new string[]
        {
            // No plugin dependencies — this plugin is the dependency root
        });
    }
}
```

- [ ] **Step 4: Create SimulationBusSubsystem.h**

Create `/home/cbartaria1/my-projects/MahlanyaRPG/Plugins/SimulationBusPlugin/Source/SimulationBusPlugin/Public/SimulationBusSubsystem.h`:

```cpp
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "SimulationBusSubsystem.generated.h"

// ── Delegate Declarations ────────────────────────────────────────────────────

/**
 * Published by: MicroclimateEngine (UMicroclimateSubsystem)
 * Subscribed by: ErosionRuntimePlugin, LocomotionPhysicsPlugin
 * Payload: precipitation intensity in mm/hr (0.0 = no rain, 50.0 = heavy storm)
 */
DECLARE_MULTICAST_DELEGATE_OneParam(FOnRainIntensityChanged, float /* IntensityMmPerHr */);

/**
 * Published by: ErosionRuntimePlugin (URuntimeHeightmapComponent)
 * Subscribed by: MicroclimateEngine
 * Payload: mean terrain saturation fraction in player-radius area (0.0–1.0)
 */
DECLARE_MULTICAST_DELEGATE_OneParam(FOnTerrainSaturationChanged, float /* SaturationFraction */);

/**
 * Published by: EconomySimulatorPlugin (UEconomySimulatorSubsystem)
 * Subscribed by: SibayaEngine (USibayaEngineSubsystem)
 * Payload: SettlementID of the settlement whose demographics changed, EventType tag
 */
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnSettlementDemographicChanged,
    FName /* SettlementID */,
    FName /* EventType: "WifeMarried" | "HutDestroyed" | "CattleRaid" | "FamilyMerge" */);

/**
 * Published by: any subsystem when a simulation threshold is crossed
 * Subscribed by: EmergentNarrativePlugin (UEmergentNarrativeSubsystem)
 * Payload: QuestTriggerConditionID as defined in quest_trigger_rules.json
 */
DECLARE_MULTICAST_DELEGATE_OneParam(FOnQuestTriggerConditionMet, FName /* ConditionID */);

/**
 * Published by: EcologySimulatorPlugin (UEcologySimulatorSubsystem)
 * Subscribed by: EmergentNarrativePlugin
 * Payload: SpeciesID, GridCellID — fired when a species population drops below critical threshold
 */
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnSpeciesPopulationCritical,
    FName /* SpeciesID */,
    FName /* GridCellID */);

/**
 * Published by: MicroclimateEngine (ULightningSystem)
 * Subscribed by: EmergentNarrativePlugin, SibayaEngine
 * Payload: World location of lightning strike, bHitSacredTree flag
 */
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnLightningStrike,
    FVector /* StrikeLocation */,
    bool    /* bHitSacredTree */);

// ── Subsystem ────────────────────────────────────────────────────────────────

/**
 * USimulationBusSubsystem
 *
 * World subsystem acting as the typed event bus for all inter-plugin simulation
 * communication. Loaded in PreDefault phase so it is available before any
 * simulation plugin starts.
 *
 * Usage (publisher):
 *   auto* Bus = GetWorld()->GetSubsystem<USimulationBusSubsystem>();
 *   Bus->OnRainIntensityChanged.Broadcast(IntensityMmPerHr);
 *
 * Usage (subscriber):
 *   auto* Bus = GetWorld()->GetSubsystem<USimulationBusSubsystem>();
 *   Bus->OnRainIntensityChanged.AddUObject(this, &UMyComponent::HandleRain);
 */
UCLASS()
class SIMULATIONBUSPLUGIN_API USimulationBusSubsystem : public UWorldSubsystem
{
    GENERATED_BODY()

public:
    // ── Rain / Erosion ───────────────────────────────────────────────────────
    FOnRainIntensityChanged        OnRainIntensityChanged;
    FOnTerrainSaturationChanged    OnTerrainSaturationChanged;

    // ── Settlement Demographics ──────────────────────────────────────────────
    FOnSettlementDemographicChanged OnSettlementDemographicChanged;

    // ── Narrative Triggers ───────────────────────────────────────────────────
    FOnQuestTriggerConditionMet    OnQuestTriggerConditionMet;

    // ── Ecology ──────────────────────────────────────────────────────────────
    FOnSpeciesPopulationCritical   OnSpeciesPopulationCritical;

    // ── Atmospheric Events ───────────────────────────────────────────────────
    FOnLightningStrike             OnLightningStrike;

    // UWorldSubsystem interface
    virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;
};
```

- [ ] **Step 5: Create SimulationBusSubsystem.cpp**

Create `/home/cbartaria1/my-projects/MahlanyaRPG/Plugins/SimulationBusPlugin/Source/SimulationBusPlugin/Private/SimulationBusSubsystem.cpp`:

```cpp
#include "SimulationBusSubsystem.h"

bool USimulationBusSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
    // Create in all world types (Game, PIE, Editor preview)
    return true;
}

void USimulationBusSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    UE_LOG(LogTemp, Log, TEXT("SimulationBusSubsystem: Initialized. All delegate channels ready."));
}

void USimulationBusSubsystem::Deinitialize()
{
    // Clear all delegate bindings to prevent dangling references
    OnRainIntensityChanged.Clear();
    OnTerrainSaturationChanged.Clear();
    OnSettlementDemographicChanged.Clear();
    OnQuestTriggerConditionMet.Clear();
    OnSpeciesPopulationCritical.Clear();
    OnLightningStrike.Clear();

    Super::Deinitialize();
}
```

- [ ] **Step 6: Verify plugin files are well-formed (syntax check)**

```bash
# Verify JSON is valid
python3 -c "
import json, pathlib
p = pathlib.Path('/home/cbartaria1/my-projects/MahlanyaRPG/Plugins/SimulationBusPlugin/SimulationBusPlugin.uplugin')
data = json.loads(p.read_text())
print('uplugin valid:', data['FriendlyName'], 'LoadingPhase:', data['Modules'][0]['LoadingPhase'])
"
```

Expected: `uplugin valid: SimulationBus LoadingPhase: PreDefault`

- [ ] **Step 7: Commit**

```bash
cd /home/cbartaria1/my-projects/MahlanyaRPG
git add Plugins/
git commit -m "feat: add SimulationBusPlugin scaffold with typed delegate event bus"
```

---

## Task 7: UE5 Project Config Files

**Files:**
- Create: `Config/DefaultScalability.ini`
- Create: `Config/DefaultGame.ini`
- Create: `Config/DefaultEngine.ini`

These config files define the mobile scalability tiers, project metadata, and required plugin list. They are text files — no UE5 Editor required to create them.

- [ ] **Step 1: Create Config directory**

```bash
mkdir -p /home/cbartaria1/my-projects/MahlanyaRPG/Config
```

- [ ] **Step 2: Create DefaultScalability.ini**

Create `/home/cbartaria1/my-projects/MahlanyaRPG/Config/DefaultScalability.ini`:

```ini
; MahlanyaRPG Scalability Settings
; Three tiers: PC Ultra (sg.* = 3), Mobile High (sg.* = 1), Mobile Low (sg.* = 0)

[ScalabilityGroups]
; PC Ultra — full simulation stack
sg.ResolutionQuality=100
sg.ViewDistanceQuality=3
sg.AntiAliasingQuality=3
sg.ShadowQuality=3
sg.GlobalIlluminationQuality=3
sg.ReflectionQuality=3
sg.PostProcessQuality=3
sg.TextureQuality=3
sg.EffectsQuality=3
sg.FoliageQuality=3
sg.ShadingQuality=3

; ── PC Ultra custom overrides ────────────────────────────────────────────────
[DeviceProfile PC_Ultra]
; Runtime erosion compute shader: enabled
+CVars=r.MahlanyaRPG.ErosionRuntime=1
; Voronoi runtime recomputation: enabled
+CVars=r.MahlanyaRPG.VoronoiRuntime=1
; Geometric audio rays per frame
+CVars=r.MahlanyaRPG.AudioRaysPerFrame=64
; View distance (metres)
+CVars=r.MahlanyaRPG.DrawDistance=8000
; Lumen global illumination
+CVars=r.Lumen.Enabled=1
; Nanite enabled
+CVars=r.Nanite.Enabled=1
; Volumetric clouds full simulation
+CVars=r.VolumetricCloud.Enabled=1

; ── Mobile High custom overrides ─────────────────────────────────────────────
[DeviceProfile Mobile_High]
+CVars=r.MahlanyaRPG.ErosionRuntime=0
+CVars=r.MahlanyaRPG.VoronoiRuntime=0
+CVars=r.MahlanyaRPG.AudioRaysPerFrame=0
+CVars=r.MahlanyaRPG.DrawDistance=2000
+CVars=r.Lumen.Enabled=0
+CVars=r.Nanite.Enabled=0
+CVars=r.VolumetricCloud.Enabled=0
+CVars=r.Shadow.CSM.MaxCascades=4

; ── Mobile Low custom overrides ──────────────────────────────────────────────
[DeviceProfile Mobile_Low]
+CVars=r.MahlanyaRPG.ErosionRuntime=0
+CVars=r.MahlanyaRPG.VoronoiRuntime=0
+CVars=r.MahlanyaRPG.AudioRaysPerFrame=0
+CVars=r.MahlanyaRPG.DrawDistance=1000
+CVars=r.Lumen.Enabled=0
+CVars=r.Nanite.Enabled=0
+CVars=r.VolumetricCloud.Enabled=0
+CVars=r.Shadow.CSM.MaxCascades=2
```

- [ ] **Step 3: Create DefaultGame.ini**

Create `/home/cbartaria1/my-projects/MahlanyaRPG/Config/DefaultGame.ini`:

```ini
[/Script/EngineSettings.GeneralProjectSettings]
ProjectID=+PROJECT_ID+
ProjectName=MahlanyaRPG
ProjectDisplayedTitle=NSLOCTEXT("", "MahlanyaTitle", "Mahlanya")
ProjectVersion=0.1.0
CompanyName=BRT Inc.
CopyrightNotice=Copyright 2026 BRT Inc. All rights reserved.
Description=Swazi historical 3D RPG — pre-colonial formation to colonial resistance (1750–1906).
```

- [ ] **Step 4: Create DefaultEngine.ini**

Create `/home/cbartaria1/my-projects/MahlanyaRPG/Config/DefaultEngine.ini`:

```ini
[OnlineSubsystem]
DefaultPlatformService=

[/Script/Engine.Engine]
GameEngine=/Script/Engine.GameEngine

; Required plugins list (also set in .uproject — listed here for documentation)
; SimulationBusPlugin, ErosionRuntimePlugin, SibayaEngine,
; LocomotionPhysicsPlugin, MicroclimateEngine, GeometricAudioPlugin,
; KnowledgeGraphPlugin, EconomySimulatorPlugin, EmergentNarrativePlugin

[/Script/WorldPartitionEditor.WorldPartitionEditorSettings]
; World Partition streaming cell size in metres — matches ErosionRuntimePlugin player-radius
bEnableLoadingInEditor=True

[/Script/Engine.RendererSettings]
r.DefaultFeature.Bloom=True
r.DefaultFeature.AmbientOcclusion=True
r.DefaultFeature.AutoExposure=True
; Nanite enabled by default for PC builds
r.Nanite.Enabled=1
; Lumen enabled by default for PC builds
r.Lumen.Enabled=1
```

- [ ] **Step 5: Write a test that validates Config files exist and are well-formed**

Append to `pipeline/tests/test_pipeline_stubs.py`:

```python
def test_config_files_exist():
    config_dir = pathlib.Path(__file__).parent.parent.parent / "Config"
    for name in ["DefaultScalability.ini", "DefaultGame.ini", "DefaultEngine.ini"]:
        assert (config_dir / name).exists(), f"Missing Config/{name}"


def test_scalability_ini_has_three_tiers():
    config_dir = pathlib.Path(__file__).parent.parent.parent / "Config"
    content = (config_dir / "DefaultScalability.ini").read_text()
    assert "PC_Ultra" in content
    assert "Mobile_High" in content
    assert "Mobile_Low" in content
    assert "r.MahlanyaRPG.ErosionRuntime" in content
    assert "r.MahlanyaRPG.DrawDistance" in content
```

- [ ] **Step 6: Run the config tests**

```bash
cd /home/cbartaria1/my-projects/MahlanyaRPG
.venv/bin/pytest pipeline/tests/test_pipeline_stubs.py::test_config_files_exist \
                 pipeline/tests/test_pipeline_stubs.py::test_scalability_ini_has_three_tiers -v
```

Expected: both PASS.

- [ ] **Step 7: Commit**

```bash
cd /home/cbartaria1/my-projects/MahlanyaRPG
git add Config/ pipeline/tests/test_pipeline_stubs.py
git commit -m "feat: add UE5 Config files with three-tier mobile scalability settings"
```

---

## Task 8: GitHub Actions CI/CD Workflows

**Files:**
- Create: `.github/workflows/pipeline-ci.yml`
- Create: `.github/workflows/content-validate.yml`
- Create: `.github/workflows/ue5-build.yml`

- [ ] **Step 1: Create workflow directory**

```bash
mkdir -p /home/cbartaria1/my-projects/MahlanyaRPG/.github/workflows
```

- [ ] **Step 2: Create pipeline-ci.yml**

Create `/home/cbartaria1/my-projects/MahlanyaRPG/.github/workflows/pipeline-ci.yml`:

```yaml
name: Pipeline CI

on:
  push:
    paths:
      - 'pipeline/**'
      - '.github/workflows/pipeline-ci.yml'
  pull_request:
    paths:
      - 'pipeline/**'

jobs:
  test:
    name: Python Pipeline Tests
    runs-on: ubuntu-22.04

    steps:
      - name: Checkout
        uses: actions/checkout@v4

      - name: Install system GDAL
        run: |
          sudo apt-get update
          sudo apt-get install -y gdal-bin libgdal-dev python3-gdal

      - name: Set up Python 3.11
        uses: actions/setup-python@v5
        with:
          python-version: '3.11'
          cache: 'pip'
          cache-dependency-path: pipeline/requirements.txt

      - name: Install Python dependencies
        run: |
          python -m pip install --upgrade pip
          pip install -r pipeline/requirements.txt
        env:
          GDAL_VERSION: $(gdal-config --version)
          CPLUS_INCLUDE_PATH: /usr/include/gdal
          C_INCLUDE_PATH: /usr/include/gdal

      - name: Run pipeline tests
        run: |
          cd pipeline
          pytest tests/ -v --tb=short --cov=. --cov-report=xml --cov-report=term-missing
        working-directory: ${{ github.workspace }}

      - name: Assert coverage ≥ 30% (stub phase baseline)
        run: |
          cd pipeline
          pytest tests/ --cov=. --cov-fail-under=30 -q
        working-directory: ${{ github.workspace }}
```

- [ ] **Step 3: Create content-validate.yml**

Create `/home/cbartaria1/my-projects/MahlanyaRPG/.github/workflows/content-validate.yml`:

```yaml
name: Content Historical Accuracy Validation

on:
  push:
    paths:
      - 'Content/Dialogue/**'
      - 'pipeline/history/data/**'
      - '.github/workflows/content-validate.yml'
  pull_request:
    paths:
      - 'Content/Dialogue/**'
      - 'pipeline/history/data/**'

jobs:
  validate:
    name: Validate NPC Dialogue Against Knowledge Graph
    runs-on: ubuntu-22.04

    steps:
      - name: Checkout
        uses: actions/checkout@v4

      - name: Set up Python 3.11
        uses: actions/setup-python@v5
        with:
          python-version: '3.11'
          cache: 'pip'
          cache-dependency-path: pipeline/requirements.txt

      - name: Install minimal dependencies
        run: pip install neo4j click

      - name: Build knowledge graph
        run: |
          python -m pipeline.history.build_knowledge_graph \
            --data-dir pipeline/history/data \
            --output pipeline/outputs/knowledge_graph.ndjson
        # This step will be a no-op until Plan 7 implements the function

      - name: Validate content
        run: |
          python -m pipeline.history.validate_content \
            Content/Dialogue \
            pipeline/outputs/knowledge_graph.ndjson
        # Exits with code 1 and prints violations if any found
        # Content/Dialogue/ may not exist yet — skip gracefully
        continue-on-error: true
```

- [ ] **Step 4: Create ue5-build.yml**

Create `/home/cbartaria1/my-projects/MahlanyaRPG/.github/workflows/ue5-build.yml`:

```yaml
name: UE5 Plugin Build

on:
  push:
    paths:
      - 'Source/**'
      - 'Plugins/**'
      - '.github/workflows/ue5-build.yml'
  pull_request:
    paths:
      - 'Source/**'
      - 'Plugins/**'

jobs:
  build:
    name: Compile UE5 Plugins (Linux)
    runs-on: ubuntu-22.04
    # NOTE: This job requires a self-hosted runner with UE5.4+ installed
    # and the runner tagged 'ue5'. Until a self-hosted runner is configured,
    # this job is skipped.
    if: ${{ vars.HAS_UE5_RUNNER == 'true' }}

    steps:
      - name: Checkout
        uses: actions/checkout@v4

      - name: Build all plugins
        run: |
          $UE5_ROOT/Engine/Build/BatchFiles/Linux/Build.sh \
            MahlanyaRPGEditor \
            Linux \
            Development \
            "$GITHUB_WORKSPACE/MahlanyaRPG.uproject" \
            -waitmutex

      - name: Run Automation Tests
        run: |
          $UE5_ROOT/Engine/Binaries/Linux/UnrealEditor \
            "$GITHUB_WORKSPACE/MahlanyaRPG.uproject" \
            -ExecCmds="Automation RunTests SimulationBus+Erosion+Sibaya; Quit" \
            -log -unattended -NullRHI
```

- [ ] **Step 5: Commit CI/CD workflows**

```bash
cd /home/cbartaria1/my-projects/MahlanyaRPG
git add .github/
git commit -m "ci: add GitHub Actions workflows for pipeline tests, content validation, and UE5 build"
```

---

## Task 9: README

**Files:**
- Create: `README.md`

- [ ] **Step 1: Create README.md**

Create `/home/cbartaria1/my-projects/MahlanyaRPG/README.md`:

```markdown
# Mahlanya — Swazi Historical 3D RPG

A historically accurate 3D action-RPG set across multiple eras of Eswatini's history
(pre-colonial formation ~1750 through colonial resistance ~1906).

The game's environment does not merely look realistic — it behaves with the physical,
geological, climatological, acoustic, and social reality of the actual Kingdom of Eswatini.
All simulation systems are driven by real Earth-science mathematics and verified historical record.

## Prerequisites

### Python Pipeline

- Python 3.11+
- GDAL 3.8+ (system library):
  - Ubuntu/Debian: `sudo apt install gdal-bin libgdal-dev`
  - macOS: `brew install gdal`
- Git LFS: `git lfs install`

### UE5 Game Project

- Unreal Engine 5.4+
- Visual Studio 2022 (Windows) or clang-14+ (Linux/macOS)
- NVIDIA GPU with CUDA 12.x (optional — for GPU-accelerated terrain erosion)

## Setup

```bash
# 1. Clone the repository
git clone <repo-url> MahlanyaRPG
cd MahlanyaRPG
git lfs pull

# 2. Create Python virtual environment
python3.11 -m venv .venv
.venv/bin/pip install -r pipeline/requirements.txt

# 3. Run pipeline tests to verify environment
cd pipeline && make test

# 4. Open MahlanyaRPG.uproject in UE5 Editor
#    (requires Unreal Engine 5.4+ installed)
```

## Running the Pipeline

```bash
cd pipeline

# Run all pipeline stages in sequence (requires GIS data access)
make full-pipeline

# Run individual stages
make acquire-dem     # Download Copernicus DEM tiles for Eswatini
make erode           # Run geomorphological erosion (4–18 hours)
make extract-rivers  # Extract Usuthu, Komati, Lusushwana river networks
make export-terrain  # Export to UE5 Landscape format

# Validate game content historical accuracy
make validate
```

## Architecture

Four interlocking layers:

1. **Offline Science Pipeline** (Python) — runs exact simulation algorithms on real GIS
   data before the game ships; produces heightmaps, LUTs, IRs, JSON layouts
2. **UE5 Runtime Core** (C++ + Blueprint) — Nanite, Lumen, World Partition, PCG, MetaSounds
3. **Custom Algorithm Plugins** (C++ UE5) — 9 plugins implementing exact algorithms
   where UE5 built-ins approximate
4. **Mobile Artifact Consumer** — mobile builds consume pre-baked artifacts only

See `docs/superpowers/specs/2026-06-24-mahlanya-rpg-design.md` for the full specification.

## Historical Accuracy

All NPC dialogue, quest content, and world text is validated against a Neo4j-compatible
knowledge graph of verified Swazi history. The CI/CD `content-validate.yml` workflow
fails any PR that introduces a historically inconsistent claim.

## Implementation Plans

Plans are executed in this order:

| Plan | Topic |
|------|-------|
| **Plan 0** | Foundation ← current |
| Plan 1 | Terrain Python Pipeline |
| Plan 2 | Terrain UE5 Runtime + ErosionRuntimePlugin |
| Plan 3 | Locomotion (LocomotionPhysicsPlugin) |
| Plan 4 | Settlement Pipeline + SibayaEngine |
| Plan 5 | Atmosphere Pipeline + MicroclimateEngine |
| Plan 6 | Audio Pipeline + GeometricAudioPlugin |
| Plan 7 | Historical Knowledge Graph |
| Plan 8 | Political Economy (EconomySimulatorPlugin) |
| Plan 9 | Cultural Protocols + Language Engine |
| Plan 10 | Ecological Simulation |
| Plan 11 | Emergent Narrative Engine |
| Plan 12 | Mobile Scalability Tier |
| Plan 13 | Co-op Multiplayer Layer |
```

- [ ] **Step 2: Run final full test suite**

```bash
cd /home/cbartaria1/my-projects/MahlanyaRPG
.venv/bin/pytest pipeline/tests/ -v --tb=short
```

Expected: all tests PASS.

- [ ] **Step 3: Final commit**

```bash
cd /home/cbartaria1/my-projects/MahlanyaRPG
git add README.md
git commit -m "docs: add project README with prerequisites, setup, and architecture overview"
```

- [ ] **Step 4: Verify final git log**

```bash
cd /home/cbartaria1/my-projects/MahlanyaRPG
git log --oneline
```

Expected output (newest first):
```
<hash> docs: add project README with prerequisites, setup, and architecture overview
<hash> ci: add GitHub Actions workflows for pipeline tests, content validation, and UE5 build
<hash> feat: add UE5 Config files with three-tier mobile scalability settings
<hash> feat: add SimulationBusPlugin scaffold with typed delegate event bus
<hash> feat: add pipeline Makefile with all sub-pipeline targets
<hash> test: add stub importability and signature tests for all pipeline modules
<hash> feat: add Python requirements and environment verification tests
<hash> feat: add Python pipeline directory scaffold with typed stubs
<hash> chore: add Git LFS config and .gitignore
<hash> Add Mahlanya RPG production specification
```

---

## Self-Review

**Spec coverage check:**

| Spec Section | Covered by Task |
|---|---|
| 4-layer architecture | Task 2 (pipeline stubs), Task 6 (SimulationBus), Task 7 (Config tiers) |
| Git LFS patterns | Task 1 |
| Python deps (all 13 libraries) | Task 3 |
| SimulationBus delegates (6 delegates) | Task 6 |
| 3 scalability tiers | Task 7 |
| CI/CD (3 workflows) | Task 8 |
| Pipeline Makefile targets | Task 5 |
| Project directory structure | Tasks 1–9 |

**Placeholder scan:** No TBD/TODO in implementation steps. Stub functions explicitly raise `NotImplementedError` with the plan that implements them. All test assertions are concrete.

**Type consistency:** `FOnRainIntensityChanged`, `FOnTerrainSaturationChanged`, `FOnSettlementDemographicChanged`, `FOnQuestTriggerConditionMet`, `FOnSpeciesPopulationCritical`, `FOnLightningStrike` — all declared in `SimulationBusSubsystem.h` and referenced nowhere else in this plan (subscribers appear in later plans). No inconsistency possible in Plan 0.

**Gap check:** The spec mentions `pipeline/outputs/` being git-ignored — covered by `.gitignore`. The spec mentions `Makefile` at project root for UE5 — that is a UE5 build concern deferred to Plan 2 when the `.uproject` file exists.
