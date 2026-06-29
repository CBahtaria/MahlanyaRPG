"""Acoustic impulse response computation via image-source + Monte Carlo hybrid. Plan 6."""
import json
import math
import pathlib
import struct

# Per-material absorption coefficients: 8 octave bands (125Hz–16kHz)
MATERIAL_ABSORPTION = {
    "granite":       [0.02, 0.02, 0.03, 0.03, 0.04, 0.05, 0.05, 0.06],
    "thatch_grass":  [0.15, 0.25, 0.40, 0.55, 0.65, 0.70, 0.72, 0.70],
    "clay_earth":    [0.35, 0.40, 0.45, 0.50, 0.55, 0.55, 0.60, 0.60],
    "dry_grass":     [0.25, 0.35, 0.45, 0.55, 0.60, 0.65, 0.65, 0.65],
    "water_surface": [0.01, 0.01, 0.02, 0.02, 0.03, 0.03, 0.05, 0.05],
    "open_sky":      [1.00, 1.00, 1.00, 1.00, 1.00, 1.00, 1.00, 1.00],
}

ENVIRONMENT_ARCHETYPES = [
    "granite_cave",
    "thatched_hut_int",
    "lubombo_canyon",
    "open_highveld",
    "usuthu_gorge",
    "riverbed_floodplain",
]

# RT60 references for each archetype (seconds) — from published literature
_ARCHETYPE_RT60 = {
    "granite_cave":        4.2,
    "thatched_hut_int":    0.15,
    "lubombo_canyon":      2.8,
    "open_highveld":       0.05,
    "usuthu_gorge":        1.6,
    "riverbed_floodplain": 0.25,
}

SPEED_OF_SOUND = 343.0  # m/s at 20°C


# ---------------------------------------------------------------------------
# Geometry helpers
# ---------------------------------------------------------------------------

def _dist(a, b):
    return math.sqrt(sum((x - y) ** 2 for x, y in zip(a, b)))


def _get_absorption(material_name: str) -> list:
    return MATERIAL_ABSORPTION.get(material_name, [0.1] * 8)


# ---------------------------------------------------------------------------
# Image source method (early reflections, orders 1–max_order)
# ---------------------------------------------------------------------------

def compute_ir_image_source(
    geometry: dict,
    source_pos: tuple,
    receiver_pos: tuple,
    max_order: int = 3,
) -> list:
    """Compute early-reflection IR via the image source method.

    Geometry dict expected keys:
        "dimensions": [Lx, Ly, Lz]  — room size in metres
        "materials": {"floor": str, "ceiling": str, "walls": str}
                     each value must be a key in MATERIAL_ABSORPTION

    Returns list of reflection dicts:
        {"delay_s": float, "energy": [float]*8, "order": int}
    Direct path (order 0) is always included.
    """
    dims = geometry.get("dimensions", [10.0, 5.0, 4.0])
    mats = geometry.get("materials", {})
    floor_abs  = _get_absorption(mats.get("floor",   "clay_earth"))
    ceil_abs   = _get_absorption(mats.get("ceiling", "thatch_grass"))
    wall_abs   = _get_absorption(mats.get("walls",   "granite"))

    Lx, Ly, Lz = dims
    sx, sy, sz = source_pos
    rx, ry, rz = receiver_pos

    reflections = []

    # Direct path
    d = _dist(source_pos, receiver_pos)
    energy = [1.0 / max(d, 0.01) ** 2] * 8
    reflections.append({"delay_s": d / SPEED_OF_SOUND, "energy": energy, "order": 0})

    # Image sources: iterate integer triplet (nx, ny, nz) for each order
    for order in range(1, max_order + 1):
        for nx in range(-order, order + 1):
            for ny in range(-order, order + 1):
                for nz in range(-order, order + 1):
                    if abs(nx) + abs(ny) + abs(nz) != order:
                        continue

                    # Reflect source in x walls (nx bounces)
                    imgx = sx + 2 * nx * Lx if nx % 2 == 0 else (2 * nx * Lx - sx)
                    imgy = sy + 2 * ny * Ly if ny % 2 == 0 else (2 * ny * Ly - sy)
                    imgz = sz + 2 * nz * Lz if nz % 2 == 0 else (2 * nz * Lz - sz)

                    d = _dist((imgx, imgy, imgz), receiver_pos)
                    if d < 0.001:
                        continue

                    # Per-band energy: inverse square law × absorption at each bounce
                    e = []
                    for b in range(8):
                        amp = 1.0 / max(d, 0.01) ** 2
                        amp *= (1.0 - floor_abs[b])  ** abs(nz)
                        amp *= (1.0 - ceil_abs[b])   ** abs(nz)
                        amp *= (1.0 - wall_abs[b])   ** (abs(nx) + abs(ny))
                        e.append(max(amp, 0.0))

                    reflections.append({
                        "delay_s": d / SPEED_OF_SOUND,
                        "energy": e,
                        "order": order,
                    })

    reflections.sort(key=lambda r: r["delay_s"])
    return reflections


# ---------------------------------------------------------------------------
# Monte Carlo path tracer (late reverberation)
# ---------------------------------------------------------------------------

def _lcg(seed: int):
    seed = (seed * 1664525 + 1013904223) & 0xFFFFFFFF
    return seed, seed / 0xFFFFFFFF


def compute_ir_monte_carlo(
    geometry: dict,
    source_pos: tuple,
    receiver_pos: tuple,
    n_rays: int = 10000,
    max_bounces: int = 20,
) -> list:
    """Late-reverberation IR via Monte Carlo path tracing.

    Fires n_rays from source_pos in random directions; records energy
    contributions at receiver_pos (within 1m capture radius).

    Returns same format as compute_ir_image_source.
    """
    dims = geometry.get("dimensions", [10.0, 5.0, 4.0])
    mats = geometry.get("materials", {})
    floor_abs = _get_absorption(mats.get("floor",   "clay_earth"))
    ceil_abs  = _get_absorption(mats.get("ceiling", "thatch_grass"))
    wall_abs  = _get_absorption(mats.get("walls",   "granite"))

    Lx, Ly, Lz = dims
    capture_radius = 1.0  # metres

    reflections = []
    seed = 42 + hash(str(source_pos))

    for ray_idx in range(n_rays):
        # Random direction (uniform sphere)
        seed, u = _lcg(seed + ray_idx * 3)
        seed, v = _lcg(seed)
        theta = math.acos(2 * u - 1)
        phi = 2 * math.pi * v
        dx = math.sin(theta) * math.cos(phi)
        dy = math.sin(theta) * math.sin(phi)
        dz = math.cos(theta)

        px, py, pz = source_pos
        energy = [1.0] * 8
        total_dist = 0.0

        for bounce in range(max_bounces):
            # Find intersection with nearest wall
            t_candidates = []
            if abs(dx) > 1e-9:
                t_candidates.append(((0 - px) / dx, "x0"))
                t_candidates.append(((Lx - px) / dx, "x1"))
            if abs(dy) > 1e-9:
                t_candidates.append(((0 - py) / dy, "y0"))
                t_candidates.append(((Ly - py) / dy, "y1"))
            if abs(dz) > 1e-9:
                t_candidates.append(((0 - pz) / dz, "z0"))
                t_candidates.append(((Lz - pz) / dz, "z1"))

            t_candidates = [(t, wall) for t, wall in t_candidates if t > 1e-6]
            if not t_candidates:
                break

            t_min, wall = min(t_candidates, key=lambda x: x[0])

            # Step to intersection
            nx_new = px + dx * t_min
            ny_new = py + dy * t_min
            nz_new = pz + dz * t_min
            total_dist += t_min

            # Check if ray passes near receiver
            dist_to_recv = _dist((nx_new, ny_new, nz_new), receiver_pos)
            if dist_to_recv < capture_radius:
                delay = total_dist / SPEED_OF_SOUND
                inv_sq = 1.0 / max(total_dist, 0.01) ** 2
                reflections.append({
                    "delay_s": delay,
                    "energy": [e * inv_sq for e in energy],
                    "order": bounce + 1,
                })

            # Reflect direction and apply absorption
            px, py, pz = nx_new, ny_new, nz_new
            if wall.startswith("z"):
                abs_coeff = floor_abs if pz < Lz / 2 else ceil_abs
                dz = -dz
            elif wall.startswith("x"):
                abs_coeff = wall_abs
                dx = -dx
            else:
                abs_coeff = wall_abs
                dy = -dy

            energy = [e * (1.0 - abs_coeff[b]) for b, e in enumerate(energy)]
            if all(e < 1e-6 for e in energy):
                break

    reflections.sort(key=lambda r: r["delay_s"])
    return reflections


# ---------------------------------------------------------------------------
# WAV export
# ---------------------------------------------------------------------------

def export_ir_wav(
    reflections: list,
    output_path: pathlib.Path,
    sample_rate: int = 48000,
) -> pathlib.Path:
    """Write the impulse response to a mono 16-bit WAV file.

    Sums all 8 frequency bands (unweighted) into a single mono channel.
    """
    output_path = pathlib.Path(output_path)
    output_path.parent.mkdir(parents=True, exist_ok=True)

    if not reflections:
        duration_s = 0.1
    else:
        duration_s = reflections[-1]["delay_s"] + 0.1

    n_samples = int(duration_s * sample_rate)
    buffer = [0.0] * n_samples

    for r in reflections:
        sample_idx = int(r["delay_s"] * sample_rate)
        if 0 <= sample_idx < n_samples:
            amplitude = sum(r["energy"]) / 8.0
            buffer[sample_idx] += amplitude

    # Normalise to [-1, 1]
    peak = max(abs(x) for x in buffer) if buffer else 1.0
    if peak > 0:
        buffer = [x / peak for x in buffer]

    # Write WAV (PCM 16-bit mono)
    with open(output_path, "wb") as fh:
        n_bytes = n_samples * 2  # 16-bit = 2 bytes/sample
        # RIFF header
        fh.write(b"RIFF")
        fh.write(struct.pack("<I", 36 + n_bytes))
        fh.write(b"WAVE")
        # fmt chunk
        fh.write(b"fmt ")
        fh.write(struct.pack("<IHHIIHH", 16, 1, 1, sample_rate,
                              sample_rate * 2, 2, 16))
        # data chunk
        fh.write(b"data")
        fh.write(struct.pack("<I", n_bytes))
        for s in buffer:
            fh.write(struct.pack("<h", int(s * 32767)))

    return output_path
