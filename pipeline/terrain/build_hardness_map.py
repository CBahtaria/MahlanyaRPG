"""Geological rock hardness rasterisation. Implemented in Plan 1."""
import pathlib

def build_hardness_map(dem_path: pathlib.Path, geology_vector_path: pathlib.Path, output_path: pathlib.Path) -> pathlib.Path:
    """Rasterise geological survey data to a hardness coefficient raster (float32, 0–1).
    Hardness values: granite=0.002, Karoo sedimentary=0.08, rhyolite=0.015, alluvial=0.12."""
    raise NotImplementedError("Implemented in Plan 1 — terrain pipeline")
