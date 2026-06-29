#!/usr/bin/env python3
"""
Mahlanya Production Pipeline: /pipeline/terrain/erode_terrain.py
Author: Charles Bartaria (cbartaria1)
Date: 2026-06-24

Full geomorphological erosion stack driven by the Zig SIMD compute library
(libmahlanya_compute.so). Transparent NumPy fallback when the Zig library
is absent (e.g. build machine without Zig installed).

Pass sequence per iteration:
  Every step:   fluvial erosion
  Every 5th:    thermal erosion
  Every 20th:   aeolian erosion
  Every 50th:   mass wasting

Estimated runtime at 5000 iterations on 15 400 × 17 200 grid:
  Zig SIMD (16 cores, AVX2):  ~1.5 hours
  NumPy CPU fallback:         ~18 hours
"""

import argparse
import pathlib
import time

import numpy as np

from pipeline.compute.libmahlanya_compute import (
    thermal_erosion_pass,
    fluvial_erosion_pass,
    aeolian_erosion_pass,
    mass_wasting_pass,
    USING_ZIG,
)


def log_info(msg: str) -> None:
    print(f"[{time.strftime('%Y-%m-%d %H:%M:%S')}] [INFO] {msg}")


def load_geotiff(path: pathlib.Path) -> np.ndarray:
    """Load a single-band GeoTIFF into a 2D float32 NumPy array."""
    log_info(f"Loading {path} ...")
    try:
        import rasterio
        with rasterio.open(str(path)) as src:
            return src.read(1).astype(np.float32)
    except ImportError:
        log_info("rasterio unavailable — initialising zero matrix (4096×4096)")
        return np.zeros((4096, 4096), dtype=np.float32)


def save_geotiff(matrix: np.ndarray, path: pathlib.Path,
                 reference_path: pathlib.Path = None) -> None:
    """Write a 2D float32 array to a GeoTIFF, copying CRS/transform from reference."""
    log_info(f"Writing output to {path} ...")
    try:
        import rasterio
        if reference_path and reference_path.exists():
            with rasterio.open(str(reference_path)) as ref:
                profile = ref.profile
            profile.update(dtype=rasterio.float32, count=1)
            with rasterio.open(str(path), "w", **profile) as dst:
                dst.write(matrix, 1)
        else:
            h, w = matrix.shape
            with rasterio.open(
                str(path), "w",
                driver="GTiff", height=h, width=w,
                count=1, dtype="float32",
            ) as dst:
                dst.write(matrix, 1)
    except ImportError:
        np.save(str(path.with_suffix(".npy")), matrix)
        log_info("rasterio unavailable — saved as .npy")


def erode_terrain(
    dem_path: pathlib.Path,
    hardness_path: pathlib.Path,
    output_path: pathlib.Path,
    iterations: int = 5000,
    wind_direction_deg: float = 75.0,
    aeolian_drift_rate: float = 0.001,
) -> pathlib.Path:
    """
    Run the full geomorphological erosion stack on the Eswatini DEM.

    All erosion passes are delegated to libmahlanya_compute (Zig SIMD) with
    automatic NumPy fallback.

    Args:
        dem_path:           Input DEM GeoTIFF (float32, UTM Zone 36S).
        hardness_path:      Rock hardness GeoTIFF (float32, 0–1).
        output_path:        Output eroded heightmap GeoTIFF.
        iterations:         Number of erosion iterations (default 5000).
        wind_direction_deg: Prevailing wind bearing (default 75° — ENE trade winds).
        aeolian_drift_rate: Aeolian saltation strength (default 0.001).

    Returns:
        Path to the eroded heightmap GeoTIFF.
    """
    backend = "Zig SIMD" if USING_ZIG else "NumPy CPU"
    log_info(f"Eswatini Geomorphological Erosion Stack — backend: {backend}")
    log_info(f"Iterations: {iterations}  Wind: {wind_direction_deg}°")

    heightmap    = load_geotiff(dem_path)
    hardness     = load_geotiff(hardness_path)
    water_map    = np.zeros_like(heightmap)
    sediment_map = np.zeros_like(heightmap)

    # Ensure all arrays are C-contiguous float32 (required by Zig ctypes bridge)
    heightmap    = np.ascontiguousarray(heightmap,    dtype=np.float32)
    hardness     = np.ascontiguousarray(hardness,     dtype=np.float32)
    water_map    = np.ascontiguousarray(water_map,    dtype=np.float32)
    sediment_map = np.ascontiguousarray(sediment_map, dtype=np.float32)

    # Seed for mass wasting — deterministic per WorldSeed convention
    mass_wasting_seed = 19061906

    log_info(f"Grid: {heightmap.shape[0]}×{heightmap.shape[1]} — starting erosion loop ...")
    t0 = time.perf_counter()

    for i in range(1, iterations + 1):
        # Simulate rainfall
        water_map += 0.01

        # --- Fluvial (every step) ---
        fluvial_erosion_pass(heightmap, water_map, sediment_map, hardness,
                             gravity=9.81, erosion_rate=0.05, deposition_rate=0.1)

        # --- Thermal (every 5th) ---
        if i % 5 == 0:
            thermal_erosion_pass(heightmap, hardness,
                                 talus_angle_deg=33.0, cell_size_m=10.0)

        # --- Aeolian (every 20th) ---
        if i % 20 == 0:
            aeolian_erosion_pass(heightmap, hardness,
                                 wind_deg=wind_direction_deg,
                                 drift_rate=aeolian_drift_rate)

        # --- Mass wasting (every 50th) ---
        if i % 50 == 0:
            mass_wasting_pass(heightmap, hardness,
                              critical_slope_deg=38.0,
                              seed=mass_wasting_seed + i)

        if i % 1000 == 0 or i == iterations:
            elapsed = time.perf_counter() - t0
            eta = (elapsed / i) * (iterations - i)
            log_info(f"  Step {i:>5}/{iterations}  elapsed {elapsed:.0f}s  ETA {eta:.0f}s")

    output_path.parent.mkdir(parents=True, exist_ok=True)
    save_geotiff(heightmap, output_path, reference_path=dem_path)
    log_info(f"Erosion complete → {output_path}")
    return output_path


def main() -> None:
    parser = argparse.ArgumentParser(
        description="Mahlanya geomorphological erosion pipeline (Zig SIMD + NumPy fallback)"
    )
    parser.add_argument("--dem",        type=pathlib.Path, required=True,
                        help="Input DEM GeoTIFF (UTM 36S)")
    parser.add_argument("--hardness",   type=pathlib.Path, required=True,
                        help="Rock hardness map GeoTIFF")
    parser.add_argument("--output",     type=pathlib.Path, required=True,
                        help="Output eroded heightmap GeoTIFF")
    parser.add_argument("--iterations", type=int, default=5000)
    parser.add_argument("--wind-deg",   type=float, default=75.0,
                        help="Prevailing wind direction in degrees (default 75)")
    args = parser.parse_args()

    erode_terrain(
        dem_path=args.dem,
        hardness_path=args.hardness,
        output_path=args.output,
        iterations=args.iterations,
        wind_direction_deg=args.wind_deg,
    )


if __name__ == "__main__":
    main()
