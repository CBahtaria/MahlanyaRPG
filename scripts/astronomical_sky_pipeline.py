#!/usr/bin/env python3
"""
Astronomical sky data pipeline for MahlanyaRPG.
Queries JPL Horizons (OBSERVER, QUANTITIES='4' = apparent Az/El) for
sun/moon/planet positions at Eswatini coordinates (lat -26.5225, lon 31.4659).
Snapshot time: 10:00 UT = 12:00 local noon (Eswatini UTC+2).
Outputs: docs/sky_data/{YYYY-MM-DD}.json (deterministic given same date)
Usage: python3 scripts/astronomical_sky_pipeline.py --date 1895-07-15
"""
import argparse
import json
import math
import os
import sys
import urllib.request
import urllib.parse
import urllib.error
from datetime import datetime, timedelta, timezone
from pathlib import Path

# Eswatini centroid (Mbabane area)
LAT = -26.5225
LON = 31.4659
ELEVATION_M = 1243  # Mbabane elevation

HORIZONS_URL = "https://ssd.jpl.nasa.gov/api/horizons.api"

BODIES = {
    "Sun":     "10",
    "Moon":    "301",
    "Mars":    "499",
    "Jupiter": "599",
    "Saturn":  "699",
    "Venus":   "299",
    "Mercury": "199",
}

# Repo root is two levels up from this script
REPO_ROOT = Path(__file__).resolve().parent.parent
OUTPUT_DIR = REPO_ROOT / "docs" / "sky_data"


def fetch_horizons(body_id: str, body_name: str, date_str: str) -> dict:
    """
    Query JPL Horizons OBSERVER table for apparent Az/El of a solar system body.

    QUANTITIES='4' returns the apparent Az & El pair as the sole two data columns
    (after the date tokens).  The query time is set to 10:00 UT, which is local
    noon for Eswatini (UTC+2), giving a physically meaningful snapshot for the
    day-of record used by the UE5 sky commandlet.

    Returns dict: {name, azimuth_deg, elevation_deg, visible}.
    Raises RuntimeError on HTTP or parse failure — callers emit a WARNING and
    continue rather than aborting the full batch.
    """
    try:
        start_dt = datetime.strptime(date_str, "%Y-%m-%d")
    except ValueError as exc:
        raise ValueError(f"date_str must be YYYY-MM-DD, got: {date_str!r}") from exc

    # Query at 10:00 UT = 12:00 local (Eswatini is UTC+2).
    # Horizons stop must be strictly after start; add 1 minute.
    start_str = f"{date_str} 10:00"
    stop_str  = f"{date_str} 10:01"

    # SITE_COORD order for Horizons: east_lon, lat, elevation_km
    site_coord = f"'{LON},{LAT},{ELEVATION_M / 1000:.3f}'"

    params = {
        "format":     "text",
        "COMMAND":    f"'{body_id}'",
        "CENTER":     "coord@399",
        "SITE_COORD": site_coord,
        "OBJ_DATA":   "NO",
        "MAKE_EPHEM": "YES",
        "EPHEM_TYPE": "OBSERVER",
        # QUANTITIES='4' = Apparent Az & El (topocentric, refraction-corrected).
        # Must be single-quoted in the URL value to be accepted by the API.
        "QUANTITIES": "'4'",
        "START_TIME": f"'{start_str}'",
        "STOP_TIME":  f"'{stop_str}'",
        "STEP_SIZE":  "'1h'",
        "ANG_FORMAT": "DEG",
    }

    query_string = urllib.parse.urlencode(params)
    url = f"{HORIZONS_URL}?{query_string}"

    try:
        with urllib.request.urlopen(url, timeout=20) as resp:
            raw = resp.read().decode("utf-8")
    except urllib.error.URLError as exc:
        raise RuntimeError(f"Horizons HTTP error for {body_name}: {exc}") from exc

    if "$$SOE" not in raw or "$$EOE" not in raw:
        raise RuntimeError(
            f"Horizons response for {body_name} missing $$SOE/$$EOE block.\n"
            f"First 500 chars: {raw[:500]}"
        )

    soe_start = raw.index("$$SOE") + len("$$SOE")
    soe_end   = raw.index("$$EOE")
    data_block = raw[soe_start:soe_end].strip()

    lines = [ln.strip() for ln in data_block.splitlines() if ln.strip()]
    if not lines:
        raise RuntimeError(f"Empty $$SOE block for {body_name}")

    # With QUANTITIES='4' the row format is:
    #   YYYY-Mon-DD HH:MM  flag  Az  El
    # The date portion occupies tokens [0] (date) and [1] (time), and token [2]
    # is a one-char flag (e.g. '*', 'm', 'C'). Az and El follow at [3] and [4].
    row = lines[0].split()
    try:
        az = float(row[3])
        el = float(row[4])
    except (IndexError, ValueError) as exc:
        raise RuntimeError(
            f"Cannot parse Az/El from Horizons row for {body_name}: {row!r}"
        ) from exc

    return {
        "name":          body_name,
        "azimuth_deg":   round(az, 4),
        "elevation_deg": round(el, 4),
        "visible":       el > 0.0,
    }


def compute_moon_phase(date_str: str) -> dict:
    """
    Pure-Python moon phase calculation.
    Reference epoch: JD 2451549.5 = 2000-01-06 00:00 UTC (known new moon).
    Synodic period: 29.53059 days.
    Returns phase_fraction (0.0–1.0), phase_name, illumination_pct.
    """
    KNOWN_NEW_MOON_JD = 2451549.5
    SYNODIC_PERIOD = 29.53059

    try:
        dt = datetime.strptime(date_str, "%Y-%m-%d").replace(tzinfo=timezone.utc)
    except ValueError as exc:
        raise ValueError(f"date_str must be YYYY-MM-DD, got: {date_str!r}") from exc

    # Julian date for the given date at noon (JD is referenced to noon)
    jd = _gregorian_to_jd(dt.year, dt.month, dt.day)

    elapsed = jd - KNOWN_NEW_MOON_JD
    phase_fraction = (elapsed % SYNODIC_PERIOD) / SYNODIC_PERIOD

    # Phase names divided into 8 octants
    if phase_fraction < 0.0625 or phase_fraction >= 0.9375:
        phase_name = "new"
    elif phase_fraction < 0.1875:
        phase_name = "waxing_crescent"
    elif phase_fraction < 0.3125:
        phase_name = "first_quarter"
    elif phase_fraction < 0.4375:
        phase_name = "waxing_gibbous"
    elif phase_fraction < 0.5625:
        phase_name = "full"
    elif phase_fraction < 0.6875:
        phase_name = "waning_gibbous"
    elif phase_fraction < 0.8125:
        phase_name = "last_quarter"
    else:
        phase_name = "waning_crescent"

    # Illumination: 0 at new (phase=0), 100 at full (phase=0.5)
    illumination_pct = round((1 - math.cos(2 * math.pi * phase_fraction)) / 2 * 100, 1)

    return {
        "phase_fraction":  round(phase_fraction, 4),
        "phase_name":      phase_name,
        "illumination_pct": illumination_pct,
    }


def _gregorian_to_jd(year: int, month: int, day: int) -> float:
    """Convert a Gregorian calendar date to Julian Day Number (noon)."""
    if month <= 2:
        year -= 1
        month += 12
    a = year // 100
    b = 2 - a + a // 4
    return int(365.25 * (year + 4716)) + int(30.6001 * (month + 1)) + day + b - 1524.5


def compute_swazi_calendar(date_str: str) -> dict:
    """
    Maps the date to Swazi astronomical/cultural calendar markers.

    isiLimela (Pleiades) heliacal rising in the Southern Hemisphere occurs in
    May/June (winter). This signals Incwala preparation and first-fruits
    planning. The Pleiades are NOT visible in November–January (lost in solar
    glare during Southern-Hemisphere summer).

    iNkosana (Southern Cross) is circumpolar from lat -26.5° and visible
    year-round at sufficient altitude.

    iNgonyama (Orion) is prominent in the Eswatini sky May–August (winter).
    """
    try:
        dt = datetime.strptime(date_str, "%Y-%m-%d")
    except ValueError as exc:
        raise ValueError(f"date_str must be YYYY-MM-DD, got: {date_str!r}") from exc

    month = dt.month

    # isiLimela season: Pleiades heliacal rising May–June (SH winter)
    # Visible pre-dawn from late April; best May–June.
    is_limela_season = month in (4, 5, 6)
    if is_limela_season:
        season_note = (
            "isiLimela (Pleiades) heliacal rising season. "
            "Time to prepare fields and plan the Incwala ceremony."
        )
    elif month in (11, 12, 1):
        season_note = (
            "isiLimela (Pleiades) not currently rising — lost in solar glare "
            "during Southern-Hemisphere summer. Incwala ceremony period (Dec–Jan)."
        )
    else:
        season_note = "isiLimela (Pleiades) not in planting season."

    # iNkosana (Southern Cross): circumpolar from -26.5°, visible year-round.
    # Altitude peaks in autumn/winter (Apr–Aug) at Eswatini.
    southern_cross_visible = True  # always above horizon; altitude peaks Apr–Aug

    # iNgonyama (Orion): May–August visibility window from Eswatini
    orion_visible = month in (5, 6, 7, 8)

    # Broad seasonal classification for Southern Hemisphere
    if month in (12, 1, 2):
        season = "summer"
    elif month in (3, 4, 5):
        season = "autumn"
    elif month in (6, 7, 8):
        season = "winter"
    else:
        season = "spring"

    return {
        "isLimelaSeason":         is_limela_season,
        "season_note":            season_note,
        "southern_cross_visible": southern_cross_visible,
        "orion_visible":          orion_visible,
        "season":                 season,
    }


def main() -> None:
    parser = argparse.ArgumentParser(
        description="Query JPL Horizons for sky positions at Eswatini coordinates."
    )
    parser.add_argument(
        "--date",
        default=datetime.now(tz=timezone.utc).strftime("%Y-%m-%d"),
        help="ISO date YYYY-MM-DD (default: today UTC)",
    )
    args = parser.parse_args()

    date_str: str = args.date

    # Validate date format early
    try:
        datetime.strptime(date_str, "%Y-%m-%d")
    except ValueError:
        print(f"ERROR: --date must be YYYY-MM-DD, got: {date_str!r}", file=sys.stderr)
        sys.exit(1)

    print(f"[sky-pipeline] Date: {date_str}  Location: {LAT}°, {LON}°  Elev: {ELEVATION_M}m")

    # Fetch all bodies; continue on per-body failure (network may be unreliable)
    bodies_output: dict = {}
    for name, body_id in BODIES.items():
        print(f"  Querying Horizons: {name} (ID={body_id}) ...", end=" ", flush=True)
        try:
            result = fetch_horizons(body_id, name, date_str)
            bodies_output[name] = {
                "azimuth_deg":   result["azimuth_deg"],
                "elevation_deg": result["elevation_deg"],
                "visible":       result["visible"],
            }
            print(f"Az={result['azimuth_deg']:.2f}° El={result['elevation_deg']:.2f}° "
                  f"({'visible' if result['visible'] else 'below horizon'})")
        except RuntimeError as exc:
            print(f"WARN: {exc}", file=sys.stderr)
            bodies_output[name] = {"azimuth_deg": None, "elevation_deg": None, "visible": None, "error": str(exc)}

    moon_phase = compute_moon_phase(date_str)
    print(f"  Moon phase: {moon_phase['phase_name']} ({moon_phase['illumination_pct']}% illuminated)")

    swazi_cal = compute_swazi_calendar(date_str)
    print(f"  Swazi calendar: season={swazi_cal['season']}  isiLimela={swazi_cal['isLimelaSeason']}")

    output = {
        "date":     date_str,
        "location": {"lat": LAT, "lon": LON, "elevation_m": ELEVATION_M},
        "bodies":   bodies_output,
        "moon":     moon_phase,
        "swazi_calendar": swazi_cal,
        "pipeline_version": "1.0",
        "deterministic":    True,
    }

    OUTPUT_DIR.mkdir(parents=True, exist_ok=True)
    out_path = OUTPUT_DIR / f"{date_str}.json"
    out_path.write_text(json.dumps(output, indent=2, ensure_ascii=False))
    print(f"[sky-pipeline] Written: {out_path}")


if __name__ == "__main__":
    main()
