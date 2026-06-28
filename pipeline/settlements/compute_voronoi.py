"""Constrained Centroidal Voronoi settlement generator. Umuti homestead layout."""
import json
import math
import pathlib

import numpy as np

from pipeline.compute.libmahlanya_compute import lloyd_relax


class SwaziSettlementGenerator:
    """Constrained Voronoi tessellation for Umuti homestead layout.

    Social geometry: Sibaya at origin, Chief apex due East, wife seniority
    encodes radial distance d_i = d_base*(1/rank_i**0.6), taboo arc 240-300° West.
    """

    TABOO_ARC_DEG = (240, 300)
    GATE_DIRECTION = np.array([0, -1])
    APEX_DIRECTION = np.array([1, 0])

    # Base radial distance for the first wife from Sibaya
    _D_BASE = 0.15

    def lloyd_relaxation(self, points: np.ndarray, iterations: int = 50) -> np.ndarray:
        """Run Lloyd relaxation via the Zig kernel with the taboo arc excluded.

        Parameters
        ----------
        points:
            (N, 2) array of [x, y] coordinates in [0, 1]².
        iterations:
            Number of Lloyd iterations.

        Returns
        -------
        np.ndarray
            Relaxed (N, 2) float64 array.
        """
        pts = np.asarray(points, dtype=np.float64)
        if pts.ndim != 2 or pts.shape[1] != 2:
            raise ValueError(f"points must be (N, 2), got {pts.shape}")
        return lloyd_relax(
            pts,
            iters=iterations,
            taboo_start=float(self.TABOO_ARC_DEG[0]),
            taboo_end=float(self.TABOO_ARC_DEG[1]),
        )

    def generate(
        self,
        n_wives: int,
        n_sons: int,
        n_dependents: int,
        cattle_count: int,
        terrain_gradient: "np.ndarray",
        seed: int = 0,
    ) -> dict:
        """Generate a single Umuti homestead layout.

        Social geometry rules
        ---------------------
        * Sibaya (cattle kraal) – centre of the homestead at [0.5, 0.5].
        * Chief's hut – apex due East: [0.5 + 0.3, 0.5].
        * Wife huts  – arranged radially East of Sibaya.
          Distance from Sibaya for wife of rank *i* (1-based):
          ``d_i = 0.15 * (1 / i ** 0.6)``
          The arc spans East (0°) ± 60° so as to stay clear of the taboo arc.
        * Son huts   – random positions avoiding the taboo arc (240–300°).
        * Dependent huts – fill the remaining space randomly.

        After placing all points the layout is refined with Lloyd relaxation
        (50 iterations, taboo arc excluded by the Zig kernel).

        Parameters
        ----------
        n_wives, n_sons, n_dependents:
            Number of people in each social category.
        cattle_count:
            Herd size (stored as metadata; influences gate placement concept).
        terrain_gradient:
            Not currently used for point placement but stored for downstream
            slope-aware adjustments.
        seed:
            RNG seed for reproducible layouts.

        Returns
        -------
        dict with keys:
            positions  – list of dicts, each with 'x', 'y', 'role'
            cattle_count
            n_wives
            n_sons
            taboo_arc  – tuple (240, 300)
            seed
        """
        rng = np.random.default_rng(seed)

        points = []   # list of [x, y]
        roles = []    # matching role labels

        # --- Sibaya (cattle kraal) ---
        sibaya = np.array([0.5, 0.5])
        points.append(sibaya.copy())
        roles.append("sibaya")

        # --- Chief's hut (apex due East) ---
        chief = sibaya + self.APEX_DIRECTION * 0.3
        points.append(chief.copy())
        roles.append("chief")

        # --- Wife huts (radially East of Sibaya, rank-ordered) ---
        # Arc centred on East (0°), spanning ±60°.  Taboo arc is 240-300° West
        # so this is well clear of it.
        wife_arc_centre_deg = 0.0   # East
        wife_arc_half_deg = 60.0
        for rank in range(1, n_wives + 1):
            d = self._D_BASE * (1.0 / rank ** 0.6)
            # Spread wives evenly across the arc
            if n_wives == 1:
                angle_deg = wife_arc_centre_deg
            else:
                angle_deg = (
                    wife_arc_centre_deg
                    - wife_arc_half_deg
                    + (2 * wife_arc_half_deg) * (rank - 1) / (n_wives - 1)
                )
            angle_rad = math.radians(angle_deg)
            # Convention: 0° = East (+x), 90° = North (+y)
            wx = sibaya[0] + d * math.cos(angle_rad)
            wy = sibaya[1] + d * math.sin(angle_rad)
            points.append(np.array([np.clip(wx, 0.0, 1.0), np.clip(wy, 0.0, 1.0)]))
            roles.append(f"wife_{rank}")

        # --- Son huts (random, taboo arc excluded) ---
        taboo_start, taboo_end = self.TABOO_ARC_DEG
        sons_placed = 0
        attempts = 0
        while sons_placed < n_sons and attempts < n_sons * 200:
            attempts += 1
            x = rng.uniform(0.05, 0.95)
            y = rng.uniform(0.05, 0.95)
            bx, by = x - 0.5, y - 0.5
            if abs(bx) > 1e-9 or abs(by) > 1e-9:
                bearing = math.degrees(math.atan2(bx, by)) % 360
                if taboo_start <= bearing <= taboo_end:
                    continue
            points.append(np.array([x, y]))
            roles.append(f"son_{sons_placed + 1}")
            sons_placed += 1

        # --- Dependent huts (fill remaining space randomly) ---
        deps_placed = 0
        attempts = 0
        while deps_placed < n_dependents and attempts < n_dependents * 200:
            attempts += 1
            x = rng.uniform(0.05, 0.95)
            y = rng.uniform(0.05, 0.95)
            bx, by = x - 0.5, y - 0.5
            if abs(bx) > 1e-9 or abs(by) > 1e-9:
                bearing = math.degrees(math.atan2(bx, by)) % 360
                if taboo_start <= bearing <= taboo_end:
                    continue
            points.append(np.array([x, y]))
            roles.append(f"dependent_{deps_placed + 1}")
            deps_placed += 1

        # --- Lloyd relaxation ---
        pts_array = np.array(points, dtype=np.float64)
        relaxed = self.lloyd_relaxation(pts_array, iterations=50)

        # Build output positions list
        positions = [
            {"x": float(relaxed[i, 0]), "y": float(relaxed[i, 1]), "role": roles[i]}
            for i in range(len(roles))
        ]

        return {
            "positions": positions,
            "cattle_count": cattle_count,
            "n_wives": n_wives,
            "n_sons": n_sons,
            "taboo_arc": self.TABOO_ARC_DEG,
            "seed": seed,
        }


def export_settlements(settlements: list, output_dir: pathlib.Path) -> pathlib.Path:
    """Write each settlement dict to ``output_dir/settlement_NNN.json``.

    Parameters
    ----------
    settlements:
        List of dicts returned by :meth:`SwaziSettlementGenerator.generate`.
    output_dir:
        Directory to write JSON files into (created if it does not exist).

    Returns
    -------
    pathlib.Path
        The *output_dir* path.
    """
    output_dir = pathlib.Path(output_dir)
    output_dir.mkdir(parents=True, exist_ok=True)

    for i, settlement in enumerate(settlements):
        out_path = output_dir / f"settlement_{i:03d}.json"
        with open(out_path, "w", encoding="utf-8") as fh:
            json.dump(settlement, fh, indent=2)

    return output_dir
