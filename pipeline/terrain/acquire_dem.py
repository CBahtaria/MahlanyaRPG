"""DEM acquisition from Copernicus GLO-10 / GLO-30 via OpenTopography or AWS S3."""

import argparse
import math
import os
import pathlib
import shutil
import subprocess
import urllib.error
import urllib.parse
import urllib.request

ESWATINI_BBOX = (30.79, 32.14, -27.32, -25.72)  # west, east, south, north
ESWATINI_UTM_EPSG = 32736
TARGET_RESOLUTION_M = 10  # metres/pixel for reprojection

_OPENTOPO_ENDPOINT = "https://portal.opentopography.org/API/globaldem"
_AWS_GLO30_BASE = "https://copernicus-dem-30m.s3.amazonaws.com"


def _require_gdal() -> None:
    """Raise ImportError with install hint if gdalwarp is not on PATH."""
    if shutil.which("gdalwarp") is None:
        raise ImportError(
            "gdalwarp not found on PATH. "
            "Install GDAL: sudo apt install gdal-bin  # Debian/Ubuntu\n"
            "                conda install -c conda-forge gdal  # Conda\n"
            "                brew install gdal               # macOS"
        )


def _compute_tile_names(
    bbox_west: float,
    bbox_east: float,
    bbox_south: float,
    bbox_north: float,
) -> list[str]:
    """Return Copernicus DEM tile names covering the supplied bounding box.

    Tiles are 1°×1° in WGS84.  The tile name encodes the *south-west corner*
    of each cell using the Copernicus COG naming convention:

        Copernicus_DSM_COG_10_{NS}{lat:02d}_00_{EW}{lon:03d}_00_DEM

    For a point at latitude L the owning tile corner is floor(L), so the
    inclusive range of corners is  floor(south) … floor(north).  When north
    is an exact integer the uppermost row already lies in the next tile, so we
    stop at floor(north) − 1 in that case — but floor() handles this naturally
    because a point exactly on the boundary belongs to the tile whose SW corner
    equals that value.
    """
    lat_min = math.floor(bbox_south)
    # If north is exact integer, points at north sit on that tile's SW corner.
    lat_max = math.floor(bbox_north) if bbox_north != math.floor(bbox_north) else int(bbox_north) - 1

    lon_min = math.floor(bbox_west)
    lon_max = math.floor(bbox_east) if bbox_east != math.floor(bbox_east) else int(bbox_east) - 1

    names: list[str] = []
    for lat in range(lat_min, lat_max + 1):
        ns = "N" if lat >= 0 else "S"
        abs_lat = abs(lat)
        for lon in range(lon_min, lon_max + 1):
            ew = "E" if lon >= 0 else "W"
            abs_lon = abs(lon)
            names.append(
                f"Copernicus_DSM_COG_10_{ns}{abs_lat:02d}_00_{ew}{abs_lon:03d}_00_DEM"
            )
    return names


def _download_via_opentopo(
    bbox_west: float,
    bbox_east: float,
    bbox_south: float,
    bbox_north: float,
    api_key: str,
    tiles_dir: pathlib.Path,
    demtype: str = "COP30",
) -> pathlib.Path:
    """Download the bounding-box mosaic from OpenTopography and save to tiles_dir."""
    params = {
        "demtype": demtype,
        "south": bbox_south,
        "north": bbox_north,
        "west": bbox_west,
        "east": bbox_east,
        "outputFormat": "GTiff",
        "API_Key": api_key,
    }
    url = _OPENTOPO_ENDPOINT + "?" + urllib.parse.urlencode(params)
    out_path = tiles_dir / f"opentopo_{demtype}.tif"
    print(f"  Downloading from OpenTopography ({demtype})…")
    with urllib.request.urlopen(url) as resp, open(out_path, "wb") as fh:
        shutil.copyfileobj(resp, fh)
    return out_path


def _download_tile_aws(tile_name: str, tiles_dir: pathlib.Path) -> pathlib.Path | None:
    """Download a single GLO-30 tile from AWS S3.  Returns None on 404."""
    url = f"{_AWS_GLO30_BASE}/{tile_name}/{tile_name}.tif"
    out_path = tiles_dir / f"{tile_name}.tif"
    if out_path.exists():
        return out_path
    print(f"  Downloading tile {tile_name} from AWS S3…")
    try:
        with urllib.request.urlopen(url) as resp, open(out_path, "wb") as fh:
            shutil.copyfileobj(resp, fh)
        return out_path
    except urllib.error.HTTPError as exc:
        if exc.code == 404:
            print(f"  Tile {tile_name} not found on S3 (ocean / no-data) — skipping.")
            return None
        raise


def _run(cmd: list[str]) -> None:
    """Run a subprocess command, raising RuntimeError on failure."""
    result = subprocess.run(cmd, capture_output=True, text=True)
    if result.returncode != 0:
        raise RuntimeError(
            f"Command failed: {' '.join(cmd)}\n"
            f"stdout: {result.stdout}\n"
            f"stderr: {result.stderr}"
        )


def acquire_dem(
    bbox_west: float,
    bbox_east: float,
    bbox_south: float,
    bbox_north: float,
    output_dir: pathlib.Path,
) -> pathlib.Path:
    """Download and mosaic Copernicus DEM GLO-10 tiles for Eswatini bounding box.
    Returns path to output GeoTIFF (UTM Zone 36S / EPSG:32736)."""

    output_dir = pathlib.Path(output_dir)
    out_utm = output_dir / "dem_utm36s.tif"

    # Short-circuit: skip download if output already exists (--local-file mode).
    if out_utm.exists():
        print(f"Output already exists, skipping download: {out_utm}")
        return out_utm

    _require_gdal()

    tiles_dir = output_dir / "tiles"
    tiles_dir.mkdir(parents=True, exist_ok=True)
    output_dir.mkdir(parents=True, exist_ok=True)

    dem_wgs84 = output_dir / "dem_wgs84.tif"
    tile_paths: list[pathlib.Path] = []

    api_key = os.environ.get("OPENTOPOGRAPHY_API_KEY", "")

    if api_key:
        # Try COP10 first, fall back to COP30 if the server rejects it.
        try:
            tile_paths.append(
                _download_via_opentopo(
                    bbox_west, bbox_east, bbox_south, bbox_north, api_key, tiles_dir, demtype="COP10"
                )
            )
        except urllib.error.HTTPError as exc:
            print(f"  COP10 request failed ({exc.code}), falling back to COP30.")
            tile_paths.append(
                _download_via_opentopo(
                    bbox_west, bbox_east, bbox_south, bbox_north, api_key, tiles_dir, demtype="COP30"
                )
            )
    else:
        # No API key — download individual GLO-30 tiles from AWS S3.
        print("OPENTOPOGRAPHY_API_KEY not set, falling back to AWS S3 GLO-30 tiles.")
        tile_names = _compute_tile_names(bbox_west, bbox_east, bbox_south, bbox_north)
        for name in tile_names:
            path = _download_tile_aws(name, tiles_dir)
            if path is not None:
                tile_paths.append(path)

    if not tile_paths:
        raise RuntimeError("No DEM tiles were downloaded. Check connectivity or API key.")

    # Mosaic tiles into a single WGS84 GeoTIFF.
    print("  Mosaicking tiles…")
    _run(
        ["gdalwarp", "-of", "GTiff"]
        + [str(p) for p in tile_paths]
        + [str(dem_wgs84)]
    )

    # Reproject to UTM Zone 36S at TARGET_RESOLUTION_M.
    print(f"  Reprojecting to EPSG:{ESWATINI_UTM_EPSG}…")
    _run(
        [
            "gdalwarp",
            "-t_srs", f"EPSG:{ESWATINI_UTM_EPSG}",
            "-tr", str(TARGET_RESOLUTION_M), str(TARGET_RESOLUTION_M),
            "-r", "bilinear",
            str(dem_wgs84),
            str(out_utm),
        ]
    )

    return out_utm


def _build_parser() -> argparse.ArgumentParser:
    p = argparse.ArgumentParser(
        description="Download and reproject Copernicus DEM for Eswatini (or any bbox)."
    )
    p.add_argument("--bbox-west",  type=float, default=ESWATINI_BBOX[0])
    p.add_argument("--bbox-east",  type=float, default=ESWATINI_BBOX[1])
    p.add_argument("--bbox-south", type=float, default=ESWATINI_BBOX[2])
    p.add_argument("--bbox-north", type=float, default=ESWATINI_BBOX[3])
    p.add_argument(
        "--output-dir",
        type=pathlib.Path,
        default=pathlib.Path("outputs/dem"),
        help="Directory for downloaded tiles and final GeoTIFF.",
    )
    return p


if __name__ == "__main__":
    args = _build_parser().parse_args()
    result = acquire_dem(
        args.bbox_west,
        args.bbox_east,
        args.bbox_south,
        args.bbox_north,
        args.output_dir,
    )
    print(f"DEM ready: {result}")
