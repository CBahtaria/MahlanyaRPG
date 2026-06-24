#!/usr/bin/env python3
"""
Mahlanya Production Pipeline: /pipeline/terrain/erode_terrain.py
Author: Charles Bartaria (cbartaria1)
Date: 2026-06-24

Full geomorphological erosion stack — GPU-accelerated (CuPy) with automatic
CPU fallback (NumPy). Runs thermal collapse, fluvial transport, aeolian
abrasion, and shear-stress landslides on the Copernicus DEM.
"""

import argparse
import pathlib
import time

try:
    import cupy as cp
    xp = cp
    USING_GPU = True
except ImportError:
    import numpy as np
    xp = np
    USING_GPU = False

import numpy as np_cpu  # Always available for I/O


def log_info(msg: str) -> None:
    print(f"[{time.strftime('%Y-%m-%d %H:%M:%S')}] [INFO] {msg}")


def load_geotiff(path: pathlib.Path):
    """Load a single-band GeoTIFF into a 2D float32 array on the active backend."""
    log_info(f"Loading {path} ...")
    try:
        import rasterio
        with rasterio.open(str(path)) as src:
            data = src.read(1).astype(np_cpu.float32)
        if USING_GPU:
            return cp.asarray(data)
        return data
    except ImportError:
        log_info("rasterio unavailable — initialising zero matrix")
        return xp.zeros((4096, 4096), dtype=xp.float32)


def save_geotiff(matrix, path: pathlib.Path, reference_path: pathlib.Path = None) -> None:
    """Write a 2D array to a GeoTIFF, optionally copying CRS/transform from reference."""
    log_info(f"Writing output to {path} ...")
    data = matrix if not USING_GPU else cp.asnumpy(matrix)
    try:
        import rasterio
        from rasterio.transform import from_bounds
        if reference_path and reference_path.exists():
            with rasterio.open(str(reference_path)) as ref:
                profile = ref.profile
            profile.update(dtype=rasterio.float32, count=1)
            with rasterio.open(str(path), "w", **profile) as dst:
                dst.write(data, 1)
        else:
            h, w = data.shape
            with rasterio.open(
                str(path), "w",
                driver="GTiff", height=h, width=w,
                count=1, dtype="float32",
            ) as dst:
                dst.write(data, 1)
    except ImportError:
        np_cpu.save(str(path.with_suffix(".npy")), data)
        log_info("rasterio unavailable — saved as .npy")


def thermal_erosion_pass(heightmap, hardness, talus_angle: float = 33.0, iterations: int = 1):
    """
    Scree accumulation on Highveld cliff faces via material talus equalization.
    Moves material down if the slope exceeds the critical talus threshold,
    modulated inversely by rock hardness (granite resists, clay collapses).
    """
    talus_rad = xp.tan(xp.radians(talus_angle))
    cell_size = 10.0  # metres — Copernicus GLO-10 resolution

    for _ in range(iterations):
        diffs = {
            (1,  0): heightmap[2:,  1:-1] - heightmap[1:-1, 1:-1],
            (-1, 0): heightmap[:-2, 1:-1] - heightmap[1:-1, 1:-1],
            (0,  1): heightmap[1:-1, 2:]  - heightmap[1:-1, 1:-1],
            (0, -1): heightmap[1:-1, :-2] - heightmap[1:-1, 1:-1],
        }
        h = hardness[1:-1, 1:-1]
        for (ox, oy), dz in diffs.items():
            excess = (-dz / cell_size) - talus_rad * (1.0 + h)
            mask = excess > 0
            if not xp.any(mask):
                continue
            move = excess * 0.1 * (1.0 - h)
            heightmap[1:-1, 1:-1] -= mask * move
            if ox == 1:
                heightmap[2:,  1:-1] += mask * move
            elif ox == -1:
                heightmap[:-2, 1:-1] += mask * move
            elif oy == 1:
                heightmap[1:-1, 2:]  += mask * move
            elif oy == -1:
                heightmap[1:-1, :-2] += mask * move
    return heightmap


def fluvial_erosion_pass(
    heightmap,
    water_map,
    sediment_map,
    hardness,
    gravity: float = 9.81,
    erosion_rate_base: float = 0.05,
    deposition_rate_base: float = 0.1,
):
    """
    Navier-Stokes variant fluvial erosion with hardness-modulated capacity.

    Erodes material proportional to flow velocity and gradient, scaled by
    geotechnical rock hardness. Deposits where transport capacity falls
    below carried sediment concentration.
    """
    cell_size = 10.0

    # Simulated rainfall input
    water_map += 0.01

    # Spatial gradient via central finite differences
    slope_x = (heightmap[2:,  1:-1] - heightmap[:-2, 1:-1]) / (2.0 * cell_size)
    slope_y = (heightmap[1:-1, 2:]  - heightmap[1:-1, :-2]) / (2.0 * cell_size)
    gradient = xp.sqrt(slope_x ** 2 + slope_y ** 2) + 1e-5

    h = hardness[1:-1, 1:-1]
    velocity = water_map[1:-1, 1:-1] * gradient * gravity
    capacity = velocity * erosion_rate_base * (1.0 - h)

    current_sediment = sediment_map[1:-1, 1:-1]

    # Erosion where capacity exceeds carried load
    erode_mask = capacity > current_sediment
    deficit = (capacity - current_sediment) * (1.0 - h) * 0.1
    heightmap[1:-1, 1:-1]    -= erode_mask * deficit
    sediment_map[1:-1, 1:-1] += erode_mask * deficit

    # Deposition where load exceeds capacity
    deposit_mask = current_sediment >= capacity
    surplus = (current_sediment - capacity) * deposition_rate_base
    heightmap[1:-1, 1:-1]    += deposit_mask * surplus
    sediment_map[1:-1, 1:-1] -= deposit_mask * surplus

    # Downstream flow routing: distribute water + sediment to lower neighbours
    for ox, oy in [(-1, 0), (1, 0), (0, -1), (0, 1)]:
        neighbour_h = heightmap[1 + ox:4095 + ox, 1 + oy:4095 + oy]
        delta = heightmap[1:-1, 1:-1] - neighbour_h
        flow_mask = delta > 0

        flow_vol = water_map[1:-1, 1:-1] * 0.25 * flow_mask
        water_map[1:-1, 1:-1] -= flow_vol
        water_map[1 + ox:4095 + ox, 1 + oy:4095 + oy] += flow_vol

        sed_vol = sediment_map[1:-1, 1:-1] * 0.25 * flow_mask
        sediment_map[1:-1, 1:-1] -= sed_vol
        sediment_map[1 + ox:4095 + ox, 1 + oy:4095 + oy] += sed_vol

    return heightmap, water_map, sediment_map


def aeolian_erosion_pass(heightmap, wind_direction_deg: float, hardness, drift_rate: float = 0.001):
    """
    Wind-driven saltation and abrasion across the dry Lowveld.
    Erodes windward faces and deposits on leeward faces.
    """
    rad = float(xp.radians(wind_direction_deg))
    wx = int(round(float(xp.cos(xp.array(rad)))))
    wy = int(round(float(xp.sin(xp.array(rad)))))

    if abs(wx) > 0 or abs(wy) > 0:
        shifted = xp.roll(heightmap, shift=(wx, wy), axis=(0, 1))
        delta = heightmap - shifted
        abrasion = xp.maximum(delta, 0) * drift_rate * (1.0 - hardness)
        heightmap -= abrasion
        heightmap = xp.roll(heightmap, shift=(-wx, -wy), axis=(0, 1)) + xp.roll(abrasion, shift=(wx, wy), axis=(0, 1))

    return heightmap


def mass_wasting_pass(heightmap, hardness, critical_slope: float = 38.0):
    """
    Probabilistic structural failure and landslide processing.
    Triggers slope collapse where shear stress exceeds geotechnical limits,
    scattering debris evenly to four cardinal neighbours.
    """
    cell_size = 10.0
    slope_threshold = xp.tan(xp.radians(critical_slope))

    slope_x = (heightmap[2:,  1:-1] - heightmap[:-2, 1:-1]) / (2.0 * cell_size)
    slope_y = (heightmap[1:-1, 2:]  - heightmap[1:-1, :-2]) / (2.0 * cell_size)
    gradient = xp.sqrt(slope_x ** 2 + slope_y ** 2)

    h = hardness[1:-1, 1:-1]
    failure_mask = gradient > (slope_threshold * (1.0 + h))
    if xp.any(failure_mask):
        collapse = (gradient - slope_threshold) * cell_size * 0.5
        heightmap[1:-1, 1:-1] -= failure_mask * collapse
        debris_quarter = failure_mask * collapse * 0.25
        heightmap[2:,  1:-1] += debris_quarter
        heightmap[:-2, 1:-1] += debris_quarter
        heightmap[1:-1, 2:]  += debris_quarter
        heightmap[1:-1, :-2] += debris_quarter

    return heightmap


def erode_terrain(
    dem_path: pathlib.Path,
    hardness_path: pathlib.Path,
    output_path: pathlib.Path,
    iterations: int = 5000,
    use_gpu: bool = True,
) -> pathlib.Path:
    """
    Run the full geomorphological erosion stack on the Eswatini DEM.

    Pass sequence per iteration:
      Every step:   fluvial erosion
      Every 5th:    thermal erosion
      Every 20th:   aeolian erosion
      Every 50th:   mass wasting

    Args:
        dem_path:     Input DEM GeoTIFF (UTM Zone 36S).
        hardness_path: Rock hardness GeoTIFF (float32, 0–1).
        output_path:  Output eroded heightmap GeoTIFF.
        iterations:   Number of erosion iterations (default 5000).
        use_gpu:      Use CuPy if available (ignored if CuPy not installed).

    Returns:
        Path to the eroded heightmap GeoTIFF.
    """
    backend = "GPU (CuPy)" if USING_GPU and use_gpu else "CPU (NumPy)"
    log_info(f"Eswatini Geomorphological Erosion Stack — backend: {backend}")

    heightmap    = load_geotiff(dem_path)
    hardness     = load_geotiff(hardness_path)
    water_map    = xp.zeros_like(heightmap)
    sediment_map = xp.zeros_like(heightmap)

    log_info(f"Running {iterations} iterations on {heightmap.shape[0]}×{heightmap.shape[1]} grid ...")
    t0 = time.perf_counter()

    for i in range(1, iterations + 1):
        heightmap, water_map, sediment_map = fluvial_erosion_pass(
            heightmap, water_map, sediment_map, hardness
        )
        if i % 5 == 0:
            heightmap = thermal_erosion_pass(heightmap, hardness)
        if i % 20 == 0:
            heightmap = aeolian_erosion_pass(heightmap, wind_direction_deg=75.0, hardness=hardness)
        if i % 50 == 0:
            heightmap = mass_wasting_pass(heightmap, hardness)

        if i % 1000 == 0 or i == iterations:
            elapsed = time.perf_counter() - t0
            eta = (elapsed / i) * (iterations - i)
            log_info(f"  Step {i:>5}/{iterations}  elapsed {elapsed:.0f}s  ETA {eta:.0f}s")

    output_path.parent.mkdir(parents=True, exist_ok=True)
    save_geotiff(heightmap, output_path, reference_path=dem_path)
    log_info(f"Erosion complete → {output_path}")
    return output_path


def main() -> None:
    parser = argparse.ArgumentParser(description="Mahlanya geomorphological erosion pipeline")
    parser.add_argument("--dem",       type=pathlib.Path, required=True)
    parser.add_argument("--hardness",  type=pathlib.Path, required=True)
    parser.add_argument("--output",    type=pathlib.Path, required=True)
    parser.add_argument("--iterations", type=int, default=5000)
    parser.add_argument("--no-gpu",    action="store_true")
    args = parser.parse_args()

    erode_terrain(
        dem_path=args.dem,
        hardness_path=args.hardness,
        output_path=args.output,
        iterations=args.iterations,
        use_gpu=not args.no_gpu,
    )


if __name__ == "__main__":
    main()
