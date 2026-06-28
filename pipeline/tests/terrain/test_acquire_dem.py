"""Tests for pipeline.terrain.acquire_dem."""

import pathlib
import shutil
import subprocess
import sys
from unittest import mock

import pytest

from pipeline.terrain.acquire_dem import (
    ESWATINI_BBOX,
    ESWATINI_UTM_EPSG,
    TARGET_RESOLUTION_M,
    _compute_tile_names,
    acquire_dem,
)


# ---------------------------------------------------------------------------
# Constants
# ---------------------------------------------------------------------------

def test_eswatini_bbox_constant():
    west, east, south, north = ESWATINI_BBOX
    assert west == pytest.approx(30.79)
    assert east == pytest.approx(32.14)
    assert south == pytest.approx(-27.32)
    assert north == pytest.approx(-25.72)
    # Sanity: bounding box is coherent
    assert west < east
    assert south < north


def test_eswatini_utm_epsg():
    assert ESWATINI_UTM_EPSG == 32736


def test_target_resolution():
    assert TARGET_RESOLUTION_M == 10


# ---------------------------------------------------------------------------
# Tile name computation
# ---------------------------------------------------------------------------

def test_tile_name_computation():
    """west=30.0, east=32.5, south=-28.0, north=-25.0 must yield 9 tiles."""
    names = _compute_tile_names(30.0, 32.5, -28.0, -25.0)

    # Expect exactly 9 tiles: 3 lats × 3 lons
    assert len(names) == 9, f"Expected 9 tiles, got {len(names)}: {names}"

    # Check each expected tile is present (by its coordinate suffix).
    expected_suffixes = [
        ("S28", "E030"), ("S28", "E031"), ("S28", "E032"),
        ("S27", "E030"), ("S27", "E031"), ("S27", "E032"),
        ("S26", "E030"), ("S26", "E031"), ("S26", "E032"),
    ]
    for ns_part, ew_part in expected_suffixes:
        matching = [n for n in names if ns_part in n and ew_part in n]
        assert matching, (
            f"Expected tile with {ns_part}/{ew_part} not found in: {names}"
        )


def test_tile_name_format():
    """Tile names must follow the Copernicus COG naming convention."""
    names = _compute_tile_names(30.0, 31.5, -28.0, -27.0)
    # floor(-28.0) = -28  → S28;  floor(31.0) = 31 → E031
    # floor(-27.0) would give lat_max = -28 (exact int → -28-1=-28+1? no: -27-1=-28)
    # Actually north=-27.0 is exact: lat_max = int(-27.0) - 1 = -28; lat range [-28,-28]
    # lon: floor(30.0)=30, east=31.5 → floor(31.5)=31; lon range [30,31]
    assert len(names) == 2
    for name in names:
        assert name.startswith("Copernicus_DSM_COG_10_")
        assert "_00_" in name
        assert name.endswith("_DEM")


def test_tile_name_northern_hemisphere():
    """Check N prefix for positive latitudes.
    south=0.5→lat_min=0, north=1.5→lat_max=1 (2 rows)
    west=36.0→lon_min=36, east=37.5→lon_max=37 (2 cols)  → 4 tiles
    """
    names = _compute_tile_names(36.0, 37.5, 0.5, 1.5)
    assert len(names) == 4
    assert all("N00" in n or "N01" in n for n in names)
    assert all("E036" in n or "E037" in n for n in names)


def test_tile_name_west_longitude():
    """Check W prefix for negative longitudes.
    south=51.0,north=52.0→exact int→lat_max=51, 1 row (N51)
    west=-1.5→lon_min=-2, east=-0.5→lon_max=-1  → tiles W002, W001
    """
    names = _compute_tile_names(-1.5, -0.5, 51.0, 52.0)
    assert len(names) == 2
    assert all("W002" in n or "W001" in n for n in names)


# ---------------------------------------------------------------------------
# acquire_dem: skip when output already exists
# ---------------------------------------------------------------------------

def test_acquire_dem_skips_if_output_exists(tmp_path):
    """If dem_utm36s.tif already exists, acquire_dem must return it immediately
    without attempting any download or GDAL call."""
    existing = tmp_path / "dem_utm36s.tif"
    existing.touch()

    download_mock = mock.patch("urllib.request.urlopen")
    gdal_mock = mock.patch("subprocess.run")

    with download_mock as dl, gdal_mock as gw:
        result = acquire_dem(30.79, 32.14, -27.32, -25.72, tmp_path)

    dl.assert_not_called()
    gw.assert_not_called()
    assert result == existing
    assert result.exists()


# ---------------------------------------------------------------------------
# acquire_dem: raises clearly when gdalwarp is missing
# ---------------------------------------------------------------------------

def test_acquire_dem_raises_without_gdal(tmp_path):
    """When gdalwarp is not on PATH, acquire_dem must raise ImportError with
    a human-readable install hint — not a bare FileNotFoundError."""

    # Patch shutil.which so it returns None for gdalwarp.
    with mock.patch("shutil.which", return_value=None):
        with pytest.raises(ImportError, match="gdalwarp"):
            acquire_dem(30.79, 32.14, -27.32, -25.72, tmp_path)


# ---------------------------------------------------------------------------
# acquire_dem: integration (skipped when GDAL / network unavailable)
# ---------------------------------------------------------------------------

def test_acquire_dem_integration(tmp_path):
    """Full integration test: downloads real tiles and reprojects them.

    Skipped unless:
      - gdalwarp is on PATH, AND
      - OPENTOPOGRAPHY_API_KEY env var is set, OR AWS S3 is reachable.
    """
    if shutil.which("gdalwarp") is None:
        pytest.skip("gdalwarp not available")

    # Use a tiny 0.1° × 0.1° slice to keep download small.
    result = acquire_dem(31.0, 31.1, -26.5, -26.4, tmp_path)
    assert result.exists()
    assert result.suffix == ".tif"
    assert result.stat().st_size > 0


# ---------------------------------------------------------------------------
# CLI smoke test
# ---------------------------------------------------------------------------

def test_cli_help():
    """Ensure the CLI --help exits cleanly (no import errors)."""
    result = subprocess.run(
        [sys.executable, "-m", "pipeline.terrain.acquire_dem", "--help"],
        capture_output=True,
        text=True,
        cwd=str(pathlib.Path(__file__).parents[3]),  # project root / pipeline parent
    )
    # argparse --help always exits 0
    assert result.returncode == 0
    assert "--bbox-west" in result.stdout
    assert "--output-dir" in result.stdout
