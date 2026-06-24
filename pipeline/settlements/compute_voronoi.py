"""Constrained Centroidal Voronoi settlement generator. Implemented in Plan 4."""
import pathlib, numpy as np

class SwaziSettlementGenerator:
    """Constrained Voronoi tessellation for Umuti homestead layout.
    Social geometry: Sibaya at origin, Chief apex due East, wife seniority
    encodes radial distance d_i = d_base*(1/rank_i**0.6), taboo arc 240-300° West."""
    TABOO_ARC_DEG = (240, 300)
    GATE_DIRECTION = np.array([0, -1])
    APEX_DIRECTION = np.array([1, 0])

    def generate(self, n_wives: int, n_sons: int, n_dependents: int,
                 cattle_count: int, terrain_gradient: "np.ndarray", seed: int = 0) -> dict:
        raise NotImplementedError("Implemented in Plan 4 — settlement pipeline")

    def lloyd_relaxation(self, points, iterations: int = 50):
        raise NotImplementedError("Implemented in Plan 4")

def export_settlements(settlements: list, output_dir: pathlib.Path) -> pathlib.Path:
    raise NotImplementedError("Implemented in Plan 4 — settlement pipeline")
