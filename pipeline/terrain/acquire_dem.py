"""DEM acquisition from Copernicus GLO-10. Implemented in Plan 1."""
import pathlib

def acquire_dem(bbox_west: float, bbox_east: float, bbox_south: float, bbox_north: float, output_dir: pathlib.Path) -> pathlib.Path:
    """Download and mosaic Copernicus DEM GLO-10 tiles for Eswatini bounding box.
    Returns path to output GeoTIFF (UTM Zone 36S / EPSG:32736)."""
    raise NotImplementedError("Implemented in Plan 1 — terrain pipeline")
