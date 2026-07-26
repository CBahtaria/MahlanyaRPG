"""Wave Function Collapse tile generator for Eswatini biome maps.

Produces geographically valid grid layouts based on Eswatini's four
geographic zones: Highveld → Middleveld → Lowveld/Lubombo east-to-west.
"""
from __future__ import annotations
import collections
import enum
import random
from typing import Iterator


class Tile(enum.Enum):
    HIGHVELD = "highveld"
    MIDDLEVELD = "middleveld"
    LOWVELD = "lowveld"
    LUBOMBO = "lubombo"
    FOREST = "forest"
    SAVANNA = "savanna"
    RIVER = "river"


# ADJACENCY maps each tile to the set of tiles that may border it.
# Rules encode real Eswatini geography:
#   Highveld (west highlands) → Middleveld (transitional) → Lowveld / Lubombo (east).
#   Rivers cannot form on the Lubombo escarpment.
ADJACENCY: dict[Tile, frozenset[Tile]] = {
    Tile.HIGHVELD: frozenset({
        Tile.HIGHVELD,
        Tile.MIDDLEVELD,
        Tile.FOREST,
        Tile.RIVER,
    }),
    Tile.MIDDLEVELD: frozenset({
        Tile.HIGHVELD,
        Tile.MIDDLEVELD,
        Tile.LOWVELD,
        Tile.LUBOMBO,
        Tile.FOREST,
        Tile.RIVER,
        Tile.SAVANNA,
    }),
    Tile.LOWVELD: frozenset({
        Tile.MIDDLEVELD,
        Tile.LOWVELD,
        Tile.LUBOMBO,
        Tile.SAVANNA,
        Tile.RIVER,
    }),
    Tile.LUBOMBO: frozenset({
        Tile.MIDDLEVELD,
        Tile.LOWVELD,
        Tile.LUBOMBO,
        Tile.SAVANNA,
    }),
    Tile.FOREST: frozenset({
        Tile.HIGHVELD,
        Tile.MIDDLEVELD,
        Tile.FOREST,
        Tile.RIVER,
    }),
    Tile.SAVANNA: frozenset({
        Tile.MIDDLEVELD,
        Tile.LOWVELD,
        Tile.LUBOMBO,
        Tile.SAVANNA,
        Tile.RIVER,
    }),
    Tile.RIVER: frozenset({
        Tile.HIGHVELD,
        Tile.MIDDLEVELD,
        Tile.LOWVELD,
        Tile.FOREST,
        Tile.SAVANNA,
        Tile.RIVER,
        # Lubombo intentionally absent — rivers don't form on the escarpment.
    }),
}

_ALL_TILES: frozenset[Tile] = frozenset(Tile)


class WFCGrid:
    """Grid of cells, each holding a set of still-possible tiles (superposition).

    Attributes
    ----------
    width, height : int
    cells : list[list[set[Tile]]]  — cells[row][col]
    rng : random.Random
    """

    def __init__(self, width: int, height: int, seed: int = 0) -> None:
        self.width = width
        self.height = height
        self.seed = seed
        self.rng = random.Random(seed)
        # Every cell starts as the full superposition.
        self.cells: list[list[set[Tile]]] = [
            [set(_ALL_TILES) for _ in range(width)]
            for _ in range(height)
        ]

    # ------------------------------------------------------------------
    # Internal helpers
    # ------------------------------------------------------------------

    def _entropy(self, row: int, col: int) -> int:
        """Number of remaining possible tiles at (row, col)."""
        return len(self.cells[row][col])

    def _neighbors(self, row: int, col: int) -> Iterator[tuple[int, int]]:
        """Yield (r, c) for the four cardinal neighbors inside the grid."""
        for dr, dc in ((-1, 0), (1, 0), (0, -1), (0, 1)):
            nr, nc = row + dr, col + dc
            if 0 <= nr < self.height and 0 <= nc < self.width:
                yield nr, nc

    def _min_entropy_cell(self) -> tuple[int, int] | None:
        """Return (row, col) of uncollapsed cell with fewest options, or None if done.

        Cells with exactly 1 option are already collapsed — skip them.
        Raises ValueError on contradiction (any cell with 0 options).
        """
        best: tuple[int, int] | None = None
        best_entropy = len(_ALL_TILES) + 1

        for r in range(self.height):
            for c in range(self.width):
                e = self._entropy(r, c)
                if e == 0:
                    raise ValueError(
                        f"Contradiction: cell ({r}, {c}) has no valid tiles remaining."
                    )
                if e == 1:
                    continue  # already collapsed
                if e < best_entropy:
                    best_entropy = e
                    best = (r, c)

        return best  # None means every cell is collapsed

    def _collapse(self, row: int, col: int) -> None:
        """Collapse cell to a single randomly-chosen tile."""
        options = list(self.cells[row][col])
        chosen = self.rng.choice(options)
        self.cells[row][col] = {chosen}

    def _propagate(self, row: int, col: int) -> None:
        """BFS constraint propagation from (row, col).

        For each neighbor of a changed cell, removes tiles that are not
        allowed by ANY tile still possible in the source cell.  If a
        neighbor's option set shrinks, enqueue it.  Raises ValueError on
        contradiction (a cell reaches 0 options).
        """
        queue: collections.deque[tuple[int, int]] = collections.deque()
        queue.append((row, col))

        while queue:
            r, c = queue.popleft()
            source_options = self.cells[r][c]

            # Build the union of all tiles permitted by source options.
            allowed_for_neighbors: set[Tile] = set()
            for tile in source_options:
                allowed_for_neighbors |= ADJACENCY[tile]

            for nr, nc in self._neighbors(r, c):
                neighbor_options = self.cells[nr][nc]
                new_options = neighbor_options & allowed_for_neighbors
                if len(new_options) < len(neighbor_options):
                    if not new_options:
                        raise ValueError(
                            f"Contradiction: cell ({nr}, {nc}) reduced to 0 options "
                            f"while propagating from ({r}, {c})."
                        )
                    self.cells[nr][nc] = new_options
                    queue.append((nr, nc))

    # ------------------------------------------------------------------
    # Public API
    # ------------------------------------------------------------------

    def step(self) -> bool:
        """Collapse one cell and propagate. Returns True if the grid is fully collapsed."""
        target = self._min_entropy_cell()
        if target is None:
            return True  # already done
        r, c = target
        self._collapse(r, c)
        self._propagate(r, c)
        return self._min_entropy_cell() is None

    def generate(self) -> list[list[Tile]]:
        """Run until fully collapsed. Returns 2D grid of final Tiles.

        Raises ValueError on contradiction (caller should retry with a different seed).
        """
        while True:
            target = self._min_entropy_cell()
            if target is None:
                break
            r, c = target
            self._collapse(r, c)
            self._propagate(r, c)

        return [[next(iter(cell)) for cell in row] for row in self.cells]

    def to_dict(self) -> dict:
        """Serialize collapsed grid to a JSON-safe dict.

        Returns {"width": w, "height": h, "seed": seed, "tiles": [[tile.value, ...], ...]}
        """
        return {
            "width": self.width,
            "height": self.height,
            "seed": self.seed,
            "tiles": [
                [next(iter(cell)).value for cell in row]
                for row in self.cells
            ],
        }


def generate_biome_map(
    width: int,
    height: int,
    seed: int = 0,
    max_retries: int = 5,
) -> dict:
    """Generate a WFC biome map, retrying on contradiction.

    Parameters
    ----------
    width, height : int — grid dimensions
    seed : int — base seed (incremented on each retry)
    max_retries : int — raise RuntimeError if all retries fail

    Returns
    -------
    dict with keys: width, height, seed (actual seed used), tiles, retries_used
    """
    last_error: ValueError | None = None
    for attempt in range(max_retries + 1):
        current_seed = seed + attempt
        grid = WFCGrid(width, height, seed=current_seed)
        try:
            grid.generate()
            result = grid.to_dict()
            result["retries_used"] = attempt
            return result
        except ValueError as exc:
            last_error = exc

    raise RuntimeError(
        f"WFC failed after {max_retries + 1} attempts "
        f"(seeds {seed}–{seed + max_retries}). Last error: {last_error}"
    )
