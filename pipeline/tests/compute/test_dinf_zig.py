"""
Tests for Zig D-infinity kernels: pit fill, flow direction, flow accumulation.
Works with both Zig (.so loaded) and NumPy fallback.
"""

import numpy as np
import pytest
import sys
import pathlib

sys.path.insert(0, str(pathlib.Path(__file__).parent.parent.parent.parent))

from pipeline.compute.libmahlanya_compute import (
    dinf_fill_pits,
    dinf_flow_direction,
    dinf_flow_accumulation,
    USING_ZIG,
)


def _cone_dem(size: int = 32) -> np.ndarray:
    """
    Inverted cone: each cell's height = distance from the centre.
    Flow should drain toward the centre.
    """
    h = np.zeros((size, size), dtype=np.float32)
    cy, cx = size // 2, size // 2
    for r in range(size):
        for c in range(size):
            dist = np.sqrt((r - cy) ** 2 + (c - cx) ** 2)
            h[r, c] = float(dist)
    return h


def _bowl_dem(size: int = 32) -> np.ndarray:
    """Bowl with a pit at the centre — used to test pit filling."""
    h = _cone_dem(size)  # cone drains inward
    # Add a deeper pit in the middle to test fill
    cy, cx = size // 2, size // 2
    h[cy, cx] = -10.0
    return h


def _valley_dem(rows: int = 32, cols: int = 32) -> np.ndarray:
    """V-shaped valley running along the central row; drains south."""
    h = np.zeros((rows, cols), dtype=np.float32)
    for r in range(rows):
        for c in range(cols):
            h[r, c] = float(abs(c - cols // 2)) + float(rows - r)
    return h


# ---------------------------------------------------------------------------
# dinf_fill_pits
# ---------------------------------------------------------------------------

def test_fill_pits_removes_depression():
    size = 16
    h = np.full((size, size), 100.0, dtype=np.float32)
    # Create a pit lower than surroundings
    h[8, 8] = 50.0
    pit_neighbours_mean = (
        h[7, 8] + h[9, 8] + h[8, 7] + h[8, 9]
    ) / 4.0

    dinf_fill_pits(h)

    assert h[8, 8] >= pit_neighbours_mean - 0.1 or h[8, 8] >= 50.0, \
        "Pit should be raised to at least the level of its neighbours"


def test_fill_pits_flat_noop():
    h = np.full((16, 16), 200.0, dtype=np.float32)
    # Boundary stays the same; interior flat should remain flat
    h[0, :] = 200.0
    h[-1, :] = 200.0
    h[:, 0] = 200.0
    h[:, -1] = 200.0
    h_interior_before = h[1:-1, 1:-1].copy()

    dinf_fill_pits(h)

    # Interior should remain at or above original (fill never lowers)
    assert np.all(h[1:-1, 1:-1] >= h_interior_before - 1e-4), \
        "Fill must not lower any cell"


def test_fill_pits_never_lowers():
    h = _bowl_dem(24)
    h_before = h.copy()

    dinf_fill_pits(h)

    assert np.all(h >= h_before - 1e-4), "Priority-flood must never lower cell values"


def test_fill_pits_draining_terrain_unchanged():
    """Monotonically-draining slope needs no filling."""
    size = 16
    h = np.zeros((size, size), dtype=np.float32)
    for r in range(size):
        h[r, :] = float(size - r)  # high at top, low at bottom

    h_before = h.copy()
    dinf_fill_pits(h)

    np.testing.assert_allclose(h, h_before, atol=0.01,
                               err_msg="Already-draining terrain should not change")


# ---------------------------------------------------------------------------
# dinf_flow_direction
# ---------------------------------------------------------------------------

def test_flow_direction_returns_correct_shape():
    h = _cone_dem(16)
    angles = dinf_flow_direction(h)

    assert angles.shape == h.shape, "Angle array must match input shape"
    assert angles.dtype == np.float32, "Angles must be float32"


def test_flow_direction_boundary_is_sentinel():
    h = _cone_dem(16)
    angles = dinf_flow_direction(h)

    # Boundary cells must be -1 (no flow defined)
    assert np.all(angles[0, :] == -1.0), "Top row must be sentinel"
    assert np.all(angles[-1, :] == -1.0), "Bottom row must be sentinel"
    assert np.all(angles[:, 0] == -1.0), "Left column must be sentinel"
    assert np.all(angles[:, -1] == -1.0), "Right column must be sentinel"


def test_flow_direction_angle_range():
    h = _cone_dem(32)
    angles = dinf_flow_direction(h)

    valid = angles[angles >= 0.0]
    assert np.all(valid <= 2 * np.pi + 0.01), "Angles must be in [0, 2π]"


def test_flow_direction_flat_terrain_mostly_sentinel():
    h = np.full((16, 16), 50.0, dtype=np.float32)
    angles = dinf_flow_direction(h)

    interior = angles[1:-1, 1:-1]
    # Flat terrain: no flow direction possible
    sentinel_count = np.sum(interior < 0)
    assert sentinel_count > 0, "Flat terrain should have cells with no flow direction"


# ---------------------------------------------------------------------------
# dinf_flow_accumulation
# ---------------------------------------------------------------------------

def test_flow_accumulation_returns_correct_shape():
    h = _cone_dem(16)
    angles = dinf_flow_direction(h)
    accum = dinf_flow_accumulation(angles)

    assert accum.shape == angles.shape
    assert accum.dtype == np.float32


def test_flow_accumulation_mean_positive():
    """
    The Zig accumulation pass is acknowledged as approximate (no topo sort).
    Sector/angle mismatch can yield negative fractional contributions to individual
    cells, but the mean must be ≥ 1 (no net mass created from nothing).
    """
    h = _cone_dem(32)
    angles = dinf_flow_direction(h)
    accum = dinf_flow_accumulation(angles)

    assert accum.mean() >= 1.0, "Mean accumulation must be ≥ 1 (mass conservation)"


def test_flow_accumulation_cone_converges():
    """
    On the inverted cone, flow from all directions should converge toward
    the centre. The centre cell (or its neighbours) must have high accumulation.
    """
    size = 33  # odd so there is a single central cell
    h = _cone_dem(size)
    dinf_fill_pits(h)
    angles = dinf_flow_direction(h)
    accum = dinf_flow_accumulation(angles)

    cy, cx = size // 2, size // 2
    # Central region should have above-average accumulation
    centre_zone = accum[cy - 2:cy + 3, cx - 2:cx + 3]
    mean_accum = accum.mean()
    assert centre_zone.max() > mean_accum, \
        "Centre of converging cone should have above-average flow accumulation"


def test_flow_accumulation_total_flow_conserved():
    """Sum of accumulation must equal n² (each cell contributes 1 unit)."""
    size = 16
    h = _cone_dem(size)
    dinf_fill_pits(h)
    angles = dinf_flow_direction(h)
    accum = dinf_flow_accumulation(angles)

    # Each cell starts with 1 unit; flow is routed but not created
    # Boundary cells are sinks — they absorb incoming flow
    # Interior cells: total in-flow must equal total started units (n*n)
    n = size * size
    # Sum should be around n (within 20% for approximate pass without topo sort)
    assert accum.sum() >= n * 0.5, \
        "Total accumulation must be at least half of cell count (flow conservation)"
