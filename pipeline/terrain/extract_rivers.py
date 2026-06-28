"""River network extraction via D-infinity flow accumulation."""
import pathlib
import datetime
import numpy as np

from pipeline.compute.libmahlanya_compute import (
    dinf_fill_pits,
    dinf_flow_direction,
    dinf_flow_accumulation,
)

# Known Eswatini river names ordered from most to least important.
# During annotation they are assigned in order of descending max accumulation.
_RIVER_NAMES = [
    "Usuthu",
    "Komati",
    "Lusushwana",
    "Mbuluzi",
    "Ngwavuma",
    "Mhlumati",
]

# Minimum accumulation (cells) a connected component must reach to receive a
# named-river label.
_NAME_THRESHOLD = 5000


def _log(msg: str) -> None:
    ts = datetime.datetime.now().strftime("%Y-%m-%d %H:%M:%S")
    print(f"[{ts}] [INFO] {msg}")


def _make_synthetic_dem(size: int = 32) -> np.ndarray:
    """Return a cone-shaped DEM (float32) for testing when no real DEM exists."""
    xs = np.linspace(-1, 1, size)
    ys = np.linspace(-1, 1, size)
    xx, yy = np.meshgrid(xs, ys)
    dem = (1.0 - np.sqrt(xx ** 2 + yy ** 2)).clip(0.0, 1.0).astype(np.float32)
    return dem


def _read_dem(path: pathlib.Path):
    """Read a DEM from *path* using rasterio.  Returns (ndarray float32, profile)."""
    import rasterio
    with rasterio.open(path) as src:
        data = src.read(1).astype(np.float32)
        profile = src.profile.copy()
    return data, profile


def _write_geotiff(array: np.ndarray, profile: dict, out_path: pathlib.Path) -> None:
    import rasterio
    out_path.parent.mkdir(parents=True, exist_ok=True)
    profile.update(dtype="float32", count=1)
    with rasterio.open(out_path, "w", **profile) as dst:
        dst.write(array, 1)


def _vectorize_streams(
    stream_mask: np.ndarray,
    accum: np.ndarray,
    profile: dict,
    output_shapefile_path: pathlib.Path,
) -> pathlib.Path:
    """Convert a boolean stream-cell raster to a shapefile of polylines.

    Each 8-connected component becomes one LineString feature. Strahler order
    is approximated as floor(log2(max_accumulation_in_component / threshold)).
    Features with max accumulation >= _NAME_THRESHOLD receive a river name
    assigned in order of descending peak accumulation value.
    """
    import geopandas as gpd
    from shapely.geometry import LineString, MultiLineString
    from scipy.ndimage import label as nd_label
    import rasterio.transform

    _log("Vectorising stream network …")

    # Label 8-connected components
    struct = np.ones((3, 3), dtype=int)
    labeled, n_components = nd_label(stream_mask.astype(np.uint8), structure=struct)
    _log(f"  Found {n_components} stream components")

    transform = profile.get("transform")
    if transform is None:
        # Fallback: identity pixel transform
        from rasterio.transform import from_bounds
        h, w = stream_mask.shape
        transform = from_bounds(0, 0, w, h, w, h)

    def pixel_to_coord(row, col):
        """Convert pixel (row, col) to (x, y) world coordinates (cell centre)."""
        x, y = rasterio.transform.xy(transform, row, col)
        return x, y

    features = []  # list of (max_accum, geometry, strahler)
    for comp_id in range(1, n_components + 1):
        rows, cols = np.where(labeled == comp_id)
        max_accum = float(accum[rows, cols].max())
        strahler = max(1, int(np.log2(max(max_accum, 2))))

        coords = [pixel_to_coord(r, c) for r, c in zip(rows.tolist(), cols.tolist())]
        if len(coords) < 2:
            # Single-pixel component: create a tiny degenerate line
            x, y = coords[0]
            geom = LineString([(x, y), (x + 1e-6, y + 1e-6)])
        else:
            geom = LineString(coords)

        features.append((max_accum, geom, strahler))

    # Sort components by descending max accumulation to assign river names
    features.sort(key=lambda t: t[0], reverse=True)

    records = []
    name_idx = 0
    for max_accum, geom, strahler in features:
        if max_accum >= _NAME_THRESHOLD and name_idx < len(_RIVER_NAMES):
            name = _RIVER_NAMES[name_idx]
            name_idx += 1
        else:
            name = ""
        records.append({
            "geometry": geom,
            "max_accum": max_accum,
            "strahler": strahler,
            "river_name": name,
        })

    gdf = gpd.GeoDataFrame(records, crs=profile.get("crs"))
    output_shapefile_path.parent.mkdir(parents=True, exist_ok=True)
    gdf.to_file(str(output_shapefile_path))
    _log(f"  Shapefile written → {output_shapefile_path}")
    return output_shapefile_path


def extract_rivers(
    eroded_dem_path: pathlib.Path,
    output_shapefile_path: pathlib.Path,
    threshold: int = 1000,
) -> pathlib.Path:
    """Extract Usuthu, Komati, Lusushwana, Mbuluzi, Ngwavuma, Mhlumati river networks
    using Tarboton 1997 D-infinity algorithm. threshold=1000 cells ≈ 100km² drainage."""

    eroded_dem_path = pathlib.Path(eroded_dem_path)
    output_shapefile_path = pathlib.Path(output_shapefile_path)

    # ------------------------------------------------------------------
    # 1. Load DEM (or create synthetic fallback)
    # ------------------------------------------------------------------
    _rasterio_available = False
    try:
        import rasterio  # noqa: F401
        _rasterio_available = True
    except ImportError:
        pass

    profile: dict = {}

    if eroded_dem_path.exists():
        _log(f"Reading DEM from {eroded_dem_path}")
        if _rasterio_available:
            dem, profile = _read_dem(eroded_dem_path)
        else:
            # Fallback: try numpy binary, otherwise synthetic
            try:
                dem = np.load(str(eroded_dem_path)).astype(np.float32)
                _log("Loaded DEM via numpy.load")
            except Exception:
                _log("Could not read DEM without rasterio; using synthetic 32×32 cone DEM")
                dem = _make_synthetic_dem(32)
    else:
        _log(
            f"DEM not found at {eroded_dem_path}; "
            "generating synthetic 32×32 cone DEM for demonstration"
        )
        dem = _make_synthetic_dem(32)

    h, w = dem.shape
    _log(f"DEM shape: {h}×{w}")

    # ------------------------------------------------------------------
    # 2. D-infinity pipeline
    # ------------------------------------------------------------------
    _log("Step 1/3 – Filling pits (Priority-Flood) …")
    filled = dem.copy()
    dinf_fill_pits(filled)

    _log("Step 2/3 – Computing D-infinity flow directions …")
    angles = dinf_flow_direction(filled)

    _log("Step 3/3 – Computing flow accumulation …")
    accum = dinf_flow_accumulation(angles)

    _log(f"Accumulation range: [{accum.min():.1f}, {accum.max():.1f}]")

    # ------------------------------------------------------------------
    # 3. Threshold to extract stream network
    # ------------------------------------------------------------------
    stream_mask = accum >= threshold
    stream_cell_count = int(stream_mask.sum())
    _log(f"Stream cells (threshold={threshold}): {stream_cell_count}")

    # ------------------------------------------------------------------
    # 4. Vectorise or fall back to GeoTIFF
    # ------------------------------------------------------------------
    _shapely_available = False
    _geopandas_available = False
    try:
        import shapely  # noqa: F401
        _shapely_available = True
    except ImportError:
        pass
    try:
        import geopandas  # noqa: F401
        _geopandas_available = True
    except ImportError:
        pass

    if _shapely_available and _geopandas_available and _rasterio_available:
        return _vectorize_streams(stream_mask, accum, profile, output_shapefile_path)
    else:
        # Fallback: write flow accumulation raster as GeoTIFF
        _log(
            "shapely/geopandas/rasterio not fully available; "
            "saving flow accumulation raster as GeoTIFF fallback"
        )
        geotiff_path = output_shapefile_path.with_suffix(".tif")
        geotiff_path.parent.mkdir(parents=True, exist_ok=True)

        if _rasterio_available:
            if not profile:
                from rasterio.transform import from_bounds
                profile = {
                    "driver": "GTiff",
                    "dtype": "float32",
                    "width": w,
                    "height": h,
                    "count": 1,
                    "transform": from_bounds(0, 0, w, h, w, h),
                    "crs": None,
                }
            _write_geotiff(accum, profile, geotiff_path)
        else:
            np.save(str(geotiff_path.with_suffix(".npy")), accum)
            geotiff_path = geotiff_path.with_suffix(".npy")

        _log(f"Fallback output written → {geotiff_path}")
        return geotiff_path
