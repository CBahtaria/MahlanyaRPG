"""Acoustic impulse response computation via image-source + Monte Carlo hybrid. Plan 6."""
import pathlib

MATERIAL_ABSORPTION = {
    "granite":       [0.02, 0.02, 0.03, 0.03, 0.04, 0.05, 0.05, 0.06],
    "thatch_grass":  [0.15, 0.25, 0.40, 0.55, 0.65, 0.70, 0.72, 0.70],
    "clay_earth":    [0.35, 0.40, 0.45, 0.50, 0.55, 0.55, 0.60, 0.60],
    "dry_grass":     [0.25, 0.35, 0.45, 0.55, 0.60, 0.65, 0.65, 0.65],
    "water_surface": [0.01, 0.01, 0.02, 0.02, 0.03, 0.03, 0.05, 0.05],
    "open_sky":      [1.00, 1.00, 1.00, 1.00, 1.00, 1.00, 1.00, 1.00],
}
ENVIRONMENT_ARCHETYPES = ["granite_cave","thatched_hut_int","lubombo_canyon","open_highveld","usuthu_gorge","riverbed_floodplain"]

def compute_ir_image_source(geometry: dict, source_pos: tuple, receiver_pos: tuple, max_order: int = 3) -> list:
    raise NotImplementedError("Implemented in Plan 6 — audio pipeline")

def compute_ir_monte_carlo(geometry: dict, source_pos: tuple, receiver_pos: tuple, n_rays: int = 10000, max_bounces: int = 20) -> list:
    raise NotImplementedError("Implemented in Plan 6 — audio pipeline")

def export_ir_wav(reflections: list, output_path: pathlib.Path, sample_rate: int = 48000) -> pathlib.Path:
    raise NotImplementedError("Implemented in Plan 6")
