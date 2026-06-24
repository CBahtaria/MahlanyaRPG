"""Export eroded heightmap to UE5 Landscape format. Implemented in Plan 1."""
import pathlib

def export_to_ue5(eroded_dem_path: pathlib.Path, weight_maps_dir: pathlib.Path, output_dir: pathlib.Path, tile_size: int = 1009) -> pathlib.Path:
    """Split heightmap into 1009x1009 UE5 Landscape tiles (.r16). tile_size must be 2^n+1."""
    raise NotImplementedError("Implemented in Plan 1 — terrain pipeline")
