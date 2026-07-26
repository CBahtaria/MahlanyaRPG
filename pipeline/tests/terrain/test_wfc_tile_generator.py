"""Tests for pipeline/terrain/wfc_tile_generator.py.

All 12 tests exercise geographic adjacency validity, determinism,
serialisation, and the retry-on-contradiction path.
"""
from __future__ import annotations

import sys
import pathlib

# Allow direct imports from the pipeline package when pytest is run from repo root.
_PIPELINE_DIR = pathlib.Path(__file__).parent.parent.parent
if str(_PIPELINE_DIR) not in sys.path:
    sys.path.insert(0, str(_PIPELINE_DIR))

import pytest

from terrain.wfc_tile_generator import (
    ADJACENCY,
    Tile,
    WFCGrid,
    generate_biome_map,
)

_ALL_TILES = frozenset(Tile)


# ---------------------------------------------------------------------------
# 1. Fresh grid — all 7 tiles in every cell
# ---------------------------------------------------------------------------

def test_all_tiles_start_as_superposition():
    grid = WFCGrid(4, 4, seed=0)
    for row in grid.cells:
        for cell in row:
            assert cell == set(_ALL_TILES), (
                f"Expected all 7 tiles in fresh cell, got {cell}"
            )


# ---------------------------------------------------------------------------
# 2. generate() fills the entire grid (no None / empty cells)
# ---------------------------------------------------------------------------

def test_generate_fills_entire_grid():
    grid = WFCGrid(5, 5, seed=7)
    result = grid.generate()
    assert len(result) == 5
    for row in result:
        assert len(row) == 5
        for tile in row:
            assert isinstance(tile, Tile), f"Expected Tile instance, got {tile!r}"


# ---------------------------------------------------------------------------
# 3. Same seed → identical output
# ---------------------------------------------------------------------------

def test_generate_deterministic_same_seed():
    a = WFCGrid(6, 6, seed=42).generate()
    b = WFCGrid(6, 6, seed=42).generate()
    assert a == b, "Two runs with seed=42 produced different grids"


# ---------------------------------------------------------------------------
# 4. Different seeds → different grids (for a non-trivial size)
# ---------------------------------------------------------------------------

def test_generate_differs_different_seeds():
    a = WFCGrid(8, 8, seed=1).generate()
    b = WFCGrid(8, 8, seed=2).generate()
    assert a != b, "seed=1 and seed=2 produced identical grids — unlikely for 8×8"


# ---------------------------------------------------------------------------
# 5. No invalid adjacency in any generated grid
# ---------------------------------------------------------------------------

def test_no_invalid_adjacency():
    grid = WFCGrid(10, 10, seed=99)
    result = grid.generate()
    height = len(result)
    width = len(result[0])
    for r in range(height):
        for c in range(width):
            tile = result[r][c]
            for dr, dc in ((-1, 0), (1, 0), (0, -1), (0, 1)):
                nr, nc = r + dr, c + dc
                if 0 <= nr < height and 0 <= nc < width:
                    neighbor = result[nr][nc]
                    assert neighbor in ADJACENCY[tile], (
                        f"Invalid adjacency at ({r},{c})→({nr},{nc}): "
                        f"{tile.value} borders {neighbor.value}"
                    )


# ---------------------------------------------------------------------------
# 6. Highveld never borders Lowveld directly
# ---------------------------------------------------------------------------

def test_no_highveld_adjacent_to_lowveld():
    # Run several seeds to build confidence.
    for seed in range(10):
        result = WFCGrid(8, 8, seed=seed).generate()
        height, width = len(result), len(result[0])
        for r in range(height):
            for c in range(width):
                if result[r][c] is not Tile.HIGHVELD:
                    continue
                for dr, dc in ((-1, 0), (1, 0), (0, -1), (0, 1)):
                    nr, nc = r + dr, c + dc
                    if 0 <= nr < height and 0 <= nc < width:
                        assert result[nr][nc] is not Tile.LOWVELD, (
                            f"seed={seed}: HIGHVELD at ({r},{c}) borders "
                            f"LOWVELD at ({nr},{nc})"
                        )


# ---------------------------------------------------------------------------
# 7. River never borders Lubombo
# ---------------------------------------------------------------------------

def test_no_river_adjacent_to_lubombo():
    for seed in range(10):
        result = WFCGrid(8, 8, seed=seed).generate()
        height, width = len(result), len(result[0])
        for r in range(height):
            for c in range(width):
                if result[r][c] is not Tile.RIVER:
                    continue
                for dr, dc in ((-1, 0), (1, 0), (0, -1), (0, 1)):
                    nr, nc = r + dr, c + dc
                    if 0 <= nr < height and 0 <= nc < width:
                        assert result[nr][nc] is not Tile.LUBOMBO, (
                            f"seed={seed}: RIVER at ({r},{c}) borders "
                            f"LUBOMBO at ({nr},{nc})"
                        )


# ---------------------------------------------------------------------------
# 8. Small 3×3 grid completes without error
# ---------------------------------------------------------------------------

def test_small_grid_completes():
    result = WFCGrid(3, 3, seed=5).generate()
    assert len(result) == 3
    assert all(len(row) == 3 for row in result)


# ---------------------------------------------------------------------------
# 9. Large 20×20 grid completes without error
# ---------------------------------------------------------------------------

def test_large_grid_completes():
    result = WFCGrid(20, 20, seed=314).generate()
    assert len(result) == 20
    assert all(len(row) == 20 for row in result)


# ---------------------------------------------------------------------------
# 10. to_dict() serialisation format
# ---------------------------------------------------------------------------

def test_to_dict_format():
    grid = WFCGrid(3, 4, seed=0)
    grid.generate()
    d = grid.to_dict()

    assert set(d.keys()) >= {"width", "height", "seed", "tiles"}
    assert d["width"] == 3
    assert d["height"] == 4
    assert d["seed"] == 0
    assert len(d["tiles"]) == 4
    for row in d["tiles"]:
        assert len(row) == 3
        for val in row:
            assert isinstance(val, str), f"Expected str tile value, got {val!r}"
            assert val in {t.value for t in Tile}, f"Unknown tile value: {val!r}"


# ---------------------------------------------------------------------------
# 11. generate_biome_map() top-level function — structure check
# ---------------------------------------------------------------------------

def test_generate_biome_map_returns_correct_structure():
    result = generate_biome_map(width=5, height=5, seed=0)
    required_keys = {"width", "height", "seed", "tiles", "retries_used"}
    assert required_keys <= set(result.keys()), (
        f"Missing keys: {required_keys - set(result.keys())}"
    )
    assert result["width"] == 5
    assert result["height"] == 5
    assert isinstance(result["retries_used"], int)
    assert len(result["tiles"]) == 5
    for row in result["tiles"]:
        assert len(row) == 5


# ---------------------------------------------------------------------------
# 12. generate_biome_map() retries on contradiction
# ---------------------------------------------------------------------------

def test_generate_biome_map_retries_on_bad_seed(monkeypatch):
    """First call to WFCGrid.generate raises ValueError; second succeeds.

    Asserts retries_used == 1.
    """
    call_count = 0
    original_generate = WFCGrid.generate

    def patched_generate(self: WFCGrid) -> list[list[Tile]]:
        nonlocal call_count
        call_count += 1
        if call_count == 1:
            raise ValueError("Forced contradiction for test")
        return original_generate(self)

    monkeypatch.setattr(WFCGrid, "generate", patched_generate)

    result = generate_biome_map(width=4, height=4, seed=0, max_retries=5)
    assert result["retries_used"] == 1, (
        f"Expected 1 retry, got {result['retries_used']}"
    )
    assert call_count == 2
