# Mahlanya RPG — Offline Science Pipeline
# Usage: make <target>
#
# Run the full pipeline end-to-end (PC build):
#   make full-pipeline
#
# Run just tests:
#   make test
#
# Individual pipeline stages can be run independently:
#   make terrain-acquire, make terrain-erode, etc.

PYTHON   := python3
PYTEST   := $(PYTHON) -m pytest
PIPELINE := $(PYTHON) -m pipeline
ZIG      := zig

# Zig compute library
ZIG_DIR         := pipeline/compute
ZIG_LIB_OUT     := $(ZIG_DIR)/zig-out/lib/libmahlanya_compute.so

# Configurable paths (override from environment or CLI)
DEM_DIR         ?= outputs/dem
HARDNESS_PATH   ?= outputs/rock_hardness_utm36s.tif
ERODED_PATH     ?= outputs/eroded_utm36s.tif
RIVERS_SHP      ?= outputs/river_network_utm36s.shp
UE5_OUTPUT_DIR  ?= outputs/ue5_export
GEOLOGY_SHP     ?= cache/geology_utm36s.shp
SETTLEMENTS_DIR ?= outputs/settlements
ATMOSPHERE_DIR  ?= outputs/atmosphere
AUDIO_DIR       ?= outputs/audio
KG_PATH         ?= outputs/knowledge_graph.ndjson
CONTENT_DIR     ?= ../Content/Dialogue

BBOX_WEST  ?= 30.79
BBOX_EAST  ?= 32.14
BBOX_SOUTH ?= -27.32
BBOX_NORTH ?= -25.72

EROSION_ITERS ?= 5000
LLOYD_ITERS   ?= 50

.PHONY: all full-pipeline full-pipeline-mobile mobile-package \
        test test-environment test-stubs test-compute \
        zig-build zig-test zig-clean \
        terrain-acquire terrain-hardness terrain-erode terrain-rivers terrain-export \
        settlements-voronoi settlements-graph \
        atmosphere-luts atmosphere-weather \
        audio-irs audio-propagation audio-bioacoustics \
        history-graph history-validate \
        outputs-dirs clean help

# ── Default ─────────────────────────────────────────────────────────────────
all: help

# ── Zig compute library ──────────────────────────────────────────────────────
zig-build:
	@echo "Building libmahlanya_compute.so (Zig SIMD kernels) ..."
	cd $(ZIG_DIR) && $(ZIG) build -Doptimize=ReleaseFast
	@echo "Built: $(ZIG_LIB_OUT)"

zig-test:
	@echo "Running Zig unit tests ..."
	cd $(ZIG_DIR) && $(ZIG) build test
	@echo "Zig tests passed."

zig-clean:
	rm -rf $(ZIG_DIR)/zig-out $(ZIG_DIR)/.zig-cache

# ── Full pipeline ────────────────────────────────────────────────────────────
full-pipeline: zig-build outputs-dirs terrain settlements atmosphere audio history
	@echo "================================================================"
	@echo "  Full pipeline complete. Artifacts in outputs/"
	@echo "================================================================"

full-pipeline-mobile: full-pipeline mobile-package
	@echo "Mobile artifact set baked — pre-baked data only, no runtime compute."

# ── Mobile packaging ──────────────────────────────────────────────────────────
MOBILE_OUTPUTS_DIR ?= outputs
MOBILE_PACKAGE_DIR ?= outputs/mobile_package

mobile-package: outputs-dirs
	$(PYTHON) -m pipeline.mobile.package_mobile_artifacts \
	    $(MOBILE_OUTPUTS_DIR) $(MOBILE_PACKAGE_DIR)
	@echo "Mobile package written to $(MOBILE_PACKAGE_DIR)/"

# Convenience group targets
terrain: terrain-acquire terrain-hardness terrain-erode terrain-rivers terrain-export

settlements: settlements-voronoi settlements-graph

atmosphere: atmosphere-luts atmosphere-weather

audio: audio-irs audio-propagation audio-bioacoustics

history: history-graph history-validate

# ── Terrain stages ────────────────────────────────────────────────────────────
terrain-acquire: outputs-dirs
	$(PYTHON) -m pipeline.terrain.acquire_dem \
	    --bbox-west $(BBOX_WEST) \
	    --bbox-east $(BBOX_EAST) \
	    --bbox-south $(BBOX_SOUTH) \
	    --bbox-north $(BBOX_NORTH) \
	    --output-dir $(DEM_DIR)

terrain-hardness: outputs-dirs
	$(PYTHON) -m pipeline.terrain.build_hardness_map \
	    --dem-path $(DEM_DIR)/dem_utm36s.tif \
	    --geology-vector-path $(GEOLOGY_SHP) \
	    --output-path $(HARDNESS_PATH)

terrain-erode: outputs-dirs
	$(PYTHON) -m pipeline.terrain.erode_terrain \
	    --dem-path $(DEM_DIR)/dem_utm36s.tif \
	    --hardness-path $(HARDNESS_PATH) \
	    --output-path $(ERODED_PATH) \
	    --iterations $(EROSION_ITERS)

terrain-rivers: outputs-dirs
	$(PYTHON) -m pipeline.terrain.extract_rivers \
	    --eroded-dem-path $(ERODED_PATH) \
	    --output-shapefile-path $(RIVERS_SHP)

terrain-export: outputs-dirs
	$(PYTHON) -m pipeline.terrain.export_to_ue5 \
	    --eroded-dem-path $(ERODED_PATH) \
	    --weight-maps-dir outputs/weight_maps \
	    --output-dir $(UE5_OUTPUT_DIR) \
	    --tile-size 1009

# ── Settlement stages ─────────────────────────────────────────────────────────
settlements-voronoi: outputs-dirs
	$(PYTHON) -m pipeline.settlements.compute_voronoi \
	    --output-dir $(SETTLEMENTS_DIR) \
	    --lloyd-iterations $(LLOYD_ITERS)

settlements-graph: outputs-dirs
	$(PYTHON) -m pipeline.settlements.build_political_graph \
	    --settlements-dir $(SETTLEMENTS_DIR) \
	    --rivers-shp $(RIVERS_SHP) \
	    --output-dir $(SETTLEMENTS_DIR)

# ── Atmosphere stages ─────────────────────────────────────────────────────────
atmosphere-luts: outputs-dirs
	$(PYTHON) -m pipeline.atmosphere.compute_sky_luts \
	    --output-dir $(ATMOSPHERE_DIR)

atmosphere-weather: outputs-dirs
	$(PYTHON) -m pipeline.atmosphere.build_weather_tables \
	    --output-dir $(ATMOSPHERE_DIR)

# ── Audio stages ──────────────────────────────────────────────────────────────
audio-irs: outputs-dirs
	$(PYTHON) -m pipeline.audio.compute_acoustic_irs \
	    --output-dir $(AUDIO_DIR)

audio-propagation: outputs-dirs
	$(PYTHON) -m pipeline.audio.compute_propagation_tables \
	    --output-dir $(AUDIO_DIR)

audio-bioacoustics: outputs-dirs
	$(PYTHON) -m pipeline.audio.build_bioacoustic_library \
	    --output-dir $(AUDIO_DIR)

# ── History stages ────────────────────────────────────────────────────────────
history-graph: outputs-dirs
	$(PYTHON) -m pipeline.history.build_knowledge_graph \
	    --data-dir pipeline/history/data \
	    --output-path $(KG_PATH)

history-validate:
	$(PYTHON) pipeline/history/validate_content.py \
	    $(CONTENT_DIR) \
	    $(KG_PATH)

# ── Tests ─────────────────────────────────────────────────────────────────────
test: test-environment test-stubs test-compute zig-test
	@echo "All pipeline tests passed."

test-environment:
	$(PYTEST) pipeline/tests/test_environment.py -v

test-stubs:
	$(PYTEST) pipeline/tests/test_pipeline_stubs.py -v

test-compute:
	$(PYTEST) pipeline/tests/compute/ -v

test-coverage:
	$(PYTEST) pipeline/tests/ \
	    --cov=pipeline \
	    --cov-report=term-missing \
	    --cov-report=html:outputs/coverage_html \
	    -v

# ── Utilities ─────────────────────────────────────────────────────────────────
outputs-dirs:
	@mkdir -p outputs/dem outputs/weight_maps $(UE5_OUTPUT_DIR) \
	           $(SETTLEMENTS_DIR) $(ATMOSPHERE_DIR) $(AUDIO_DIR) cache

clean: zig-clean
	rm -rf outputs/ .pytest_cache pipeline/__pycache__ \
	       pipeline/**/__pycache__ pipeline/**/**/__pycache__
	find pipeline -name "*.pyc" -delete

help:
	@echo ""
	@echo "Mahlanya RPG — Offline Science Pipeline"
	@echo "========================================"
	@echo ""
	@echo "Full pipeline:"
	@echo "  make full-pipeline       Run all stages (terrain → history)"
	@echo ""
	@echo "Terrain stages:"
	@echo "  make terrain-acquire     Download Copernicus DEM GLO-10 tiles"
	@echo "  make terrain-hardness    Rasterise geological rock-hardness map"
	@echo "  make terrain-erode       Run full geomorphological erosion stack"
	@echo "  make terrain-rivers      Extract river network (D-infinity)"
	@echo "  make terrain-export      Export 1009×1009 UE5 heightmap tiles"
	@echo ""
	@echo "Settlement stages:"
	@echo "  make settlements-voronoi Compute Umuti Centroidal Voronoi layouts"
	@echo "  make settlements-graph   Build inter-settlement political graph"
	@echo ""
	@echo "Atmosphere stages:"
	@echo "  make atmosphere-luts     Compute Hosek-Wilkie spectral sky LUTs"
	@echo "  make atmosphere-weather  Build SAWS seasonal weather tables"
	@echo ""
	@echo "Audio stages:"
	@echo "  make audio-irs           Compute acoustic impulse responses"
	@echo "  make audio-propagation   Build atmospheric propagation tables"
	@echo "  make audio-bioacoustics  Build Eswatini bioacoustic library"
	@echo ""
	@echo "History stages:"
	@echo "  make history-graph       Build Swazi historical knowledge graph"
	@echo "  make history-validate    Validate all dialogue content (CI gate)"
	@echo ""
	@echo "Zig compute library:"
	@echo "  make zig-build           Compile libmahlanya_compute.so (SIMD kernels)"
	@echo "  make zig-test            Run Zig unit tests"
	@echo "  make zig-clean           Remove Zig build artifacts"
	@echo ""
	@echo "Tests:"
	@echo "  make test                Run all tests (environment + stubs + compute + Zig)"
	@echo "  make test-environment    Check required library versions"
	@echo "  make test-stubs          Verify pipeline API signatures"
	@echo "  make test-compute        Run Zig kernel tests via Python bridge"
	@echo "  make test-coverage       Full coverage report"
	@echo ""
	@echo "Mobile packaging:"
	@echo "  make mobile-package      Bundle pre-baked artifacts for mobile cook"
	@echo "  make full-pipeline-mobile Run full pipeline + package mobile artifacts"
	@echo ""
	@echo "Utilities:"
	@echo "  make clean               Remove outputs and caches"
	@echo ""
	@echo "Override variables (e.g. make terrain-erode EROSION_ITERS=100):"
	@echo "  EROSION_ITERS=$(EROSION_ITERS)  LLOYD_ITERS=$(LLOYD_ITERS)"
	@echo "  BBOX_WEST=$(BBOX_WEST)  BBOX_EAST=$(BBOX_EAST)"
	@echo "  BBOX_SOUTH=$(BBOX_SOUTH)  BBOX_NORTH=$(BBOX_NORTH)"
	@echo ""
