"""
Tests for Zig erosion kernels via Python ctypes bridge.
Works with both Zig (.so loaded) and NumPy fallback.
"""

import numpy as np
import pytest
import sys
import pathlib

sys.path.insert(0, str(pathlib.Path(__file__).parent.parent.parent.parent))

from pipeline.compute.libmahlanya_compute import (
    thermal_erosion_pass,
    fluvial_erosion_pass,
    aeolian_erosion_pass,
    mass_wasting_pass,
    USING_ZIG,
)


def _flat_dem(rows: int, cols: int, base: float = 100.0) -> np.ndarray:
    return np.full((rows, cols), base, dtype=np.float32)


def _slope_dem(rows: int, cols: int) -> np.ndarray:
    """Linear slope: high on the left, low on the right."""
    h = np.zeros((rows, cols), dtype=np.float32)
    for c in range(cols):
        h[:, c] = float(cols - c)
    return h


def _uniform_hardness(rows: int, cols: int, val: float = 0.5) -> np.ndarray:
    return np.full((rows, cols), val, dtype=np.float32)


# ---------------------------------------------------------------------------
# thermal_erosion_pass
# ---------------------------------------------------------------------------

def test_thermal_reduces_tall_spike():
    h = _flat_dem(32, 32, 100.0)
    k = _uniform_hardness(32, 32, 0.1)
    h[16, 16] = 200.0  # artificially tall spike

    thermal_erosion_pass(h, k, talus_angle_deg=33.0, cell_size_m=10.0)

    assert h[16, 16] < 200.0, "Tall spike should erode"


def test_thermal_flat_terrain_unchanged():
    h = _flat_dem(32, 32, 50.0)
    k = _uniform_hardness(32, 32, 0.5)
    h_orig = h.copy()

    thermal_erosion_pass(h, k, talus_angle_deg=33.0, cell_size_m=10.0)

    np.testing.assert_allclose(h, h_orig, atol=1e-3,
                               err_msg="Flat terrain should not erode significantly")


def test_thermal_hard_rock_erodes_less():
    h_soft = _flat_dem(32, 32, 100.0)
    h_soft[16, 16] = 200.0
    k_soft = _uniform_hardness(32, 32, 0.01)

    h_hard = _flat_dem(32, 32, 100.0)
    h_hard[16, 16] = 200.0
    k_hard = _uniform_hardness(32, 32, 0.95)

    thermal_erosion_pass(h_soft, k_soft)
    thermal_erosion_pass(h_hard, k_hard)

    erosion_soft = 200.0 - h_soft[16, 16]
    erosion_hard = 200.0 - h_hard[16, 16]
    assert erosion_soft >= erosion_hard, "Soft rock erodes at least as much as hard rock"


def test_thermal_preserves_total_mass_approximately():
    h = _flat_dem(32, 32, 100.0)
    h[10, 10] = 300.0
    k = _uniform_hardness(32, 32, 0.3)
    total_before = h.sum()

    thermal_erosion_pass(h, k, talus_angle_deg=25.0, cell_size_m=10.0)

    # Mass is redistributed, not created or destroyed (allow 1% tolerance)
    np.testing.assert_allclose(h.sum(), total_before, rtol=0.01,
                               err_msg="Thermal erosion must roughly conserve mass")


# ---------------------------------------------------------------------------
# fluvial_erosion_pass
# ---------------------------------------------------------------------------

def test_fluvial_erodes_steep_slopes():
    h = _slope_dem(32, 32)
    water = np.full((32, 32), 0.5, dtype=np.float32)
    sediment = np.zeros((32, 32), dtype=np.float32)
    k = _uniform_hardness(32, 32, 0.1)
    h_orig = h.copy()

    fluvial_erosion_pass(h, water, sediment, k, gravity=9.81, erosion_rate=0.05, deposition_rate=0.1)

    changed = np.abs(h - h_orig).sum()
    assert changed > 0, "Fluvial erosion should change the heightmap"


def test_fluvial_sediment_conservation():
    h = _slope_dem(32, 32)
    water = np.full((32, 32), 0.5, dtype=np.float32)
    sediment = np.zeros((32, 32), dtype=np.float32)
    k = _uniform_hardness(32, 32, 0.1)

    total_before = h.sum() + sediment.sum()
    fluvial_erosion_pass(h, water, sediment, k)
    total_after = h.sum() + sediment.sum()

    np.testing.assert_allclose(total_after, total_before, rtol=0.05,
                               err_msg="Fluvial pass must roughly conserve height+sediment mass")


def test_fluvial_hard_rock_less_erosion():
    h_soft = _slope_dem(32, 32)
    h_hard = h_soft.copy()
    water = np.full((32, 32), 0.5, dtype=np.float32)
    sed_soft = np.zeros((32, 32), dtype=np.float32)
    sed_hard = np.zeros((32, 32), dtype=np.float32)

    k_soft = _uniform_hardness(32, 32, 0.01)
    k_hard = _uniform_hardness(32, 32, 0.95)

    fluvial_erosion_pass(h_soft, water.copy(), sed_soft, k_soft)
    fluvial_erosion_pass(h_hard, water.copy(), sed_hard, k_hard)

    eroded_soft = np.abs(h_soft - _slope_dem(32, 32)).sum()
    eroded_hard = np.abs(h_hard - _slope_dem(32, 32)).sum()
    assert eroded_soft >= eroded_hard, "Soft rock erodes more than hard rock"


# ---------------------------------------------------------------------------
# aeolian_erosion_pass
# ---------------------------------------------------------------------------

def test_aeolian_modifies_terrain():
    # Column ridge at col 16; east wind (0°) shifts columns → diff is non-zero at col 17
    h = _flat_dem(32, 32, 50.0)
    h[:, 16] += 10.0  # vertical ridge — perpendicular to east wind
    k = _uniform_hardness(32, 32, 0.1)
    h_orig = h.copy()

    aeolian_erosion_pass(h, k, wind_deg=0.0, drift_rate=0.01)

    assert not np.allclose(h, h_orig, atol=1e-4), "Aeolian pass should modify terrain"


def test_aeolian_zero_drift_minimal_change():
    h = _flat_dem(32, 32, 50.0)
    h[16, 16] = 80.0
    k = _uniform_hardness(32, 32, 0.5)
    h_orig = h.copy()

    aeolian_erosion_pass(h, k, wind_deg=0.0, drift_rate=0.0)

    np.testing.assert_allclose(h, h_orig, atol=1e-3,
                               err_msg="Zero drift rate should not change terrain")


def test_aeolian_does_not_create_mass():
    h = _flat_dem(32, 32, 50.0)
    h[5:10, 5:10] = 100.0
    k = _uniform_hardness(32, 32, 0.2)
    total_before = h.sum()

    aeolian_erosion_pass(h, k, wind_deg=45.0, drift_rate=0.005)

    # Mass may be lost at borders but not created; allow 1% for boundary effects
    assert h.sum() <= total_before * 1.01, "Aeolian must not create mass"


# ---------------------------------------------------------------------------
# mass_wasting_pass
# ---------------------------------------------------------------------------

def test_mass_wasting_reduces_extreme_slopes():
    h = _flat_dem(32, 32, 0.0)
    # Create a cliff: half the grid is 500m higher
    h[:, 16:] = 500.0
    k = _uniform_hardness(32, 32, 0.1)
    h_orig = h.copy()

    mass_wasting_pass(h, k, critical_slope_deg=38.0, seed=42)

    # The cliff edge should have been modified
    changed = np.abs(h - h_orig).sum()
    assert changed > 0, "Extreme slope should trigger mass wasting"


def test_mass_wasting_stable_slope_unchanged():
    h = _flat_dem(32, 32, 100.0)
    # Very gentle slope (1m over 32 cells → ~0.002 rad << 38°)
    for c in range(32):
        h[:, c] = 100.0 + c * 0.1
    k = _uniform_hardness(32, 32, 0.5)
    h_orig = h.copy()

    mass_wasting_pass(h, k, critical_slope_deg=38.0, seed=42)

    # Gentle slopes should not fail
    np.testing.assert_allclose(h, h_orig, atol=0.5,
                               err_msg="Stable gentle slopes should not mass-waste")


def test_mass_wasting_seeded_deterministic():
    h1 = _flat_dem(32, 32, 0.0)
    h1[:, 16:] = 500.0
    k = _uniform_hardness(32, 32, 0.1)
    h2 = h1.copy()

    mass_wasting_pass(h1, k, seed=99)
    mass_wasting_pass(h2, k, seed=99)

    np.testing.assert_array_equal(h1, h2, err_msg="Same seed must produce identical output")
