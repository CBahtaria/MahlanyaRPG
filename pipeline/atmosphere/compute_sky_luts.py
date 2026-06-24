"""Hosek-Wilkie spectral sky LUT computation at 26°S latitude. Plan 5."""
import pathlib

def compute_sky_luts(latitude_deg: float, output_dir: pathlib.Path, sun_elevation_steps: int = 91,
                     turbidity_min: float = 1.5, turbidity_max: float = 6.0,
                     turbidity_steps: int = 10, wavelength_bands: int = 16) -> tuple:
    raise NotImplementedError("Implemented in Plan 5 — atmosphere pipeline")

def export_lunar_calendar(year_start: int, year_end: int, output_path: pathlib.Path) -> pathlib.Path:
    raise NotImplementedError("Implemented in Plan 5")

def export_star_field(latitude_deg: float, epoch_year: float, output_path: pathlib.Path, min_magnitude: float = 6.5) -> pathlib.Path:
    raise NotImplementedError("Implemented in Plan 5")
