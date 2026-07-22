"""Tests for eswatini_faults.geojson data integrity."""
import json
import pathlib
import pytest

FAULT_DATA_PATH = pathlib.Path(__file__).parents[3] / "Plugins" / "EswatiniGeophysics" / "Data" / "eswatini_faults.geojson"

ESWATINI_LON_MIN = 30.79
ESWATINI_LON_MAX = 32.14
ESWATINI_LAT_MIN = -27.32
ESWATINI_LAT_MAX = -25.72
MIN_FAULT_POINT_COUNT = 2

def _load_features():
    with open(FAULT_DATA_PATH) as f:
        return json.load(f)["features"]

def test_fault_data_file_exists():
    assert FAULT_DATA_PATH.exists(), f"Fault data not found at {FAULT_DATA_PATH}"

def test_fault_data_is_valid_geojson():
    data = _load_features()
    assert isinstance(data, list)
    assert len(data) > 0

def test_fault_features_have_linestring_geometry():
    for feature in _load_features():
        assert feature["geometry"]["type"] == "LineString"

def test_fault_coordinates_within_eswatini_bbox():
    for feature in _load_features():
        for lon, lat in feature["geometry"]["coordinates"]:
            assert ESWATINI_LON_MIN <= lon <= ESWATINI_LON_MAX, f"Longitude {lon} outside Eswatini bounds"
            assert ESWATINI_LAT_MIN <= lat <= ESWATINI_LAT_MAX, f"Latitude {lat} outside Eswatini bounds"

def test_fault_lines_have_minimum_point_count():
    for feature in _load_features():
        coords = feature["geometry"]["coordinates"]
        assert len(coords) >= MIN_FAULT_POINT_COUNT, f"Fault line has fewer than {MIN_FAULT_POINT_COUNT} points"

def test_fault_features_have_name_property():
    for feature in _load_features():
        assert "name" in feature["properties"]
        assert isinstance(feature["properties"]["name"], str)
        assert len(feature["properties"]["name"]) > 0
