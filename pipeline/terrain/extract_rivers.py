"""River network extraction via D-infinity flow accumulation. Implemented in Plan 1."""
import pathlib

def extract_rivers(eroded_dem_path: pathlib.Path, output_shapefile_path: pathlib.Path, threshold: int = 1000) -> pathlib.Path:
    """Extract Usuthu, Komati, Lusushwana, Mbuluzi, Ngwavuma, Mhlumati river networks
    using Tarboton 1997 D-infinity algorithm. threshold=1000 cells ≈ 100km² drainage."""
    raise NotImplementedError("Implemented in Plan 1 — terrain pipeline")
