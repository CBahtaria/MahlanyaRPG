"""
Stub importability and API-signature tests.

Every pipeline module must be importable and expose the exact function/class
signatures specified in the plan. These tests pin the public API so later
full-implementation PRs cannot accidentally change the contract.

All stubs are expected to raise NotImplementedError when called — that is
intentional and confirmed here.
"""

import inspect
import pathlib
import pytest


# ── Terrain ────────────────────────────────────────────────────────────────────

def test_acquire_dem_importable():
    from pipeline.terrain import acquire_dem
    assert hasattr(acquire_dem, "acquire_dem")


def test_acquire_dem_signature():
    from pipeline.terrain.acquire_dem import acquire_dem
    sig = inspect.signature(acquire_dem)
    params = list(sig.parameters)
    assert "bbox_west" in params
    assert "bbox_east" in params
    assert "bbox_south" in params
    assert "bbox_north" in params
    assert "output_dir" in params


def test_acquire_dem_raises_not_implemented():
    from pipeline.terrain.acquire_dem import acquire_dem
    with pytest.raises(NotImplementedError):
        acquire_dem(30.79, 32.14, -27.32, -25.72, pathlib.Path("/tmp"))


def test_build_hardness_map_importable():
    from pipeline.terrain import build_hardness_map
    assert hasattr(build_hardness_map, "build_hardness_map")


def test_build_hardness_map_signature():
    from pipeline.terrain.build_hardness_map import build_hardness_map
    sig = inspect.signature(build_hardness_map)
    params = list(sig.parameters)
    assert "dem_path" in params
    assert "geology_vector_path" in params
    assert "output_path" in params


def test_build_hardness_map_raises_not_implemented():
    from pipeline.terrain.build_hardness_map import build_hardness_map
    with pytest.raises(NotImplementedError):
        build_hardness_map(pathlib.Path("/tmp/dem.tif"),
                           pathlib.Path("/tmp/geo.shp"),
                           pathlib.Path("/tmp/out.tif"))


def test_erode_terrain_importable():
    from pipeline.terrain import erode_terrain
    assert hasattr(erode_terrain, "erode_terrain")


def test_erode_terrain_signature():
    from pipeline.terrain.erode_terrain import erode_terrain
    sig = inspect.signature(erode_terrain)
    params = list(sig.parameters)
    assert "dem_path" in params
    assert "hardness_path" in params
    assert "output_path" in params
    assert "iterations" in params
    assert "use_gpu" in params


def test_extract_rivers_importable():
    from pipeline.terrain import extract_rivers
    assert hasattr(extract_rivers, "extract_rivers")


def test_extract_rivers_signature():
    from pipeline.terrain.extract_rivers import extract_rivers
    sig = inspect.signature(extract_rivers)
    params = list(sig.parameters)
    assert "eroded_dem_path" in params
    assert "output_shapefile_path" in params
    assert "threshold" in params


def test_extract_rivers_raises_not_implemented():
    from pipeline.terrain.extract_rivers import extract_rivers
    with pytest.raises(NotImplementedError):
        extract_rivers(pathlib.Path("/tmp/dem.tif"), pathlib.Path("/tmp/rivers.shp"))


def test_export_to_ue5_importable():
    from pipeline.terrain import export_to_ue5
    assert hasattr(export_to_ue5, "export_to_ue5")


def test_export_to_ue5_signature():
    from pipeline.terrain.export_to_ue5 import export_to_ue5
    sig = inspect.signature(export_to_ue5)
    params = list(sig.parameters)
    assert "eroded_dem_path" in params
    assert "weight_maps_dir" in params
    assert "output_dir" in params
    assert "tile_size" in params


# ── Settlements ────────────────────────────────────────────────────────────────

def test_compute_voronoi_importable():
    from pipeline.settlements import compute_voronoi
    assert hasattr(compute_voronoi, "SwaziSettlementGenerator")


def test_compute_voronoi_class_has_taboo_arc():
    from pipeline.settlements.compute_voronoi import SwaziSettlementGenerator
    assert hasattr(SwaziSettlementGenerator, "TABOO_ARC_DEG")
    assert SwaziSettlementGenerator.TABOO_ARC_DEG == (240, 300)


def test_compute_voronoi_generate_signature():
    from pipeline.settlements.compute_voronoi import SwaziSettlementGenerator
    sig = inspect.signature(SwaziSettlementGenerator.generate)
    params = list(sig.parameters)
    assert "n_wives" in params
    assert "n_sons" in params
    assert "n_dependents" in params
    assert "cattle_count" in params
    assert "terrain_gradient" in params


def test_build_political_graph_importable():
    from pipeline.settlements import build_political_graph
    assert hasattr(build_political_graph, "build_political_graph")


# ── Atmosphere ─────────────────────────────────────────────────────────────────

def test_compute_sky_luts_importable():
    from pipeline.atmosphere import compute_sky_luts
    assert hasattr(compute_sky_luts, "compute_sky_luts")


def test_compute_sky_luts_exports():
    from pipeline.atmosphere.compute_sky_luts import (
        compute_sky_luts,
        export_lunar_calendar,
        export_star_field,
    )
    for fn in (compute_sky_luts, export_lunar_calendar, export_star_field):
        assert callable(fn)


def test_build_weather_tables_importable():
    from pipeline.atmosphere import build_weather_tables
    assert hasattr(build_weather_tables, "build_weather_tables")


# ── Audio ──────────────────────────────────────────────────────────────────────

def test_compute_acoustic_irs_importable():
    from pipeline.audio import compute_acoustic_irs
    assert hasattr(compute_acoustic_irs, "MATERIAL_ABSORPTION")
    assert hasattr(compute_acoustic_irs, "ENVIRONMENT_ARCHETYPES")


def test_material_absorption_completeness():
    from pipeline.audio.compute_acoustic_irs import MATERIAL_ABSORPTION
    required = {"granite", "thatch_grass", "clay_earth", "dry_grass",
                "water_surface", "open_sky"}
    assert required.issubset(set(MATERIAL_ABSORPTION.keys()))
    for mat, bands in MATERIAL_ABSORPTION.items():
        assert len(bands) == 8, f"{mat} must have 8 octave-band coefficients"


def test_environment_archetypes_count():
    from pipeline.audio.compute_acoustic_irs import ENVIRONMENT_ARCHETYPES
    assert len(ENVIRONMENT_ARCHETYPES) == 6


def test_acoustic_ir_functions_exist():
    from pipeline.audio.compute_acoustic_irs import (
        compute_ir_image_source,
        compute_ir_monte_carlo,
    )
    for fn in (compute_ir_image_source, compute_ir_monte_carlo):
        assert callable(fn)


def test_compute_propagation_tables_importable():
    from pipeline.audio import compute_propagation_tables
    assert hasattr(compute_propagation_tables, "compute_propagation_tables")


def test_build_bioacoustic_library_importable():
    from pipeline.audio import build_bioacoustic_library
    assert hasattr(build_bioacoustic_library, "build_bioacoustic_library")


# ── History ────────────────────────────────────────────────────────────────────

def test_build_knowledge_graph_importable():
    from pipeline.history import build_knowledge_graph
    assert hasattr(build_knowledge_graph, "build_knowledge_graph")


def test_validate_content_importable():
    from pipeline.history import validate_content
    assert hasattr(validate_content, "validate_content")
    assert hasattr(validate_content, "SwaziHistoricalValidator")


def test_validate_content_signature():
    from pipeline.history.validate_content import validate_content
    sig = inspect.signature(validate_content)
    params = list(sig.parameters)
    assert "content_dir" in params
    assert "knowledge_graph_path" in params


def test_swazi_historical_validator_signature():
    from pipeline.history.validate_content import SwaziHistoricalValidator
    sig = inspect.signature(SwaziHistoricalValidator.__init__)
    params = list(sig.parameters)
    assert "knowledge_graph_path" in params


# ── History data files exist ───────────────────────────────────────────────────

@pytest.mark.parametrize("filename", [
    "persons.json",
    "places.json",
    "battles.json",
    "events.json",
    "relations.json",
    "material_culture.json",
])
def test_history_data_file_exists(history_data_dir, filename):
    path = history_data_dir / filename
    assert path.exists(), f"Missing history data file: {path}"


# ── Validate content — functional tests ───────────────────────────────────────

def test_validate_content_no_graph_returns_empty(tmp_path):
    from pipeline.history.validate_content import validate_content
    violations = validate_content(tmp_path, tmp_path / "nonexistent.ndjson")
    assert violations == []


def test_validate_content_no_content_dir_returns_empty(sample_knowledge_graph, tmp_path):
    from pipeline.history.validate_content import validate_content
    violations = validate_content(tmp_path / "nonexistent", sample_knowledge_graph)
    assert violations == []


def test_validate_scene_valid(sample_knowledge_graph, sample_scene_valid):
    from pipeline.history.validate_content import SwaziHistoricalValidator
    validator = SwaziHistoricalValidator(sample_knowledge_graph)
    violations = validator.validate_scene(sample_scene_valid)
    assert violations == [], f"Unexpected violations: {violations}"


def test_validate_scene_chronological_violation(sample_knowledge_graph,
                                                 sample_scene_chrono_violation):
    from pipeline.history.validate_content import SwaziHistoricalValidator
    validator = SwaziHistoricalValidator(sample_knowledge_graph)
    violations = validator.validate_scene(sample_scene_chrono_violation)
    assert len(violations) == 1
    assert "SW001" in violations[0]["entity"]
    assert "1795" in violations[0]["reason"] or "bounds" in violations[0]["reason"]


def test_validate_scene_item_anachronism(sample_knowledge_graph,
                                          sample_scene_item_violation):
    from pipeline.history.validate_content import SwaziHistoricalValidator
    validator = SwaziHistoricalValidator(sample_knowledge_graph)
    violations = validator.validate_scene(sample_scene_item_violation)
    assert len(violations) == 1
    assert "GUN001" in violations[0]["entity"]


def test_validate_scene_unknown_entity(sample_knowledge_graph):
    from pipeline.history.validate_content import SwaziHistoricalValidator
    validator = SwaziHistoricalValidator(sample_knowledge_graph)
    violations = validator.validate_scene({
        "era_year": 1820,
        "entities": ["UNKNOWN_PERSON"],
        "equipped_items": [],
    })
    assert len(violations) == 1
    assert "not found" in violations[0]["reason"]
