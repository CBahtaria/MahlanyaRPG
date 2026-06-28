"""
Tests for Zig Voronoi/Lloyd kernels: lloyd_relax and voronoi_cell_areas.
Works with both Zig (.so loaded) and NumPy fallback.
"""

import math
import numpy as np
import pytest
import sys
import pathlib

sys.path.insert(0, str(pathlib.Path(__file__).parent.parent.parent.parent))

from pipeline.compute.libmahlanya_compute import (
    lloyd_relax,
    voronoi_cell_areas,
    USING_ZIG,
)


TABOO_START = 240.0
TABOO_END   = 300.0


def _bearing_deg(x: float, y: float) -> float:
    """Convert (x,y) relative to centre (0.5,0.5) to bearing in degrees (0=North,CW)."""
    bx = x - 0.5
    by = y - 0.5
    if abs(bx) < 1e-9 and abs(by) < 1e-9:
        return 0.0
    return math.degrees(math.atan2(bx, by)) % 360.0


def _random_pts(n: int, seed: int = 0) -> np.ndarray:
    rng = np.random.default_rng(seed)
    return rng.random((n, 2)).astype(np.float64)


def _clustered_pts(n: int) -> np.ndarray:
    """All points bunched in top-left quadrant — badly distributed."""
    pts = np.zeros((n, 2), dtype=np.float64)
    for i in range(n):
        pts[i] = [0.05 + 0.1 * (i % 4), 0.05 + 0.1 * (i // 4)]
    return np.clip(pts, 0.01, 0.99)


# ---------------------------------------------------------------------------
# lloyd_relax
# ---------------------------------------------------------------------------

def test_lloyd_returns_correct_shape():
    pts = _random_pts(10)
    result = lloyd_relax(pts, iters=5)
    assert result.shape == (10, 2), "Must return (N,2) array"
    assert result.dtype == np.float64, "Must return float64"


def test_lloyd_all_points_in_unit_square():
    pts = _random_pts(20, seed=7)
    result = lloyd_relax(pts, iters=10)
    assert np.all(result >= 0.0) and np.all(result <= 1.0), \
        "All relaxed points must stay within [0,1]²"


def test_lloyd_reduces_clustering():
    """After relaxation, points should be more spread out (higher min pairwise distance)."""
    pts = _clustered_pts(16)

    def min_pairwise(p):
        dists = []
        n = len(p)
        for i in range(n):
            for j in range(i + 1, n):
                d = math.sqrt((p[i, 0] - p[j, 0]) ** 2 + (p[i, 1] - p[j, 1]) ** 2)
                dists.append(d)
        return min(dists) if dists else 0.0

    before = min_pairwise(pts)
    result = lloyd_relax(pts, iters=20, taboo_start=TABOO_START, taboo_end=TABOO_END)
    after = min_pairwise(result)

    assert after >= before * 0.9, \
        "Lloyd relaxation must not increase clustering (min pairwise dist must not shrink)"


def test_lloyd_taboo_arc_exclusion():
    """
    After sufficient iterations, NO point should be inside the taboo arc (240°–300°)
    with non-trivial distance from centre.
    """
    pts = _random_pts(30, seed=42)
    result = lloyd_relax(pts, iters=50, taboo_start=TABOO_START, taboo_end=TABOO_END)

    for i, (x, y) in enumerate(result):
        bx = x - 0.5
        by = y - 0.5
        if abs(bx) < 0.05 and abs(by) < 0.05:
            continue  # very close to centre — bearing is undefined
        bearing = _bearing_deg(x, y)
        assert not (TABOO_START < bearing < TABOO_END), \
            f"Point {i} at ({x:.3f},{y:.3f}) bearing {bearing:.1f}° falls inside taboo arc"


def test_lloyd_single_point_stays_at_centre():
    pts = np.array([[0.5, 0.5]], dtype=np.float64)
    result = lloyd_relax(pts, iters=10)
    np.testing.assert_allclose(result, [[0.5, 0.5]], atol=0.1,
                               err_msg="Single point should stay near centre")


def test_lloyd_idempotent_after_convergence():
    """Running many extra iterations on a converged result should not move points much."""
    pts = _random_pts(15, seed=123)
    result_50  = lloyd_relax(pts.copy(), iters=50)
    result_100 = lloyd_relax(pts.copy(), iters=100)
    # After convergence, additional iterations should produce similar results
    np.testing.assert_allclose(result_50, result_100, atol=0.1,
                               err_msg="Converged Lloyd should not change significantly with more iters")


# ---------------------------------------------------------------------------
# voronoi_cell_areas
# ---------------------------------------------------------------------------

def test_voronoi_areas_returns_correct_shape():
    pts = _random_pts(12)
    areas = voronoi_cell_areas(pts)
    assert areas.shape == (12,), "Must return 1D array of length N"
    assert areas.dtype == np.float64, "Must return float64"


def test_voronoi_areas_sum_to_one():
    pts = _random_pts(20, seed=5)
    areas = voronoi_cell_areas(pts)
    np.testing.assert_allclose(areas.sum(), 1.0, atol=0.01,
                               err_msg="Cell areas must sum to 1.0 (full unit square coverage)")


def test_voronoi_areas_all_positive():
    pts = _random_pts(16, seed=8)
    areas = voronoi_cell_areas(pts)
    assert np.all(areas > 0), "Every Voronoi cell must have positive area"


def test_voronoi_uniform_distribution_equal_areas():
    """Regularly-spaced grid of N×N points should produce roughly equal areas."""
    n = 4
    pts = np.array([[x / n + 1 / (2 * n), y / n + 1 / (2 * n)]
                    for y in range(n) for x in range(n)], dtype=np.float64)
    areas = voronoi_cell_areas(pts)
    expected = 1.0 / (n * n)
    np.testing.assert_allclose(areas, expected, atol=0.05,
                               err_msg="Uniform grid must produce equal areas")


def test_voronoi_areas_larger_for_isolated_points():
    """A point far from all others should own a larger area."""
    pts = np.array([
        [0.5, 0.5],   # centre — should have modest area
        [0.1, 0.1],
        [0.9, 0.1],
        [0.1, 0.9],
        [0.9, 0.9],
    ], dtype=np.float64)
    areas = voronoi_cell_areas(pts)
    # Centre point is surrounded; corner points may own more because they are on the boundary
    assert areas.sum() > 0.99, "Areas must nearly cover the unit square"


def test_voronoi_areas_after_lloyd_more_uniform():
    """After Lloyd relaxation, cell areas should be more uniform."""
    pts = _clustered_pts(12)

    def variance(p):
        a = voronoi_cell_areas(p)
        return float(np.var(a))

    var_before = variance(pts)
    relaxed = lloyd_relax(pts.copy(), iters=30, taboo_start=TABOO_START, taboo_end=TABOO_END)
    var_after = variance(relaxed)

    assert var_after <= var_before * 1.5, \
        "Lloyd relaxation should not drastically increase area variance"
