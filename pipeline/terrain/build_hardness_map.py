#!/usr/bin/env python3
"""
Mahlanya Production Pipeline: /pipeline/terrain/build_hardness_map.py
Author: Charles Bartaria (cbartaria1)
Date: 2026-06-25

Rasterises geological survey data to a float32 rock-hardness coefficient
raster (0.0–1.0) matching the DEM extent. Falls back to an elevation-band
synthetic map when GDAL/rasterio or the vector file are unavailable.

Geotechnical hardness values (erodibility coefficient k):
  Granite gneiss (Highveld)      : 0.002
  Lubombo rhyolite               : 0.015
  Default / mixed                : 0.05
  Karoo sedimentary (sandstone)  : 0.08
  Alluvial deposits              : 0.12
"""

import pathlib
import time

import numpy as np


# ---------------------------------------------------------------------------
# Logging helper (matches erode_terrain.py style)
# ---------------------------------------------------------------------------

def log_info(msg: str) -> None:
    print(f"[{time.strftime('%Y-%m-%d %H:%M:%S')}] [INFO] {msg}")


# ---------------------------------------------------------------------------
# Hardness look-up table
# ---------------------------------------------------------------------------

HARDNESS_BY_NAME: dict[str, float] = {
    # Exact / partial name matching (lower-cased)
    "granite":     0.002,
    "gneiss":      0.002,
    "granite gneiss": 0.002,
    "rhyolite":    0.015,
    "lubombo":     0.015,
    "sandstone":   0.08,
    "shale":       0.08,
    "karoo":       0.08,
    "sedimentary": 0.08,
    "alluvial":    0.12,
    "alluvium":    0.12,
    "river":       0.12,
    "fluvial":     0.12,
}

DEFAULT_HARDNESS: float = 0.05

# Candidate attribute field names to inspect in the vector features
_GEOLOGY_FIELDS = ("GEOLOGY", "rock_type", "lithology", "LITH", "geology",
                   "ROCK_TYPE", "LITHOLOGY", "lith", "FORMATION", "formation")


def _name_to_hardness(name: str) -> float:
    """Map a lithology name string to a hardness coefficient."""
    lower = name.lower().strip()
    for key, val in HARDNESS_BY_NAME.items():
        if key in lower:
            return val
    return DEFAULT_HARDNESS


def _elevation_hardness(elevation: np.ndarray) -> np.ndarray:
    """
    Synthetic hardness from elevation bands matching Eswatini geology:
      > 1400 m  — Highveld granite gneiss  (0.002)
      600–1400 m — Middleveld mixed         (0.05)
      200–600 m  — Lowveld Karoo sediments  (0.08)
      < 200 m   — Lowveld alluvial          (0.12)
    """
    out = np.full(elevation.shape, DEFAULT_HARDNESS, dtype=np.float32)
    out = np.where(elevation > 1400.0,  0.002, out).astype(np.float32)
    out = np.where((elevation >= 600.0) & (elevation <= 1400.0), 0.05, out)
    out = np.where((elevation >= 200.0) & (elevation <  600.0),  0.08, out)
    out = np.where(elevation < 200.0,   0.12, out).astype(np.float32)
    return out


# ---------------------------------------------------------------------------
# GeoTIFF I/O helpers (matches erode_terrain.py style)
# ---------------------------------------------------------------------------

def _load_geotiff_cpu(path: pathlib.Path):
    """Load a single-band GeoTIFF into a (data, profile) tuple. Returns None on failure."""
    try:
        import rasterio
        with rasterio.open(str(path)) as src:
            data = src.read(1).astype(np.float32)
            profile = src.profile.copy()
        return data, profile
    except Exception as exc:
        log_info(f"Could not load {path}: {exc}")
        return None, None


def _save_geotiff(data: np.ndarray, path: pathlib.Path, reference_path: pathlib.Path = None) -> None:
    """Write a 2D float32 array to a GeoTIFF, copying CRS/transform from reference if possible."""
    log_info(f"Writing hardness raster to {path} ...")
    path.parent.mkdir(parents=True, exist_ok=True)
    try:
        import rasterio
        if reference_path and reference_path.exists():
            with rasterio.open(str(reference_path)) as ref:
                profile = ref.profile.copy()
            profile.update(dtype="float32", count=1)
        else:
            h, w = data.shape
            profile = dict(
                driver="GTiff", height=h, width=w,
                count=1, dtype="float32",
            )
        with rasterio.open(str(path), "w", **profile) as dst:
            dst.write(data, 1)
    except ImportError:
        npy_path = path.with_suffix(".npy")
        np.save(str(npy_path), data)
        log_info(f"rasterio unavailable — saved as {npy_path}")


# ---------------------------------------------------------------------------
# Core rasterisation from vector
# ---------------------------------------------------------------------------

def _rasterize_geology_vector(
    geology_vector_path: pathlib.Path,
    reference_profile: dict,
    height: int,
    width: int,
) -> np.ndarray | None:
    """
    Rasterise geological vector polygons onto the DEM grid.
    Returns a float32 hardness array, or None if rasterisation fails.
    """
    try:
        import rasterio
        import rasterio.features
        try:
            import fiona
        except ImportError:
            log_info("fiona unavailable — cannot read vector file")
            return None

        transform = reference_profile.get("transform")

        shapes = []  # list of (geometry, hardness_value)

        with fiona.open(str(geology_vector_path)) as layer:
            # Identify which attribute field carries lithology information
            schema_props = list(layer.schema["properties"].keys())
            geology_field = None
            for candidate in _GEOLOGY_FIELDS:
                if candidate in schema_props:
                    geology_field = candidate
                    break

            if geology_field:
                log_info(f"Using vector field '{geology_field}' for hardness look-up")
            else:
                log_info("No recognised geology field — assigning hardness by feature index")

            for idx, feature in enumerate(layer):
                geom = feature["geometry"]
                if geology_field:
                    raw = feature["properties"].get(geology_field) or ""
                    hardness_val = _name_to_hardness(str(raw))
                else:
                    # Cycle through representative values when field absent
                    representative = [0.002, 0.05, 0.08, 0.015, 0.12]
                    hardness_val = representative[idx % len(representative)]
                shapes.append((geom, hardness_val))

        if not shapes:
            log_info("Vector file contains no features")
            return None

        out = rasterio.features.rasterize(
            shapes,
            out_shape=(height, width),
            transform=transform,
            fill=DEFAULT_HARDNESS,
            dtype="float32",
        )
        return out.astype(np.float32)

    except Exception as exc:
        log_info(f"Vector rasterisation failed: {exc}")
        return None


# ---------------------------------------------------------------------------
# Public API
# ---------------------------------------------------------------------------

def build_hardness_map(
    dem_path: pathlib.Path,
    geology_vector_path: pathlib.Path,
    output_path: pathlib.Path,
) -> pathlib.Path:
    """
    Rasterise geological survey data to a hardness coefficient raster (float32, 0–1).

    Strategy:
      1. If geology vector exists and rasterio+fiona are available: rasterise
         polygons directly onto the DEM grid using attribute lithology names.
      2. Else if DEM exists: generate synthetic hardness from elevation bands.
      3. Else: return a 32×32 uniform synthetic map at DEFAULT_HARDNESS.

    Args:
        dem_path:             Input DEM GeoTIFF.
        geology_vector_path:  Geological survey vector file (GeoPackage, Shapefile …).
        output_path:          Destination float32 GeoTIFF.

    Returns:
        Path to the written hardness raster.
    """
    log_info("build_hardness_map — Eswatini geological hardness rasterisation")

    hardness: np.ndarray | None = None
    dem_data: np.ndarray | None = None
    dem_profile: dict | None = None

    # ------------------------------------------------------------------
    # Step 1 — load DEM if it exists
    # ------------------------------------------------------------------
    if dem_path.exists():
        log_info(f"Loading DEM: {dem_path}")
        dem_data, dem_profile = _load_geotiff_cpu(dem_path)
        if dem_data is not None:
            log_info(f"DEM loaded — shape {dem_data.shape[0]}×{dem_data.shape[1]}, "
                     f"elevation range {dem_data.min():.1f}–{dem_data.max():.1f} m")
    else:
        log_info(f"DEM not found at {dem_path} — will use synthetic fallback")

    # ------------------------------------------------------------------
    # Step 2 — attempt vector rasterisation
    # ------------------------------------------------------------------
    if geology_vector_path.exists() and dem_data is not None and dem_profile is not None:
        log_info(f"Geology vector found: {geology_vector_path}")
        h, w = dem_data.shape
        hardness = _rasterize_geology_vector(geology_vector_path, dem_profile, h, w)
        if hardness is not None:
            log_info("Vector rasterisation succeeded")
        else:
            log_info("Vector rasterisation failed — falling back to elevation bands")
    elif not geology_vector_path.exists():
        log_info(f"Geology vector not found at {geology_vector_path} — using elevation-band synthesis")

    # ------------------------------------------------------------------
    # Step 3 — elevation-band synthetic fallback
    # ------------------------------------------------------------------
    if hardness is None:
        if dem_data is not None:
            log_info("Generating synthetic hardness from Eswatini elevation bands ...")
            hardness = _elevation_hardness(dem_data)
            log_info(f"Synthetic hardness: min={hardness.min():.4f}  max={hardness.max():.4f}")
        else:
            log_info("No DEM available — generating 32×32 uniform hardness map (k=0.05)")
            hardness = np.full((32, 32), DEFAULT_HARDNESS, dtype=np.float32)

    # ------------------------------------------------------------------
    # Step 4 — clip to valid range and write output
    # ------------------------------------------------------------------
    hardness = np.clip(hardness, 0.0, 1.0).astype(np.float32)
    _save_geotiff(hardness, output_path, reference_path=dem_path)
    log_info(f"Hardness map complete → {output_path}  "
             f"(shape {hardness.shape[0]}×{hardness.shape[1]}, "
             f"range {hardness.min():.4f}–{hardness.max():.4f})")
    return output_path
