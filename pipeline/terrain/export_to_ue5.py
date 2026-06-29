#!/usr/bin/env python3
"""
Mahlanya Production Pipeline: /pipeline/terrain/export_to_ue5.py
Author: Charles Bartaria (cbartaria1)
Date: 2026-06-25

Splits the eroded Eswatini heightmap into UE5 Landscape tiles (.r16) and
optionally tiles any weight-map GeoTIFFs into per-layer 8-bit PNGs.

UE5 Landscape tile requirements
  - Tile size must be 2^n + 1  (e.g. 1009, 505, 253, 127 …)
  - Format: .r16 — raw 16-bit unsigned little-endian integers (0–65535)
  - Each tile:  tile_{row:03d}_{col:03d}.r16
  - Manifest:   tiles_manifest.json

Weight maps (optional)
  - Looked up from weight_maps_dir: rock_hardness.tif, sediment_deposit.tif …
  - Tiled to 8-bit PNG: weight_maps/{layer_name}/tile_{row:03d}_{col:03d}.png
"""

import json
import pathlib
import struct
import time

import numpy as np


# ---------------------------------------------------------------------------
# Logging helper (matches erode_terrain.py style)
# ---------------------------------------------------------------------------

def log_info(msg: str) -> None:
    print(f"[{time.strftime('%Y-%m-%d %H:%M:%S')}] [INFO] {msg}")


# ---------------------------------------------------------------------------
# UE5 tile-size validation
# ---------------------------------------------------------------------------

# All landscape sizes documented as valid by Unreal Engine 5.
# The pattern is (n-1) being a product of 2^a * b where b ∈ {1,2,3,4,6,7} etc.,
# but in practice UE5 publishes a fixed table. We use a generous set that covers
# every size listed in the UE5 Landscape documentation.
_UE5_VALID_TILE_SIZES: frozenset[int] = frozenset([
    # 2^n+1 (power-of-two + 1) — strict mathematical form
    33, 65, 129, 257, 513, 1025, 2049, 4097, 8193,
    # Additional UE5-documented sizes (composite quads)
    127, 253, 505, 1009, 2017, 4033, 8129,
    63, 125, 249,
])


def _is_valid_ue5_tile_size(n: int) -> bool:
    """Return True if n is a size recognised by UE5's Landscape system."""
    return n in _UE5_VALID_TILE_SIZES


# ---------------------------------------------------------------------------
# GeoTIFF loading (CPU only; mirrors erode_terrain.py fallback pattern)
# ---------------------------------------------------------------------------

def _load_geotiff(path: pathlib.Path) -> tuple[np.ndarray | None, dict | None]:
    """Load a single-band GeoTIFF. Returns (data float32, profile) or (None, None)."""
    log_info(f"Loading {path} ...")
    try:
        import rasterio
        with rasterio.open(str(path)) as src:
            data = src.read(1).astype(np.float32)
            profile = src.profile.copy()
        return data, profile
    except ImportError:
        log_info("rasterio unavailable — cannot load GeoTIFF")
        return None, None
    except Exception as exc:
        log_info(f"Failed to load {path}: {exc}")
        return None, None


# ---------------------------------------------------------------------------
# Tile helpers
# ---------------------------------------------------------------------------

def _pad_to_tile(arr: np.ndarray, row_start: int, col_start: int, tile_size: int) -> np.ndarray:
    """
    Extract a tile_size×tile_size patch from arr starting at (row_start, col_start).
    Pads with edge replication when the tile extends beyond the array boundary.
    """
    h, w = arr.shape
    row_end = row_start + tile_size
    col_end = col_start + tile_size

    # How much padding is needed on each side?
    pad_bottom = max(0, row_end - h)
    pad_right  = max(0, col_end - w)

    row_end_clamp = min(row_end, h)
    col_end_clamp = min(col_end, w)

    patch = arr[row_start:row_end_clamp, col_start:col_end_clamp]

    if pad_bottom > 0 or pad_right > 0:
        patch = np.pad(patch, ((0, pad_bottom), (0, pad_right)), mode="edge")

    return patch


def _normalise_to_uint16(data: np.ndarray, global_min: float, global_max: float) -> np.ndarray:
    """Map float32 elevation → uint16 (0–65535)."""
    span = global_max - global_min
    if span < 1e-6:
        span = 1.0
    normalised = (data - global_min) / span
    return (np.clip(normalised, 0.0, 1.0) * 65535.0).astype(np.uint16)


def _write_r16(tile_uint16: np.ndarray, path: pathlib.Path) -> None:
    """Write a uint16 array as a raw .r16 file (little-endian)."""
    path.parent.mkdir(parents=True, exist_ok=True)
    # Ensure little-endian byte order
    data_le = tile_uint16.astype("<u2")
    path.write_bytes(data_le.tobytes())


def _normalise_to_uint8(data: np.ndarray) -> np.ndarray:
    """Map float32 (any range) → uint8 (0–255)."""
    mn, mx = data.min(), data.max()
    span = mx - mn
    if span < 1e-6:
        span = 1.0
    return (np.clip((data - mn) / span, 0.0, 1.0) * 255.0).astype(np.uint8)


def _write_png_grey(tile_uint8: np.ndarray, path: pathlib.Path) -> None:
    """Write a 2D uint8 array as a grayscale PNG (pure-Python zlib path)."""
    path.parent.mkdir(parents=True, exist_ok=True)
    try:
        # Prefer Pillow
        from PIL import Image
        img = Image.fromarray(tile_uint8, mode="L")
        img.save(str(path))
        return
    except ImportError:
        pass
    # Fallback: minimal PNG encoder using only stdlib zlib
    import zlib
    h, w = tile_uint8.shape

    def _png_chunk(tag: bytes, data: bytes) -> bytes:
        length = struct.pack(">I", len(data))
        crc = struct.pack(">I", zlib.crc32(tag + data) & 0xFFFFFFFF)
        return length + tag + data + crc

    ihdr_data = struct.pack(">IIBBBBB", w, h, 8, 0, 0, 0, 0)  # 8-bit greyscale
    raw_rows = b"".join(b"\x00" + bytes(row) for row in tile_uint8)
    idat_data = zlib.compress(raw_rows, level=6)

    png = (
        b"\x89PNG\r\n\x1a\n"
        + _png_chunk(b"IHDR", ihdr_data)
        + _png_chunk(b"IDAT", idat_data)
        + _png_chunk(b"IEND", b"")
    )
    path.write_bytes(png)


# ---------------------------------------------------------------------------
# Weight-map tiling
# ---------------------------------------------------------------------------

def _tile_weight_map(
    tif_path: pathlib.Path,
    layer_name: str,
    output_dir: pathlib.Path,
    height: int,
    width: int,
    tile_size: int,
    total_rows: int,
    total_cols: int,
) -> None:
    """Tile a single weight-map GeoTIFF into per-tile 8-bit PNGs."""
    data, _ = _load_geotiff(tif_path)
    if data is None:
        log_info(f"  Weight map {layer_name}: could not load, skipping")
        return

    # Resize to match heightmap dimensions if needed
    if data.shape != (height, width):
        try:
            from PIL import Image
            img = Image.fromarray(data).resize((width, height), Image.BILINEAR)
            data = np.array(img, dtype=np.float32)
        except ImportError:
            # Simple nearest-neighbour resize via slicing
            row_idx = (np.arange(height) * data.shape[0] / height).astype(int)
            col_idx = (np.arange(width)  * data.shape[1] / width ).astype(int)
            data = data[np.ix_(row_idx, col_idx)]

    wm_dir = output_dir / "weight_maps" / layer_name
    tile_count = 0
    for row in range(total_rows):
        for col in range(total_cols):
            patch = _pad_to_tile(data, row * tile_size, col * tile_size, tile_size)
            tile_uint8 = _normalise_to_uint8(patch)
            out_path = wm_dir / f"tile_{row:03d}_{col:03d}.png"
            _write_png_grey(tile_uint8, out_path)
            tile_count += 1
    log_info(f"  Weight map '{layer_name}': {tile_count} PNG tiles → {wm_dir}")


# ---------------------------------------------------------------------------
# Public API
# ---------------------------------------------------------------------------

def export_to_ue5(
    eroded_dem_path: pathlib.Path,
    weight_maps_dir: pathlib.Path,
    output_dir: pathlib.Path,
    tile_size: int = 1009,
) -> pathlib.Path:
    """
    Split the eroded heightmap into UE5 Landscape tiles (.r16) and write a manifest.

    Args:
        eroded_dem_path: Eroded DEM GeoTIFF (float32 elevation in metres).
        weight_maps_dir: Directory of weight-map GeoTIFFs (rock_hardness.tif, etc.).
                         Ignored gracefully if it does not exist.
        output_dir:      Destination directory for tiles and manifest.
        tile_size:       Tile edge length in pixels; must be 2^n+1 (default 1009).

    Returns:
        Path to the tiles_manifest.json file.
    """
    log_info(f"export_to_ue5 — UE5 Landscape tile export  tile_size={tile_size}")

    # ------------------------------------------------------------------
    # Validate tile size
    # ------------------------------------------------------------------
    if not _is_valid_ue5_tile_size(tile_size):
        raise ValueError(
            f"tile_size={tile_size} is not a valid UE5 Landscape size (must be 2^n+1, "
            "e.g. 127, 253, 505, 1009)."
        )

    output_dir.mkdir(parents=True, exist_ok=True)

    # ------------------------------------------------------------------
    # Load heightmap (or synthesise a zero tile on failure)
    # ------------------------------------------------------------------
    if eroded_dem_path.exists():
        dem_data, dem_profile = _load_geotiff(eroded_dem_path)
    else:
        log_info(f"DEM not found at {eroded_dem_path} — generating synthetic zero tile")
        dem_data = None
        dem_profile = None

    if dem_data is None:
        log_info("Using synthetic 1-tile zero heightmap")
        dem_data = np.zeros((tile_size, tile_size), dtype=np.float32)

    height, width = dem_data.shape
    log_info(f"Heightmap dimensions: {height}×{width} px")

    # ------------------------------------------------------------------
    # Global min/max for consistent uint16 normalisation
    # ------------------------------------------------------------------
    min_elev = float(dem_data.min())
    max_elev = float(dem_data.max())
    log_info(f"Elevation range: {min_elev:.2f} m — {max_elev:.2f} m")

    # ------------------------------------------------------------------
    # Calculate tile grid
    # ------------------------------------------------------------------
    import math
    total_rows = math.ceil(height / tile_size)
    total_cols = math.ceil(width  / tile_size)
    total_tiles = total_rows * total_cols
    log_info(f"Tile grid: {total_rows} rows × {total_cols} cols = {total_tiles} tiles")

    # ------------------------------------------------------------------
    # Write .r16 heightmap tiles
    # ------------------------------------------------------------------
    tiles_written = 0
    for row in range(total_rows):
        for col in range(total_cols):
            patch = _pad_to_tile(dem_data, row * tile_size, col * tile_size, tile_size)
            tile_u16 = _normalise_to_uint16(patch, min_elev, max_elev)
            tile_path = output_dir / f"tile_{row:03d}_{col:03d}.r16"
            _write_r16(tile_u16, tile_path)
            tiles_written += 1
            if tiles_written % 10 == 0 or tiles_written == total_tiles:
                log_info(f"  Tiles written: {tiles_written}/{total_tiles}")

    log_info(f"Heightmap tiles complete — {tiles_written} .r16 files in {output_dir}")

    # ------------------------------------------------------------------
    # Write tiles_manifest.json
    # ------------------------------------------------------------------
    manifest = {
        "total_rows":       total_rows,
        "total_cols":       total_cols,
        "tile_size":        tile_size,
        "min_elevation_m":  round(min_elev, 4),
        "max_elevation_m":  round(max_elev, 4),
        "overlap_pixels":   0,
    }
    manifest_path = output_dir / "tiles_manifest.json"
    manifest_path.write_text(json.dumps(manifest, indent=2))
    log_info(f"Manifest written → {manifest_path}")

    # ------------------------------------------------------------------
    # Weight-map tiling (optional)
    # ------------------------------------------------------------------
    if weight_maps_dir.exists():
        tif_files = list(weight_maps_dir.glob("*.tif")) + list(weight_maps_dir.glob("*.tiff"))
        if tif_files:
            log_info(f"Tiling {len(tif_files)} weight map(s) from {weight_maps_dir} ...")
            for tif_path in sorted(tif_files):
                layer_name = tif_path.stem  # e.g. "rock_hardness"
                log_info(f"  Processing weight map: {layer_name}")
                try:
                    _tile_weight_map(
                        tif_path, layer_name, output_dir,
                        height, width, tile_size,
                        total_rows, total_cols,
                    )
                except Exception as exc:
                    log_info(f"  Weight map '{layer_name}' failed (skipping): {exc}")
        else:
            log_info(f"No .tif files found in {weight_maps_dir} — skipping weight maps")
    else:
        log_info(f"weight_maps_dir not found ({weight_maps_dir}) — skipping weight maps")

    log_info(f"export_to_ue5 complete → {output_dir}")
    return manifest_path
