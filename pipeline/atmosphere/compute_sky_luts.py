"""Hosek-Wilkie spectral sky LUT computation at 26°S latitude. Plan 5."""
import calendar
import json
import math
import pathlib


# ---------------------------------------------------------------------------
# Core Hosek-Wilkie approximation
# ---------------------------------------------------------------------------

def _hosek_wilkie_radiance(lambda_nm: float, sun_elev_deg: float,
                            turbidity: float, altitude_m: float) -> float:
    """Simplified Hosek-Wilkie normalised radiance [0,1].

    Physical effects: Rayleigh (λ^-4), Mie (turbidity-driven, λ^-1.3),
    solar elevation (sin^0.6), altitude density (exp(-alt/8500)).
    """
    rayleigh_density = math.exp(-altitude_m / 8500.0)
    rayleigh = rayleigh_density * (550.0 / lambda_nm) ** 4
    mie = (turbidity - 1.0) * 0.04 * (550.0 / lambda_nm) ** 1.3
    elevation_factor = math.sin(math.radians(max(sun_elev_deg, 0.0))) ** 0.6
    sky = elevation_factor * (rayleigh + mie)
    # Normalise against reference: T=1.5, θ=45°, λ=550nm, alt=0
    ref = (math.sin(math.radians(45.0)) ** 0.6) * (1.0 * 1.0 + 0.5 * 0.04)
    return min(sky / ref, 1.0)


# ---------------------------------------------------------------------------
# Lunar calendar
# ---------------------------------------------------------------------------

def _date_to_jd(year: int, month: int, day: int) -> float:
    a = (14 - month) // 12
    y = year + 4800 - a
    m = month + 12 * a - 3
    return (day + (153 * m + 2) // 5 + 365 * y
            + y // 4 - y // 100 + y // 400 - 32045)


def _compute_lunar_calendar(year_start: int, year_end: int) -> list:
    SYNODIC_MONTH = 29.53058867
    REFERENCE_JD = 2451550.1  # 2000-01-06.6 new moon
    PHASE_NAMES = [
        "New Moon", "Waxing Crescent", "First Quarter", "Waxing Gibbous",
        "Full Moon", "Waning Gibbous", "Last Quarter", "Waning Crescent",
    ]
    records = []
    for year in range(year_start, year_end + 1):
        for month in range(1, 13):
            for day in range(1, calendar.monthrange(year, month)[1] + 1):
                jd = _date_to_jd(year, month, day)
                phase = ((jd - REFERENCE_JD) % SYNODIC_MONTH) / SYNODIC_MONTH
                records.append({
                    "date": f"{year:04d}-{month:02d}-{day:02d}",
                    "phase": round(phase, 4),
                    "phase_name": PHASE_NAMES[int(phase * 8) % 8],
                })
    return records


# ---------------------------------------------------------------------------
# Star field
# ---------------------------------------------------------------------------

def _generate_star_field(latitude_deg: float, epoch_year: float,
                          min_magnitude: float) -> dict:
    """Procedurally generate a realistic star catalog visible at latitude_deg."""
    # Visibility: star rises above horizon if dec > -(90 - |lat|)
    horizon_limit = -(90.0 - abs(latitude_deg))

    seed = int(epoch_year * 10000) + int(abs(latitude_deg))

    def _lcg(s):
        s = (s * 1664525 + 1013904223) & 0xFFFFFFFF
        return s, s / 0xFFFFFFFF

    stars = []
    attempts = 0
    while len(stars) < 8000 and attempts < 200000:
        attempts += 1
        seed, r1 = _lcg(seed)
        seed, r2 = _lcg(seed)
        seed, r3 = _lcg(seed)

        ra = r1 * 360.0
        dec = math.degrees(math.asin(2 * r2 - 1))

        if dec < horizon_limit:
            continue

        mag = 2.0 + r3 ** 0.4 * 6.5
        if mag > min_magnitude + 2.0:
            continue

        stars.append({
            "ra_deg": round(ra, 4),
            "dec_deg": round(dec, 4),
            "magnitude": round(mag, 2),
        })

    return {
        "epoch": f"J{epoch_year:.1f}",
        "latitude_deg": latitude_deg,
        "count": len(stars),
        "stars": stars,
    }


# ---------------------------------------------------------------------------
# Public API
# ---------------------------------------------------------------------------

def compute_sky_luts(
    latitude_deg: float,
    output_dir: pathlib.Path,
    sun_elevation_steps: int = 91,
    turbidity_min: float = 1.5,
    turbidity_max: float = 6.0,
    turbidity_steps: int = 10,
    wavelength_bands: int = 16,
) -> tuple:
    """Compute Hosek-Wilkie sky radiance LUTs and ancillary data files.

    Outputs (in output_dir):
        sky_lut_highveld.json  — 3D LUT: elevation × turbidity × wavelength
        sky_lut_lowveld.json
        lunar_calendar_1750_1910.json
        starfield_26S_J1850.json

    Returns:
        (highveld_path, lowveld_path) as pathlib.Path objects.
    """
    output_dir = pathlib.Path(output_dir)
    output_dir.mkdir(parents=True, exist_ok=True)

    sun_elevations = [i * (90 / max(sun_elevation_steps - 1, 1))
                      for i in range(sun_elevation_steps)]
    turbidities = [turbidity_min + i * (turbidity_max - turbidity_min) / max(turbidity_steps - 1, 1)
                   for i in range(turbidity_steps)]
    wavelengths = [380 + i * (340 // max(wavelength_bands - 1, 1))
                   for i in range(wavelength_bands)]

    altitude_bands = {"highveld": 1700.0, "lowveld": 250.0}
    out_paths = {}

    for band_name, altitude_m in altitude_bands.items():
        lut = []
        for elev in sun_elevations:
            elev_row = []
            for turb in turbidities:
                wave_row = [
                    round(_hosek_wilkie_radiance(lam, elev, turb, altitude_m), 6)
                    for lam in wavelengths
                ]
                elev_row.append(wave_row)
            lut.append(elev_row)

        path = output_dir / f"sky_lut_{band_name}.json"
        with open(path, "w", encoding="utf-8") as fh:
            json.dump({
                "sun_elevations": [round(e, 4) for e in sun_elevations],
                "turbidities": [round(t, 4) for t in turbidities],
                "wavelengths_nm": wavelengths,
                "altitude_m": altitude_m,
                "latitude_deg": latitude_deg,
                "radiance": lut,
            }, fh)
        out_paths[band_name] = path

    # Lunar calendar 1750–1910
    lunar_path = output_dir / "lunar_calendar_1750_1910.json"
    with open(lunar_path, "w", encoding="utf-8") as fh:
        json.dump(_compute_lunar_calendar(1750, 1910), fh)

    # Star field at 26°S in J1850
    star_path = output_dir / "starfield_26S_J1850.json"
    with open(star_path, "w", encoding="utf-8") as fh:
        json.dump(_generate_star_field(latitude_deg, 1850.0, min_magnitude=6.5), fh)

    return out_paths["highveld"], out_paths["lowveld"]


def export_lunar_calendar(
    year_start: int,
    year_end: int,
    output_path: pathlib.Path,
) -> pathlib.Path:
    """Write lunar phase data for every day in [year_start, year_end]."""
    output_path = pathlib.Path(output_path)
    output_path.parent.mkdir(parents=True, exist_ok=True)
    with open(output_path, "w", encoding="utf-8") as fh:
        json.dump(_compute_lunar_calendar(year_start, year_end), fh)
    return output_path


def export_star_field(
    latitude_deg: float,
    epoch_year: float,
    output_path: pathlib.Path,
    min_magnitude: float = 6.5,
) -> pathlib.Path:
    """Write a star catalog visible at latitude_deg in the given epoch."""
    output_path = pathlib.Path(output_path)
    output_path.parent.mkdir(parents=True, exist_ok=True)
    with open(output_path, "w", encoding="utf-8") as fh:
        json.dump(_generate_star_field(latitude_deg, epoch_year, min_magnitude), fh)
    return output_path
