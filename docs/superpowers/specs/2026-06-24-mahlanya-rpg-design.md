# Swazi Historical 3D RPG — "Mahlanya" — Production Specification
**Date:** 2026-06-24  
**Author:** Charles Bartaria (cbartaria1)  
**Status:** Approved for Implementation  

---

## Context

This document specifies the complete architecture for a Swazi historical 3D RPG set across multiple eras of Eswatini's history (pre-colonial formation ~1750 through colonial resistance ~1906). The game's central differentiator is that its environment does not *look* realistic — it *behaves* with the physical, geological, climatological, acoustic, and social reality of the actual Kingdom of Eswatini. Every simulation system is driven by real Earth-science mathematics, real GIS data, and verified Swazi historical record.

**The protagonist is Mahlanya.** The game spans the collision between Swazi sovereignty and colonial expansion. Accuracy is non-negotiable — the historical knowledge graph structurally prevents historically false statements.

**Engine:** Unreal Engine 5 (C++ + Blueprint)  
**Target Platforms:** PC (primary), Mobile (iOS + Android, pre-baked tier), Console (future phase)  
**Multiplayer:** Single-player with co-op multiplayer layer (GAS-ready architecture from day one)  
**GIS Data:** Copernicus DEM GLO-10 (10m resolution) + public SAWS climatology data  

---

## Architecture: Four Interlocking Layers

```
┌─────────────────────────────────────────────────────────────────┐
│  LAYER 1: OFFLINE SCIENCE PIPELINE  (Python)                   │
│  Runs before game ships. Produces world artifacts.              │
│  Tools: GDAL, rasterio, numpy, scipy, numba, cupy, shapely,    │
│         geopandas, trimesh, neo4j-driver, pyproj               │
└───────────────────────────┬─────────────────────────────────────┘
                            │ exports: .r16 heightmaps, .exr LUTs,
                            │         .wav IRs, .json layouts,
                            │         .shp shapefiles, .ndjson graph
                            ▼
┌─────────────────────────────────────────────────────────────────┐
│  LAYER 2: UE5 RUNTIME CORE  (C++ + Blueprint)                  │
│  Nanite, Lumen, World Partition, PCG Framework, Control Rig,   │
│  Motion Warping, Chaos Physics, MetaSounds, GAS, Replication   │
└───────────────────────────┬─────────────────────────────────────┘
                            │ extended by
                            ▼
┌─────────────────────────────────────────────────────────────────┐
│  LAYER 3: CUSTOM ALGORITHM PLUGINS  (C++ UE5 Plugins)          │
│  Where UE5 built-ins approximate, exact algorithms run here.   │
│  8 plugins: Erosion, Sibaya, Locomotion, Microclimate,         │
│             GeometricAudio, KnowledgeGraph, Economy, Narrative  │
└───────────────────────────┬─────────────────────────────────────┘
                            │ mobile path
                            ▼
┌─────────────────────────────────────────────────────────────────┐
│  LAYER 4: MOBILE ARTIFACT CONSUMER  (UE5 Mobile Renderer)      │
│  Consumes pre-baked artifacts from Layer 1 only.               │
│  No runtime heavy compute. Full visual fidelity via baked data.│
└─────────────────────────────────────────────────────────────────┘
```

---

## System 1 — Geophysical Terrain Synthesis

### Problem
Eswatini's landscape is defined by the violent elevation drop from the ancient granitic Highveld (~1600–1800m ASL) down through the Middleveld to the sedimentary Lowveld (~100–400m ASL) and the Lubombo rhyolite plateau on the eastern border. No game engine terrain tool reproduces this with geophysical accuracy. We implement the full geomorphological erosion stack on real DEM data.

### Offline Pipeline (`/pipeline/terrain/`)

**Step 1 — DEM Acquisition** (`acquire_dem.py`)
- Downloads Copernicus DEM GLO-10 tiles covering Eswatini bounding box (30.79°E–32.14°E, 27.32°S–25.72°S)
- Reprojects from WGS84 to UTM Zone 36S (EPSG:32736) for metric-accurate distance calculations
- Mosaics tiles into a single 16-bit GeoTIFF using `gdalwarp`
- Resolution: 10m/pixel → approximately 15,400 × 17,200 pixel grid for all of Eswatini

**Step 2 — Geological Rock Hardness Map** (`build_hardness_map.py`)
- Loads Eswatini geological survey vector data (publicly available from the Council for Geoscience South Africa)
- Rasterizes to match DEM extent: assigns per-cell hardness coefficient k from geotechnical literature:
  - Granite gneiss (Highveld): k = 0.002 (erodes very slowly)
  - Karoo sedimentary (Lowveld): k = 0.08 (erodes rapidly)
  - Lubombo rhyolite: k = 0.015
  - Alluvial deposits (river corridors): k = 0.12
- Exports: `rock_hardness_utm36s.tif`

**Step 3 — Full Geomorphological Erosion Stack** (`erode_terrain.py`)
Implemented as GPU-accelerated NumPy/CuPy operations on 4096×4096 tiles (tiled processing for full coverage):

```python
# Executed in sequence per tile:

def thermal_erosion_pass(heightmap, hardness, talus_angle=33.0):
    """Scree accumulation on Highveld cliff faces (freeze-thaw)."""
    ...

def fluvial_erosion_pass(heightmap, water_map, sediment_map,
                          hardness, gravity=9.81, erosion_rate_base=0.05):
    """
    Navier-Stokes variant fluvial erosion.
    Spatial gradient computed via finite differences.
    Velocity = water_depth * gradient * gravity.
    Erosion rate modulated by rock hardness coefficient.
    Sediment carrying capacity = velocity * erosion_rate * (1 - hardness).
    """
    slope_x = (heightmap[2:, 1:-1] - heightmap[:-2, 1:-1]) / 2.0
    slope_y = (heightmap[1:-1, 2:] - heightmap[1:-1, :-2]) / 2.0
    gradient = np.sqrt(slope_x**2 + slope_y**2)
    velocity = water_map[1:-1, 1:-1] * gradient * gravity
    capacity = velocity * erosion_rate_base * (1.0 - hardness[1:-1, 1:-1])
    deficit = np.maximum(capacity - sediment_map[1:-1, 1:-1], 0)
    heightmap[1:-1, 1:-1] -= deficit
    sediment_map[1:-1, 1:-1] += deficit
    ...

def aeolian_erosion_pass(heightmap, wind_direction_deg, hardness):
    """Wind-driven sediment transport across flat Lowveld."""
    ...

def mass_wasting_pass(heightmap, hardness, critical_slope=38.0):
    """Probabilistic rockfall + landslide on slopes exceeding shear stress."""
    ...
```

Runs 5,000 iterations of the full stack. GPU execution via CuPy with CUDA fallback to NumPy for CPU-only machines. Estimated runtime: ~4 hours on RTX 4090, ~18 hours on CPU.

**Step 4 — River Network Extraction** (`extract_rivers.py`)
- D-infinity flow accumulation (Tarboton 1997 algorithm) on eroded heightmap
- Extracts stream network at flow accumulation threshold = 1,000 cells (~100km² drainage area)
- Produces polyline shapefile of: Usuthu River, Komati River, Lusushwana, Mbuluzi, Ngwavuma, Mhlumati
- River nodes annotated with: stream order (Strahler), flow velocity estimate, crossability flag (fordable / swimmable / impassable)
- Exports: `river_network_utm36s.shp`, `ford_locations.geojson`

**Step 5 — UE5 Export** (`export_to_ue5.py`)
- Splits eroded heightmap into 1009×1009 UE5 Landscape tiles (required resolution for World Partition)
- Converts float32 GeoTIFF → 16-bit PNG heightmap (`.r16` format for UE5 direct import)
- Exports layer weight maps: rock_hardness, sediment_deposit, flow_velocity, river_mask, biome_zones
- Exports: `heightmap_tiles/`, `weight_maps/`, `river_network.spline_data`

### UE5 Runtime
- **Nanite Landscape** tessellated from exported heightmap tiles
- **World Partition** with streaming grid at 256m cell size (auto-streams as player moves)
- **Landscape Material** (master material with 6 blend layers):
  - Reads `rock_hardness` weight map → paints bare granite ↔ soil transitions
  - Reads `sediment_deposit` → paints alluvial clay and river floodplain mud
  - Reads `flow_velocity` → paints wet rock, algae bloom, eroded channels
  - Reads `biome_zones` → drives vegetation density mask fed to PCG Framework
  - Reads `river_mask` → paints riparian gallery forest ground material

### Custom C++ Plugin — `ErosionRuntimePlugin`
- **Trigger**: `MicroclimateEngine` broadcasts `OnRainIntensityChanged` event
- On rain start: registers a Compute Shader pass (`ErosionCS.usf`) targeting a 200m radius heightmap patch around player
- Compute shader runs a simplified 2D erosion step (GPU, 50 iterations per rain second)
- Results write to a runtime `URuntimeHeightmapComponent` that overlays the base Nanite mesh via displacement
- Reads terrain material tags to modulate erosion rate (clay erodes fast, granite barely)
- Footprints and wagon wheel ruts write displacement via the same component
- Session-local only: component resets to baked heightmap on session end (ensures determinism for multiplayer)

### Mobile Tier
- Imports `heightmap_tiles/*.r16` directly
- Simplified 4-layer Landscape Material (no runtime displacement)
- River splines rendered as static mesh ribbons

### Critical Files
```
/pipeline/terrain/
├── acquire_dem.py
├── build_hardness_map.py
├── erode_terrain.py
├── extract_rivers.py
└── export_to_ue5.py

/Game/Terrain/
├── LM_SwaziBiome_Master.uasset       ← Landscape Material
├── LF_RockHardness.uasset            ← Landscape Layer Function
├── LF_SedimentDeposit.uasset
└── LF_BiomeZone.uasset

/Plugins/ErosionRuntimePlugin/
├── Source/ErosionRuntimePlugin/
│   ├── ErosionRuntimePlugin.cpp
│   ├── ErosionRuntimePlugin.h
│   ├── URuntimeHeightmapComponent.cpp
│   └── URuntimeHeightmapComponent.h
└── Shaders/
    └── ErosionCS.usf                 ← HLSL compute shader
```

---

## System 2 — Umuti Settlement Engine (Voronoi Tessellation)

### Problem
Traditional Swazi homesteads (Umuti) follow strict social geometry radiating from the Sibaya (cattle kraal). Generic game engines place huts randomly. We implement constrained Centroidal Voronoi Tessellation with Swazi social constraint tensors, producing settlements whose spatial layout *communicates* social hierarchy without any UI text.

### Offline Pipeline (`/pipeline/settlements/`)

**Step 1 — Voronoi Computation** (`compute_voronoi.py`)
```python
from scipy.spatial import Voronoi
import numpy as np

class SwaziSettlementGenerator:
    """
    Generates Umuti layout using constrained Voronoi tessellation.
    
    Social geometry rules:
    - Sibaya (cattle kraal) at geometric origin (0,0)
    - Chief's principal hut at apex: direction = East (vector [1,0])
    - Wife seniority → distance from Sibaya: d_i = d_base * (1 / rank_i^0.6)
    - Hut cell area ∝ social importance (elders get larger cells)
    - Taboo zone: 30° arc due West is reserved for ancestral spirits (luhlanga)
    - Gate: single entrance at due South (lowest status, facing the chief)
    """
    
    TABOO_ARC_DEG = (240, 300)    # West arc = spiritually forbidden
    GATE_DIRECTION = np.array([0, -1])  # Due South
    APEX_DIRECTION = np.array([1, 0])   # Due East (Chief's hut)
    
    def generate(self, n_wives: int, n_sons: int, n_dependents: int,
                  cattle_count: int, terrain_gradient: np.ndarray) -> dict:
        ...
    
    def lloyd_relaxation(self, points, iterations=50):
        """Centroidal Voronoi Optimization — iterative relaxation."""
        ...
```

- Runs Lloyd's algorithm for 50 iterations per settlement
- Terrain gradient from the erosion pipeline modulates cell shapes (uphill huts cluster more tightly)
- Outputs JSON manifest per settlement with: hut positions (UTM), orientations (bearing degrees), social roles, demographic data, construction material type (grass/wood/mixed based on biome zone)

**Step 2 — Political Graph Construction** (`build_political_graph.py`)
- Loads all settlement JSON manifests
- Builds directed graph: `{source_sid: {target_sid: {relation: str, cattle_tribute: int, distance_km: float}}}`
- Settlement hierarchy: Umuti → Indvuna's village → Inkhundla (chiefdom centre) → Lusengo (royal kraal)
- Trade route edges follow shortest-path on the actual terrain graph (A* on the heightmap gradient field)
- Exports: `political_graph.ndjson`, `trade_routes.geojson`

### UE5 PCG Framework (`/Game/Settlements/PCG/`)
- **PCG_UmutiLayout**: Graph that reads JSON manifest → places hut static mesh instances at exact Voronoi positions with correct Y-rotation (orientation bearing)
- Per-hut variation: PCG selects mesh variant (large/medium/small) based on social role field in JSON
- Fence posts, granaries, cattle pen geometry also placed via PCG child graphs
- LOD and Hierarchical Instance Static Mesh (HISM) culling handled by PCG automatically

### Custom C++ Plugin — `SibayaEngine`

**Runtime Voronoi Recomputation** (triggered by game events):
```cpp
// USibayaEngineSubsystem.h
UCLASS()
class USibayaEngineSubsystem : public UWorldSubsystem
{
    GENERATED_BODY()
public:
    // Called when demographic state changes
    void OnWifeMarried(FSettlementID SettlementID, FWifeData NewWife);
    void OnHutDestroyed(FSettlementID SettlementID, FHutID HutID);
    void OnCattleRaid(FSettlementID SettlementID, int32 CattleLost);
    void OnFamilyMerge(FSettlementID A, FSettlementID B);
    
private:
    // C++ port of scipy Voronoi + Lloyd relaxation
    FVoronoiLayout ComputeVoronoi(const FSettlementData& Data);
    void UpdatePCGInstances(FSettlementID ID, const FVoronoiLayout& Layout);
    void SpawnRuinOverlay(FHutID HutID, const FVoronoiCell& Cell);
};
```

**Archaeological Layer**: When a hut is destroyed, `SpawnRuinOverlay` places:
- Ground decal: ash pit ring (2m diameter)
- Post-hole meshes at cell boundary intersections
- Ghost wireframe spline of the former Voronoi cell boundary (visible only in tracking/perception mode)

**Inter-Settlement Graph Queries** (exposed to Blueprint):
```cpp
TArray<FSettlementRelation> GetAlliesOf(FSettlementID ID);
float GetCattleTribute(FSettlementID From, FSettlementID To);
TArray<FVector> GetTradeRoutePath(FSettlementID From, FSettlementID To);
bool IsInFeud(FSettlementID A, FSettlementID B);
```

### Mobile Tier
- Static pre-baked placement from JSON; no runtime Voronoi
- PCG disabled; uses standard HISM instancing from baked transform arrays

### Critical Files
```
/pipeline/settlements/
├── compute_voronoi.py
├── build_political_graph.py
└── export_to_ue5.py

/Game/Settlements/
├── PCG/
│   ├── PCG_UmutiLayout.uasset
│   ├── PCG_FenceRing.uasset
│   └── PCG_CattlePen.uasset
├── Meshes/SM_Hut_Large.uasset
├── Meshes/SM_Hut_Medium.uasset
├── Meshes/SM_Sibaya_Fence.uasset
└── Data/Settlements/*.json

/Plugins/SibayaEngine/
├── Source/SibayaEngine/
│   ├── USibayaEngineSubsystem.cpp
│   ├── VoronoiSolver.cpp          ← C++ Lloyd relaxation
│   ├── PoliticalGraphComponent.cpp
│   └── ArchaeologyOverlayActor.cpp
```

---

## System 3 — Kinetic Locomotion & Terrain Physics

### Problem
Standard RPG locomotion allows characters to sprint up 45° inclines at flat-road speed. Mahlanya moves through the actual physics of Eswatini terrain: anisotropic friction tensors per material, biomechanical fatigue at altitude, tracking physics for footprint depth simulation, and procedural slip/recovery driven by Chaos physics.

### Custom C++ Plugin — `LocomotionPhysicsPlugin`

**Terrain Material Friction Tensors** (anisotropic 3×3 per material):
```cpp
// FTerrainFrictionTensor — full anisotropic friction
struct FTerrainFrictionTensor
{
    float StaticDry;      // μ_s when dry
    float StaticWet;      // μ_s when wet (from rain saturation state)
    float DynamicDry;     // μ_d when dry
    float DynamicWet;     // μ_d when wet
    float AnisotropyAxis; // Bearing in degrees of max-grip direction
                          // (shale: perpendicular to bedding = high grip,
                          //  parallel to bedding = low grip)
    float AnisotropyRatio;// Ratio of grip across vs along anisotropy axis
};

// Runtime lookup table keyed by UPhysicalMaterial*
static const TMap<FName, FTerrainFrictionTensor> FrictionRegistry = {
    { "PM_GraniteWet",    { 0.65f, 0.38f, 0.55f, 0.28f, 0.f,   1.0f } },
    { "PM_GraniteDry",    { 0.82f, 0.82f, 0.72f, 0.72f, 0.f,   1.0f } },
    { "PM_ClayWet",       { 0.18f, 0.06f, 0.12f, 0.04f, 0.f,   1.0f } },
    { "PM_ClayDry",       { 0.55f, 0.55f, 0.42f, 0.42f, 0.f,   1.0f } },
    { "PM_ShaleWet",      { 0.45f, 0.22f, 0.35f, 0.14f, 235.f, 3.2f } },
    { "PM_GrasslandDry",  { 0.60f, 0.60f, 0.48f, 0.48f, 0.f,   1.0f } },
    { "PM_BurnedGrass",   { 0.12f, 0.08f, 0.09f, 0.06f, 0.f,   1.0f } },
    { "PM_RiverSand",     { 0.38f, 0.28f, 0.30f, 0.20f, 0.f,   1.0f } },
};
```

**Slope Dot Product & Instability Computation**:
```cpp
void ULocomotionPhysicsComponent::TickComponent(float DeltaTime, ...)
{
    // 1. Surface normal from multi-trace (5-point foot trace)
    FVector SurfaceNormal = ComputeWeightedSurfaceNormal();
    
    // 2. Movement direction vector (normalised)
    FVector MoveDir = GetOwner()->GetVelocity().GetSafeNormal();
    
    // 3. Slope severity: dot product of uphill vector and surface normal
    float SlopeDot = FVector::DotProduct(MoveDir, SurfaceNormal);
    
    // 4. Friction from anisotropic tensor
    float EffectiveFriction = ComputeAnisotropicFriction(
        CurrentMaterial, MoveDir, bIsWet, SlopeDot);
    
    // 5. Required friction = sin(slope_angle) for uphill motion
    float RequiredFriction = FMath::Sin(FMath::Acos(SlopeDot));
    
    // 6. Instability if effective < required
    float InstabilityMargin = RequiredFriction - EffectiveFriction;
    if (InstabilityMargin > 0.f)
        TriggerSlip(InstabilityMargin, SurfaceNormal);
}
```

**Biomechanical Model**:
- Centre-of-mass recalculates each frame based on equipped item mass and position (shield offset, water calabash slosh via rigid body fluid proxy)
- **Altitude fatigue**: `StaminaDrainMultiplier = 1.0 + max(0, (Altitude_m - 800) / 1000) * 0.40` — at 1800m Highveld, stamina drains 40% faster
- **Fatigue curve**: Exponential depletion `dS/dt = -BaseRate * SpeedFactor * AltitudeMult`, logarithmic recovery `dS/dt = RecoveryRate * ln(1 + rest_duration)`
- **Instability → Chaos slip**: When `InstabilityMargin > 0.15`, Chaos physics takes over character root; Umshiza stick impulse ray-cast fires into terrain (player input) to arrest slide

**Tracking Physics**:
```cpp
// Footprint depth (m) = character mass / (foot_area_m2 * terrain_hardness_pa)
float FootprintDepth = CharacterMass / (FootContactArea * TerrainHardnessPa);

// Age footprint: rain fills at rate proportional to RainIntensity
// Sun bakes: after N hours dry, hardness of print increases (locked depth)
void UFootprintManagerComponent::AgePrints(float DeltaTime,
                                            float RainIntensity,
                                            float SunIntensity);
```

- Footprint depth written as negative displacement to `URuntimeHeightmapComponent`
- Player reads footprint age via `UTrackingPerceptionComponent` which computes a "freshness score" from depth delta vs current weather state

**Herding Mechanics**:
- Cattle herd as aggregate rigid body: each animal has individual mass + friction state
- Herd momentum = weighted mean of individual momenta
- Player applies steering boundary forces (shouts, Umshiza gestures) as repulsion/attraction vectors
- Stampede state: emergent when collective momentum exceeds threshold; individual animal Chaos ragdolls

### Control Rig + Motion Warping (UE5 Native)
- `CR_Mahlanya`: Full-body IK with 5-bone spine chain, 2-bone leg IK, pole vectors tracking surface tangents
- `MWP_SlopeAdapt`: Motion Warping profile reads surface angle; warps root rotation up to ±35° to match terrain without foot-skating
- `ABP_Mahlanya`: Animation Blueprint state machine with states: Walk, Run, Crouch-Walk, Climb, Wade (shallow), Swim, Slip-Recover, Slide-Arrest

### Critical Files
```
/Plugins/LocomotionPhysicsPlugin/
├── Source/LocomotionPhysicsPlugin/
│   ├── ULocomotionPhysicsComponent.cpp
│   ├── FTerrainFrictionTensor.h
│   ├── UFootprintManagerComponent.cpp
│   └── UHerdingController.cpp

/Game/Characters/Mahlanya/
├── SK_Mahlanya.uasset               ← Skeletal mesh
├── ABP_Mahlanya.uasset              ← Animation Blueprint
├── CR_Mahlanya.uasset               ← Control Rig
├── MWP_SlopeAdapt.uasset            ← Motion Warping profile
└── PhysicsAsset_Mahlanya.uasset     ← Chaos physics asset
```

---

## System 4 — Atmospheric Physics & Microclimates

### Problem
Eswatini experiences dramatic microclimatic variation driven by its extreme topography. Highveld fog, Lowveld haze, orographic rainfall, valley inversion, and lightning are not aesthetic choices — they are climatological facts that alter gameplay (visibility, stealth, traversal difficulty, spiritual event triggers).

### Offline Pipeline (`/pipeline/atmosphere/`)

**Step 1 — Spectral Sky LUT Computation** (`compute_sky_luts.py`)
- Implements **Hosek-Wilkie spectral sky model** at Eswatini latitude (26°S)
- Computes sky radiance across 16 wavelength bands (380–720nm, 20nm steps) for:
  - Sun elevations: 0°–90° (1° steps)
  - Turbidity (Mie aerosol loading): T = 1.5 (clean Highveld) to T = 6.0 (dusty Lowveld haze)
  - Two altitude bands: Highveld (1700m), Lowveld (250m) — different Rayleigh density
- Exports 3D LUT textures (sun_elevation × turbidity × wavelength) as 32-bit EXR:
  - `sky_lut_highveld.exr` — crisp, blue, low-aerosol
  - `sky_lut_lowveld.exr` — warm, hazy, high-Mie
- Also exports **lunar phase table** (`lunar_calendar_1750_1910.json`): pre-computed moon phase for every day across the game's historical span (relevant to Incwala and Umhlanga timing)
- Exports **19th-century star field** (`starfield_26S_J1850.json`): RA/Dec catalog for 8,000 stars visible at 26°S in epoch J1850.0

**Step 2 — SAWS Climatology Integration** (`build_weather_tables.py`)
- Loads SAWS historical weather summary data for Eswatini stations (Manzini, Big Bend, Pigg's Peak, Mbabane)
- Builds per-month probability distributions:
  - Thunderstorm probability by hour (peak: 14:00–18:00, October–March)
  - Cold front passage frequency (April–September)
  - Daily temperature range per altitude band
  - Mean wind direction and speed per season
- Exports: `weather_seasonal_table.json`

### Custom HLSL Sky Shader (`/Game/Atmosphere/Materials/`)

**M_SwaziSkyAtmosphere.uasset** (replaces UE5 built-in sky):
```hlsl
// SwaziSkyAtmosphere.usf
// Samples Hosek-Wilkie LUT with runtime sun elevation + turbidity params

Texture3D HighveldSkyLUT;  // Bound from sky_lut_highveld.exr
Texture3D LowveldSkyLUT;   // Bound from sky_lut_lowveld.exr

float4 SampleSkyRadiance(float3 ViewDir, float SunElevation,
                          float Turbidity, float AltitudeLerp)
{
    // Lerp between Highveld and Lowveld LUT based on player altitude
    // WavelengthBand: loop index 0–15 iterating the 16 spectral bands (380–720nm)
    float3 UVW = float3(SunElevation / 90.0,
                        (Turbidity - 1.5) / 4.5,
                        WavelengthBand / 15.0);
    float4 HighveldSample = HighveldSkyLUT.Sample(SkyLUTSampler, UVW);
    float4 LowveldSample  = LowveldSkyLUT.Sample(SkyLUTSampler, UVW);
    return lerp(LowveldSample, HighveldSample, AltitudeLerp);
}
```

- Single `DustHumidityParam` float drives continuous Mie↔Rayleigh transition (0 = clean Highveld blue, 1 = dusty Lowveld amber-haze)
- Southern hemisphere star field rendered as point sprite instanced mesh, driven by `starfield_26S_J1850.json` (historically accurate)
- Moon rendered with phase from `lunar_calendar_1750_1910.json` for the in-game date

### Custom C++ Plugin — `MicroclimateEngine`

**Barometric Pressure 1D Model**:
```cpp
// Simplified barometric pressure sim driving weather state machine
struct FMicroclimateState
{
    float PressureHPa;          // Current surface pressure
    float PressureTendencyHPa;  // dP/dt over last 3 hours
    float CloudDensityFraction; // 0–1 (drives Volumetric Cloud coverage)
    float FogBaseAltitude_m;    // Altitude of fog base (valley inversion)
    float PrecipitationIntensity; // mm/hr (feeds ErosionRuntimePlugin)
    float WindSpeed_ms;
    float WindBearing_deg;
    float TurbidityParam;       // Fed to sky shader
};
```

**Orographic Rainfall**:
- Each tick, wind vector is cast against the terrain heightmap gradient
- If wind component is upslope and terrain elevation delta > 400m in 10km: condensation probability spikes
- Precipitation fires `OnRainIntensityChanged` event consumed by `ErosionRuntimePlugin`, `LocomotionPhysicsPlugin` (wet friction), stealth system

**Valley Inversion Fog**:
- Cold air drainage simulation at night: exponential decay of cold air mass down valley channels (extracted from river network shapefile)
- Great Usuthu gorge fills from valley floor up; fog base altitude is a runtime variable clamped by inversion height
- Feeds `ExponentialHeightFog` actor in UE5 with dynamic `FogDensity` and `FogHeightFalloff`

**Lightning System**:
- Cloud charge accumulation model: charge = f(cloud_density, storm_duration, altitude)
- When charge threshold exceeded, lightning strike fires at a terrain-sampled location within storm cell
- Strike location checked against `river_network.shp` features and PCG-placed tree instances
- Struck tree: `BurntTree` material swap + persistent world flag (saved to slot)
- Struck tree acquires `ELightningTreeState::Sacred` flag (Swazi spiritual significance: Umkhosi Wemali; NPC responses change)

**Controlled Burns (Historical Swazi Agricultural Practice)**:
- NPCs and player can ignite grass in winter (May–August) — historically accurate veld management
- Fire propagation: cellular automaton on biome zone grid; wind direction biases spread
- Smoke: `NiagaraSystem_SmokePlume` with Mie-accurate phase function settings
- Post-burn: `PM_BurnedGrass` physical material (near-zero friction) lasts until next rain season
- Stealth: `SmokeVolumetricDensity` at player coordinates reads into stealth manager

**Stealth Meter Integration**:
```cpp
float UStealthManagerComponent::ComputeDetectionRange() const
{
    float FogDensityAtPlayer = MicroclimateSubsystem->GetVolumetricDensityAt(
        GetOwner()->GetActorLocation());
    float SmokeDensityAtPlayer = MicroclimateSubsystem->GetSmokeDensityAt(
        GetOwner()->GetActorLocation());
    
    float VisibilityMultiplier = 1.0f
        - FogDensityAtPlayer * 0.8f
        - SmokeDensityAtPlayer * 0.6f;
    
    return BaseDetectionRange * FMath::Clamp(VisibilityMultiplier, 0.05f, 1.0f);
}
```

### Critical Files
```
/pipeline/atmosphere/
├── compute_sky_luts.py
└── build_weather_tables.py

/Game/Atmosphere/
├── Materials/M_SwaziSkyAtmosphere.uasset
├── Shaders/SwaziSkyAtmosphere.usf
├── LUTs/sky_lut_highveld.exr
├── LUTs/sky_lut_lowveld.exr
└── Data/weather_seasonal_table.json

/Plugins/MicroclimateEngine/
├── Source/MicroclimateEngine/
│   ├── UMicroclimateSubsystem.cpp
│   ├── FMicroclimateState.h
│   ├── UOrographicRainfallComponent.cpp
│   ├── UValleyInversionComponent.cpp
│   ├── ULightningSystem.cpp
│   └── UStealthManagerComponent.cpp
```

---

## System 5 — Acoustic Wave Ray-Tracing

### Problem
Sound in Eswatini is a survival tool. The call of the Hadeda ibis at dawn, the echo of drums in the Lubombo canyon, the deadened voice inside a beehive hut, the anomalous long-range propagation of the herdboys' whistle in morning temperature inversions — none of these behaviours are possible with stereo audio files or simple reverb presets.

### Offline Pipeline (`/pipeline/audio/`)

**Step 1 — Impulse Response Pre-computation** (`compute_acoustic_irs.py`)

Implements **image-source method + Monte Carlo path tracing hybrid** for 6 environment archetypes:

```python
# Per-material absorption coefficients (8 octave bands: 125Hz–16kHz)
MATERIAL_ABSORPTION = {
    'granite':      [0.02, 0.02, 0.03, 0.03, 0.04, 0.05, 0.05, 0.06],
    'thatch_grass': [0.15, 0.25, 0.40, 0.55, 0.65, 0.70, 0.72, 0.70],
    'clay_earth':   [0.35, 0.40, 0.45, 0.50, 0.55, 0.55, 0.60, 0.60],
    'dry_grass':    [0.25, 0.35, 0.45, 0.55, 0.60, 0.65, 0.65, 0.65],
    'water_surface':[0.01, 0.01, 0.02, 0.02, 0.03, 0.03, 0.05, 0.05],
    'open_sky':     [1.00, 1.00, 1.00, 1.00, 1.00, 1.00, 1.00, 1.00],
}

def compute_ir_image_source(geometry, source_pos, receiver_pos,
                              max_order=3):
    """First 3 reflection orders via image source method (exact early reflections)."""
    ...

def compute_ir_monte_carlo(geometry, source_pos, receiver_pos,
                            n_rays=10000, max_bounces=20):
    """Late reverberation tail via Monte Carlo path tracing."""
    ...

ENVIRONMENT_ARCHETYPES = [
    'granite_cave',        # RT60 ~4.2s, highly reflective
    'thatched_hut_int',    # RT60 ~0.15s, highly absorbent, intimate
    'lubombo_canyon',      # Complex multi-reflection, long delay echoes
    'open_highveld',       # Near-anechoic, slight ground reflection
    'usuthu_gorge',        # Parallel wall flutter echo, water background
    'riverbed_floodplain', # Soft broadband absorption, low RT60
]
```

**Step 2 — Atmospheric Propagation Tables** (`compute_propagation_tables.py`)
- Per weather condition: wind speed/direction, temperature gradient (inversion or normal lapse), humidity
- Computes: effective sound speed profile, refraction curvature, high-frequency absorption rate (dB/km)
- Exports lookup table: `atmospheric_propagation.json`

**Step 3 — Bioacoustic Library** (`build_bioacoustic_library.py`)
- Assembles frequency-domain profiles for 28 bird species endemic to Eswatini altitude bands
- Assigns species to biome zones (from terrain pipeline output)
- Defines active time windows (dawn chorus, dusk, nocturnal)
- Insect chorus: summer cicada (2.5–4kHz band), winter silence, rain frog chorus (0.8–1.5kHz after rain)
- Exports: `bioacoustic_library.json`

### Custom C++ Plugin — `GeometricAudioPlugin`

**Runtime Geometric Ray-Caster**:
```cpp
UCLASS()
class UGeometricAudioComponent : public UAudioComponent
{
    GENERATED_BODY()
public:
    // Called each audio frame for dynamic open-air sources
    void ComputeRuntimeIR(const FVector& SourceLocation,
                           const FVector& ListenerLocation);

private:
    // Casts N rays from source; reads UAcousticMaterialComponent tags
    // from hit geometry; accumulates per-band energy response
    static constexpr int32 NUM_RAYS = 64;
    static constexpr int32 MAX_BOUNCES = 6;
    
    struct FRayAcousticHit {
        float Distance_m;
        float DelaySeconds;     // Distance / 343.0f
        float EnergyPerBand[8]; // Per octave-band energy after absorption
    };
    
    TArray<FRayAcousticHit> TraceAcousticRays(const FVector& Origin,
                                               const FVector& Target);
    FAudioImpulseResponse BuildRuntimeIR(
        const TArray<FRayAcousticHit>& Hits);
};
```

**Atmospheric Acoustic Effects**:
```cpp
// Wind effect: modify effective sound propagation speed
float EffectiveSoundSpeed(FVector PropagationDir, float AirTemp_C,
                           FVector WindVelocity)
{
    float SpeedOfSound = 331.3f + 0.606f * AirTemp_C; // m/s
    float WindComponent = FVector::DotProduct(PropagationDir.GetSafeNormal(),
                                               WindVelocity);
    return SpeedOfSound + WindComponent;
}

// Temperature inversion: sound bends toward cold air layer
// Modelled as additional effective propagation distance (longer range)
float InversionRangeMultiplier(float TempGradient_C_per_100m)
{
    if (TempGradient_C_per_100m > 0) // Inversion: warm over cold
        return 1.0f + TempGradient_C_per_100m * 0.15f;
    return 1.0f; // Normal lapse rate: no anomalous propagation
}
```

**Swazi Long-Distance Whistle**:
- Whistle language modelled as 2.0–3.5kHz band point source with figure-8 directivity
- `UGeometricAudioComponent` computes effective range using propagation tables + runtime IR
- Visual feedback: a subtle particle system shows the acoustic wavefront radius as a faint shimmer ring (disabled in combat, enabled in survival/tracking mode)

**MetaSounds Integration**:
- `MS_Environment_Master`: MetaSounds graph routing all environment audio through appropriate convolution reverb (selects pre-baked IR from archetype based on `UAcousticZoneComponent` the player is in)
- `MS_DrumTigubu`: Procedural rhythm engine for Tigubu drums; MetaSounds grain synthesizer for authentic timbre variation
- `MS_ImbongiVoice`: Pitch and formant variation applied to Imbongi voice recordings for reverb-zone-appropriate spatialization
- `MS_Bioacoustics_Master`: Queries `bioacoustic_library.json` each dawn/dusk; procedurally layers species calls based on player biome, season, time-of-day, weather

**UAcousticMaterialComponent**: Component attached to every static mesh material that carries acoustic absorption coefficients. Queried by `GeometricAudioPlugin` during ray-casting.

### Critical Files
```
/pipeline/audio/
├── compute_acoustic_irs.py
├── compute_propagation_tables.py
└── build_bioacoustic_library.py

/Game/Audio/
├── ImpulseResponses/
│   ├── IR_GraniteCave.wav
│   ├── IR_ThatchedHutInt.wav
│   ├── IR_LubomboCanyon.wav
│   ├── IR_OpenHighveld.wav
│   ├── IR_UsuthuGorge.wav
│   └── IR_RiverbedFloodplain.wav
├── MetaSounds/
│   ├── MS_Environment_Master.uasset
│   ├── MS_DrumTigubu.uasset
│   ├── MS_ImbongiVoice.uasset
│   └── MS_Bioacoustics_Master.uasset
└── Data/
    ├── atmospheric_propagation.json
    └── bioacoustic_library.json

/Plugins/GeometricAudioPlugin/
├── Source/GeometricAudioPlugin/
│   ├── UGeometricAudioComponent.cpp
│   ├── UAcousticMaterialComponent.cpp
│   ├── UAcousticZoneComponent.cpp
│   └── FAtmosphericAcoustics.cpp
```

---

## System 6 — Historical Knowledge Graph

### Problem
Historical accuracy requires that no NPC, quest, or world-text can assert something that is historically false. A constraint-based knowledge graph ensures this structurally, not via manual review.

### Data Model (`/pipeline/history/`)

**Neo4j-compatible JSON-LD schema** (`build_knowledge_graph.py`):
```json
// Entity types and example nodes
{
  "entities": {
    "Person": {
      "NB001": { "name": "Ngwane III", "title": "King", "lived": "~1780–1815",
                  "clan": "Nkosi Dlamini", "capital": "PL_Zombodze" },
      "SW001": { "name": "Sobhuza I", "title": "King", "lived": "~1795–1839",
                  "clan": "Nkosi Dlamini", "capital": "PL_Lobamba" },
      "MS002": { "name": "Mswati II", "title": "King", "lived": "~1826–1865" }
    },
    "Place": {
      "PL_Lobamba": { "name": "Lobamba", "type": "Royal Kraal",
                       "coords_utm": [348200, 2980400],
                       "era_active": [1820, 1906] },
      "PL_Nyonyane": { "name": "Nyonyane (Execution Rock)",
                        "type": "Landmark", "coords_utm": [320000, 2995000] }
    },
    "Battle": {
      "BT001": { "name": "Battle of Lubombo", "year": 1846,
                  "victor": "SW001", "participants": ["SW001", "CL_Gaza"] }
    },
    "Event": {
      "EV001": { "name": "Mfecane", "period": [1815, 1840],
                  "type": "Regional Upheaval" },
      "EV002": { "name": "Concession Rush", "period": [1880, 1894],
                  "type": "Colonial Pressure" }
    }
  },
  "relations": [
    { "from": "SW001", "rel": "succeeded",    "to": "NB001" },
    { "from": "SW001", "rel": "fought_at",    "to": "BT001" },
    { "from": "BT001", "rel": "happened_at",  "to": "PL_Lubombo" },
    { "from": "EV002", "rel": "pressured",    "to": "SW001" }
  ]
}
```

**Historical Accuracy Validator** (`validate_content.py`):
- CI/CD check that runs on every content commit
- Parses NPC dialogue YAML files, quest description strings, world-text strings
- Extracts entity references (person names, place names, dates, event names)
- Queries knowledge graph to verify: is this claim consistent with known relations?
- Fails the build if a contradiction is detected

**UE5 Integration** (`UHistoricalKnowledgeGraphSubsystem`):
```cpp
// Query interface exposed to Blueprint and other subsystems
FHistoricalEntity GetEntity(FName EntityID);
TArray<FHistoricalRelation> GetRelationsOf(FName EntityID, FName RelType);
bool ExistedDuring(FName EntityID, int32 InGameYear);
FString GetPeriodAccurateTitle(FName PersonID, int32 InGameYear);
TArray<FName> GetAlliesOf(FName ClanOrPersonID, int32 InGameYear);
```

### Critical Files
```
/pipeline/history/
├── build_knowledge_graph.py
├── validate_content.py
└── data/
    ├── persons.json
    ├── places.json
    ├── battles.json
    ├── events.json
    ├── relations.json
    └── material_culture.json

/Plugins/KnowledgeGraphPlugin/
├── Source/KnowledgeGraphPlugin/
│   ├── UHistoricalKnowledgeGraphSubsystem.cpp
│   └── FHistoricalGraphQuery.cpp
└── Data/knowledge_graph.ndjson    ← packed for runtime queries
```

---

## System 7 — Living Political Economy

### Problem
The Swazi economy is cattle-based. Cattle = currency, status, military power, spiritual capital. Colonial concession pressure is an economic infection. The political economy simulation generates quests emergently from economic stress, not from hardcoded scripts.

### Data Model & Simulation (`/pipeline/economy/`)

**Simulation loop** (runs at game-time tick, roughly 1 real second = 1 game day):
```cpp
UCLASS()
class UEconomySimulatorSubsystem : public UWorldSubsystem
{
    GENERATED_BODY()
    
    struct FClanEconomicState {
        int32 CattleCount;           // Primary wealth metric
        float PoliticalStrength;     // 0–1, derived from cattle + alliances
        float ColonialPressure;      // 0–1, concession infection spreading
        float DroughtStress;         // Fed from MicroclimateEngine seasonal data
        TMap<FName, float> Tribute;  // Outbound tribute flows
        TArray<FName> Concessions;   // Accepted concessions (irreversible)
    };
    
    // Called every game-day tick
    void SimulateEconomyTick(float GameDayDelta);
    
    // Concession spread: probability of acceptance = f(cattle, pressure, drought)
    float ComputeConcessionAcceptanceProbability(FName ClanID);
    
    // Triggers EmergentNarrativeEngine when thresholds crossed
    void CheckQuestTriggers();
};
```

**Cattle raid probability**: `P(raid) = DroughtStress * (1 - PoliticalStrength) * 0.3`
**Lobola transaction**: triggers `USibayaEngineSubsystem::OnWifeMarried` → settlement recomputes
**Colonial concession infection**: once a border chief accepts, adjacent chiefs' pressure increases by `0.15` per game-month

### Critical Files
```
/Plugins/EconomySimulatorPlugin/
├── Source/EconomySimulatorPlugin/
│   ├── UEconomySimulatorSubsystem.cpp
│   ├── FClanEconomicState.h
│   └── UConcessionSpreadComponent.cpp
```

---

## System 8 — Language & Cultural Protocol Engine

### siSwati Language Layer
- Morphological ruleset for agglutinative siSwati name generation: prefix (class marker) + root + suffix
- Person name generator produces semantically meaningful names (e.g., "Mahlanya" = "reckless/daring one")
- All NPC greetings, titles, and honorifics assembled from a `siswati_morphology.json` ruleset
- Class 1a nouns (persons) decline correctly in possessive and locative cases for dialogue

### Protocol State Machine (`UProtocolStateComponent`)
```cpp
enum class EProtocolStatus : uint8 {
    Required,       // Must perform this protocol in this interaction
    Optional,       // Socially expected but not mandatory
    Violated,       // Player failed to observe
    Observed        // Player correctly performed
};

struct FProtocol {
    FName ID;
    FString Description;
    EProtocolStatus Status;
    float RelationshipImpact;   // Positive = correct, Negative = violated
};

// Example protocols loaded from protocols.json:
// - PROT_GreetElder:     crouch + clap + "Sawubona" → +0.08 relationship
// - PROT_AddressInkosi:  remove headgear + indirect speech → +0.15
// - PROT_InhloniphhoLaw: use substitute vocabulary near in-laws → +0.05
```

- Protocol violations accumulate `RelationshipDamage` score → NPC hostility thresholds
- Inhlonipho (avoidance vocabulary): system knows relationship graph; substitutes taboo words automatically in NPC generated dialogue

### Critical Files
```
/Plugins/CulturalProtocolPlugin/
├── Source/CulturalProtocolPlugin/
│   ├── UProtocolStateComponent.cpp
│   └── USiSwatiLanguageSubsystem.cpp
└── Data/
    ├── siswati_morphology.json
    └── protocols.json
```

---

## System 9 — Ecological Simulation

### Fauna Population Dynamics (`UEcologySimulatorSubsystem`)
- **Lotka-Volterra ODE system** for 6 prey-predator pairs (lion/impala, leopard/warthog, crocodile/fish, wild dog/zebra, python/small mammals, Black mamba/rodents)
- Population counts discretised into 10×10 km grid cells
- Each grid cell updates every game-week: `dN/dt = rN - αNP`, `dP/dt = βαNP - δP`
- Fauna agents spawn/despawn in the player's streaming cell based on grid population count
- **Seasonal migration**: Elephant herds follow water-source graph (river network shapefile) in dry season; wildebeest follow biome-zone boundaries

### Flora Distribution (`UFloraManagerComponent`)
- Biome zone raster from terrain pipeline drives 5 vegetation types:
  - Afromontane forest (Highveld ravines, elevation > 1400m, high rainfall zones)
  - Swazi thornveld (Middleveld, 600–1200m)
  - Acacia savanna (Lowveld, < 600m)
  - Riparian gallery forest (river corridor buffers, 200m either side of river_network.shp)
  - High-altitude grassland (Highveld plateau, > 1500m, wind-exposed)
- PCG Framework places flora assets using biome zone weight maps as input
- **Medicinal plant registry**: 40 species (`medicinal_plants.json`) with biome zone, season, and altitude constraints; Inyanga NPC quests draw from this registry

### Critical Files
```
/Plugins/EcologySimulatorPlugin/
├── Source/EcologySimulatorPlugin/
│   ├── UEcologySimulatorSubsystem.cpp  ← Lotka-Volterra grid
│   ├── UFaunaAgentComponent.cpp
│   └── UFloraManagerComponent.cpp
└── Data/
    ├── fauna_species.json
    └── medicinal_plants.json
```

---

## System 10 — Emergent Narrative Engine

### Simulation-Driven Quest Generation (`UEmergentNarrativeSubsystem`)

Quests are NOT hardcoded scripts. They are threshold-triggered consequence events from simulation state:

```cpp
struct FQuestTriggerRule {
    FName QuestTemplateID;
    TArray<FSimCondition> Conditions;   // All must be true to fire
    float Cooldown_GameDays;
    EQuestPriority Priority;
};

// Example trigger rules loaded from quest_trigger_rules.json:
// TRIGGER: DroughtStress > 0.7 AND CattleCount < 50 → QUEST: "The Raid of Need"
// TRIGGER: ColonialPressure > 0.6 AND ChiefStrength < 0.4 → QUEST: "The Concession Betrayal"
// TRIGGER: LightningStruck(SacredTree) → QUEST: "The Omen of Mdzimba"
// TRIGGER: HistoricalDate == BT001.year → WORLD_EVENT: "Battle of Lubombo"
```

**Historical Event Calendar** (`UHistoricalCalendarSubsystem`):
- Major documented events fire at historically correct in-game dates
- Mfecane waves (1815–1840): periodic raids from the north
- 1846 Lubombo battle: major world event
- 1880s concession rush: colonial pressure on political graph spikes
- 1898 Bhunu's trial: political crisis arc
- Events are world-level occurrences that reset regional political graph edges

**Procedural Imbongi (Praise Poet) System**:
```
[Verse template in siSwati morphological form]
"{Praiseword} {PlayerName_Root}{Suffix_Honorific},
 {Verb_Past_Tense} {HistoricalEnemy_Name} {Locative_PlaceName},
 {SimState_Achievement_Phrase}."
```
- Queries knowledge graph for historically appropriate enemy names and place epithets
- Queries economy simulator for player's cattle wealth
- Queries ecology simulation for hunts completed
- Outputs verse in siSwati with English gloss subtitle

**Player Chronicle**: Each playthrough writes a structured JSON chronicle (`playthrough_chronicle.json`) recording all significant simulation events in period-appropriate voice. Exportable.

### Critical Files
```
/Plugins/EmergentNarrativePlugin/
├── Source/EmergentNarrativePlugin/
│   ├── UEmergentNarrativeSubsystem.cpp
│   ├── UHistoricalCalendarSubsystem.cpp
│   └── UImbongiGenerator.cpp
└── Data/
    ├── quest_trigger_rules.json
    └── imbongi_verse_templates.json
```

---

## Full Technology Stack

### Offline Science Layer (Python ≥ 3.11)
| Library | Version | Purpose |
|---|---|---|
| `gdal` | 3.8+ | DEM acquisition, reprojection, mosaicking |
| `rasterio` | 1.3+ | Raster I/O, windowed processing |
| `numpy` | 1.26+ | Vectorised erosion simulation |
| `scipy` | 1.12+ | Voronoi, flow accumulation, ODE solver |
| `numba` | 0.59+ | JIT-compiled erosion for CPU acceleration |
| `cupy` | 13.0+ | CUDA-accelerated erosion (NVIDIA GPU) |
| `shapely` | 2.0+ | River network geometry, spatial operations |
| `geopandas` | 0.14+ | Shapefile I/O, coordinate transforms |
| `pyproj` | 3.6+ | CRS transformations (WGS84 ↔ UTM36S) |
| `trimesh` | 4.0+ | 3D mesh operations for acoustic IR geometry |
| `neo4j` | 5.0+ | Knowledge graph I/O |
| `click` | 8.1+ | CLI for pipeline scripts |
| `pytest` | 8.0+ | Unit tests for all pipeline stages |

### UE5 Runtime
- **Unreal Engine**: 5.4+ (minimum)
- **Language**: C++17 + Blueprints
- **Required plugins**: Nanite Landscape, World Partition, PCG Framework, Control Rig, Motion Warping, Chaos Physics, MetaSounds, Gameplay Ability System, Online Subsystem (for co-op)

### Custom C++ Plugins (9 total)
All plugins: C++17, builds as UE5 module, tested via Unreal's Automation Testing Framework

`ErosionRuntimePlugin` and `MicroclimateEngine` would otherwise form a circular dependency (each needs the other's data). This is broken by a thin shared event bus plugin `SimulationBusPlugin` that neither plugin depends on the other to use — they both publish/subscribe via the bus.

| Plugin | Primary Dependency |
|---|---|
| `SimulationBusPlugin` | None (loaded first; provides `USimulationBusSubsystem`) |
| `ErosionRuntimePlugin` | UE5 Compute Shaders, `SimulationBusPlugin` |
| `MicroclimateEngine` | UE5 Volumetric Clouds, `SimulationBusPlugin` |
| `SibayaEngine` | PCG Framework, `SimulationBusPlugin` |
| `LocomotionPhysicsPlugin` | Control Rig, Chaos Physics, `SimulationBusPlugin` |
| `GeometricAudioPlugin` | MetaSounds, UE5 Audio Engine, `SimulationBusPlugin` |
| `KnowledgeGraphPlugin` | ndjson bundled data |
| `EconomySimulatorPlugin` | `SibayaEngine`, `MicroclimateEngine`, `KnowledgeGraphPlugin` |
| `EmergentNarrativePlugin` | All other plugins |

`SimulationBusPlugin` (`/Plugins/SimulationBusPlugin/`) exposes `USimulationBusSubsystem` — a `UWorldSubsystem` with typed multicast delegates:
- `FOnRainIntensityChanged` — published by `MicroclimateEngine`, subscribed by `ErosionRuntimePlugin` and `LocomotionPhysicsPlugin`
- `FOnTerrainSaturationChanged` — published by `ErosionRuntimePlugin`, subscribed by `MicroclimateEngine`
- `FOnSettlementDemographicChanged` — published by `EconomySimulatorPlugin`, subscribed by `SibayaEngine`
- `FOnQuestTriggerConditionMet` — published by any subsystem, subscribed by `EmergentNarrativePlugin`

Plugin load order enforced in `MahlanyaRPG.uproject`: `SimulationBusPlugin` → all simulation plugins → `EmergentNarrativePlugin`.

---

## Project Directory Structure

```
/MahlanyaRPG/
├── pipeline/                    ← Python offline science layer
│   ├── terrain/
│   │   ├── acquire_dem.py
│   │   ├── build_hardness_map.py
│   │   ├── erode_terrain.py
│   │   ├── extract_rivers.py
│   │   └── export_to_ue5.py
│   ├── settlements/
│   │   ├── compute_voronoi.py
│   │   └── build_political_graph.py
│   ├── atmosphere/
│   │   ├── compute_sky_luts.py
│   │   └── build_weather_tables.py
│   ├── audio/
│   │   ├── compute_acoustic_irs.py
│   │   ├── compute_propagation_tables.py
│   │   └── build_bioacoustic_library.py
│   ├── history/
│   │   ├── build_knowledge_graph.py
│   │   ├── validate_content.py
│   │   └── data/
│   ├── requirements.txt
│   ├── Makefile                 ← `make full-pipeline` runs all steps in order
│   └── tests/                  ← pytest test suite for all pipeline stages
│
├── MahlanyaRPG.uproject        ← UE5 project file
│
├── Source/MahlanyaRPG/         ← UE5 C++ game module
│   ├── Characters/
│   ├── GameModes/
│   └── UI/
│
├── Plugins/                    ← 9 custom plugins
│   ├── SimulationBusPlugin/       ← Loaded first; shared event bus
│   ├── ErosionRuntimePlugin/
│   ├── SibayaEngine/
│   ├── LocomotionPhysicsPlugin/
│   ├── MicroclimateEngine/
│   ├── GeometricAudioPlugin/
│   ├── KnowledgeGraphPlugin/
│   ├── EconomySimulatorPlugin/
│   └── EmergentNarrativePlugin/
│
├── Content/                    ← UE5 assets (tracked via Git LFS)
│   ├── Terrain/
│   ├── Settlements/
│   ├── Characters/
│   ├── Atmosphere/
│   ├── Audio/
│   └── VFX/
│
├── Config/
│   ├── DefaultEngine.ini
│   ├── DefaultGame.ini
│   └── DefaultScalability.ini  ← Mobile scalability tiers defined here
│
├── docs/
│   └── superpowers/
│       └── specs/
│           └── 2026-06-24-mahlanya-rpg-design.md
│
├── .github/
│   └── workflows/
│       ├── pipeline-ci.yml      ← Tests Python pipeline on PR
│       ├── content-validate.yml ← Runs validate_content.py on dialogue PRs
│       └── ue5-build.yml        ← UE5 build check (Linux build server)
│
├── .gitignore
├── .gitattributes               ← Git LFS for .uasset, .r16, .exr, .wav
└── README.md
```

---

## CI/CD Pipeline

### GitHub Actions Workflows

**`pipeline-ci.yml`** (triggers on changes to `/pipeline/**`):
1. Spins up Python 3.11 + GDAL environment
2. Runs `pytest pipeline/tests/` — unit tests for every pipeline stage
3. Runs `python pipeline/history/validate_content.py` on all dialogue YAML
4. Reports coverage (target: ≥ 90% pipeline code coverage)

**`content-validate.yml`** (triggers on changes to `Content/Dialogue/**`):
1. Extracts all entity references from dialogue YAML
2. Queries knowledge graph JSON
3. Fails PR if any historically inconsistent claim detected
4. Posts specific violation message to PR review

**`ue5-build.yml`** (triggers on changes to `Source/**` or `Plugins/**`):
1. Compiles all 8 plugins + game module on Linux build server
2. Runs UE5 Automation Test suite
3. Reports compile errors and test failures

### Git LFS Configuration (`.gitattributes`)
```
*.uasset filter=lfs diff=lfs merge=lfs -text
*.umap    filter=lfs diff=lfs merge=lfs -text
*.r16     filter=lfs diff=lfs merge=lfs -text
*.exr     filter=lfs diff=lfs merge=lfs -text
*.wav     filter=lfs diff=lfs merge=lfs -text
*.png     filter=lfs diff=lfs merge=lfs -text
```

---

## Mobile Scalability Architecture

**`DefaultScalability.ini`** defines 3 tiers:

| Feature | PC Ultra | Mobile High | Mobile Low |
|---|---|---|---|
| Terrain erosion | Runtime (GPU CS) | Pre-baked only | Pre-baked only |
| Nanite | Enabled | Disabled (fallback LOD) | Disabled |
| Lumen | Enabled | Disabled (baked lighting) | Disabled |
| Volumetric clouds | Full simulation | Static mesh clouds | Skybox |
| Voronoi settlements | Runtime recompute | Static from JSON | Static from JSON |
| Geometric audio | 64 rays/frame | Pre-baked IR only | Stereo reverb bus |
| Shadow quality | Ray-traced | CSM 4 cascades | CSM 2 cascades |
| Draw distance | 8km | 2km | 1km |

Mobile builds package only the pre-baked artifact set (heightmaps, IRs, JSON layouts, LUTs) — the Python pipeline outputs both the runtime data and the mobile-baked data as separate build targets.

---

## Multiplayer Architecture

All 10 simulation systems are designed for **deterministic seed-reproducibility**:
- Every random draw in every system takes an explicit seed derived from `WorldSeed + SystemID + EntityID`
- Given identical seeds, offline pipeline outputs and runtime simulation produce identical results
- This is the prerequisite for authoritative multiplayer

**Co-op Layer** (Phase 9):
- UE5 Gameplay Ability System (GAS) handles ability replication from day one
- `MicroclimateEngine` runs server-authoritative; clients receive weather state delta updates
- `SibayaEngine` runs server-authoritative; settlement state is replicated
- `EconomySimulatorSubsystem` runs server-authoritative
- `LocomotionPhysicsPlugin` runs client-predicted with server reconciliation (standard UE5 movement replication)
- Player count: 2–4 co-op (designed for the scale of a cattle raid or diplomatic escort)

---

## Phased Implementation Plan

| Phase | Duration | Deliverable | Success Criteria |
|---|---|---|---|
| **0 — Foundation** | 8 weeks | UE5 project scaffold, Git LFS, CI/CD, Python env | All CI/CD pipelines green; blank UE5 project compiles |
| **1 — Terrain** | 16 weeks | Eroded Eswatini heightmap rendering in UE5 with river splines | All 5 river systems visible; biome materials auto-painted |
| **2 — Locomotion** | 12 weeks | Mahlanya walks/runs/slips on correct terrain with IK and friction | Slip occurs on wet clay slope; no slip on dry granite |
| **3 — Settlement** | 12 weeks | Procedural Umuti villages from Voronoi + political graph | Settlement layout communicates hierarchy without UI text |
| **4 — Atmosphere** | 12 weeks | Full atmospheric shaders + weather system + bioacoustics | Fog rolls into Usuthu gorge at night; Hadeda calls at dawn |
| **5 — Audio** | 10 weeks | Geometric acoustic system + pre-baked IRs in MetaSounds | Shout in Lubombo canyon produces measurable delay echo |
| **6 — Historical World** | 16 weeks | Knowledge graph, economy sim, multi-era content, NPC protocols | Content validator passes on all dialogue; economy generates raids |
| **7 — Narrative** | 16 weeks | Emergent quest engine, Imbongi system, Mahlanya's full arc | 3 quests generated emergently from simulation without scripting |
| **8 — Mobile** | 12 weeks | Mobile build consuming pre-baked artifacts at 60fps on mid-range | Stable 60fps on iPhone 14 / Samsung Galaxy S23 |
| **9 — Co-op** | 16 weeks | 2–4 player co-op with authoritative simulation | 2-player cattle raid quest functional end-to-end |
| **10 — Ship** | 8 weeks | Steam + mobile store submission | Certification passed; all CI/CD green |

---

## Verification Strategy

### Per-System Verification

| System | How to verify |
|---|---|
| Terrain | Load eroded heightmap in QGIS; overlay actual Usuthu/Komati river shapefile — rivers must align within 500m |
| Settlements | Spawn 20 villages; verify every Chief's hut faces within ±5° of East; verify no huts in West taboo arc |
| Locomotion | Run Mahlanya up 35° wet-clay slope: must slip. Run same slope on dry granite: must not slip |
| Atmosphere | Verify Hosek-Wilkie LUT matches published chromaticity plots from Hosek & Wilkie 2012 paper |
| Audio | Measure RT60 in granite cave archetype: must be within 10% of published cave acoustic literature values |
| Knowledge Graph | Run `validate_content.py` on all dialogue — zero violations |
| Economy | Simulate 10 game-years: verify cattle raid frequency correlates with drought events |
| Narrative | Verify 3 distinct quest types fire purely from simulation state with zero scripted triggers |

### Integration Verification
- Full pipeline run: `make full-pipeline` → `pytest` → UE5 cook → play through 10 game-minutes in PIE (Play In Editor)
- Historical accuracy review: consult Eswatini National Archives and Matsapha academic sources before Phase 6 content locks
- Cultural accuracy review: engage a siSwati-speaking cultural consultant before Phase 6 ship

---

## Risk Register

| Risk | Severity | Mitigation |
|---|---|---|
| UE5 Nanite Landscape not available on mobile | High | Use traditional heightmap tessellation on mobile tier; verified in Phase 0 |
| CUDA not available on build machine | Medium | NumPy fallback in `erode_terrain.py`; erosion marks as "slow" in CI not "failed" |
| Copernicus DEM missing tiles for border areas | Low | SRTM 30m as fallback for any missing tiles; SRTM tiles cover all of Eswatini |
| Historical knowledge gaps | Medium | Ambiguous facts marked as `"confidence": "low"` in graph; validator warns but does not fail |
| Mobile 60fps target missed | Medium | Aggressive scalability tier; Phase 8 dedicated profiling sprint |
| Plugin inter-dependency ordering issue at startup | Medium | `EmergentNarrativePlugin` declared last in `uplugin` dependency chain; boot order enforced |
