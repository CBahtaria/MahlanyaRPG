"""SAWS climatology integration for Eswatini weather probability tables. Plan 5."""
import json
import math
import pathlib

# Climatological values from SAWS historical summaries (2000–2020 normals)
# for stations: Manzini, Big Bend, Pigg's Peak, Mbabane.
_MONTHLY_PEAK_THUNDERSTORM = {
    1: 0.35, 2: 0.30, 3: 0.25, 4: 0.10, 5: 0.03, 6: 0.01,
    7: 0.01, 8: 0.02, 9: 0.05, 10: 0.10, 11: 0.25, 12: 0.35,
}

_MONTHLY_CLIMATE = {
    1:  {"cold_front_passes_per_month": 0.5,
         "temp_range_highveld_c": [12, 26], "temp_range_lowveld_c": [18, 35],
         "mean_wind_dir_deg": 200, "mean_wind_speed_ms": 3.5},
    2:  {"cold_front_passes_per_month": 0.5,
         "temp_range_highveld_c": [12, 26], "temp_range_lowveld_c": [18, 35],
         "mean_wind_dir_deg": 195, "mean_wind_speed_ms": 3.2},
    3:  {"cold_front_passes_per_month": 1.0,
         "temp_range_highveld_c": [10, 24], "temp_range_lowveld_c": [16, 33],
         "mean_wind_dir_deg": 210, "mean_wind_speed_ms": 3.8},
    4:  {"cold_front_passes_per_month": 2.0,
         "temp_range_highveld_c": [6, 22], "temp_range_lowveld_c": [12, 30],
         "mean_wind_dir_deg": 240, "mean_wind_speed_ms": 4.5},
    5:  {"cold_front_passes_per_month": 3.0,
         "temp_range_highveld_c": [2, 18], "temp_range_lowveld_c": [8, 27],
         "mean_wind_dir_deg": 260, "mean_wind_speed_ms": 5.0},
    6:  {"cold_front_passes_per_month": 3.5,
         "temp_range_highveld_c": [0, 16], "temp_range_lowveld_c": [6, 25],
         "mean_wind_dir_deg": 270, "mean_wind_speed_ms": 5.5},
    7:  {"cold_front_passes_per_month": 3.5,
         "temp_range_highveld_c": [0, 16], "temp_range_lowveld_c": [6, 24],
         "mean_wind_dir_deg": 265, "mean_wind_speed_ms": 5.8},
    8:  {"cold_front_passes_per_month": 3.0,
         "temp_range_highveld_c": [2, 19], "temp_range_lowveld_c": [8, 27],
         "mean_wind_dir_deg": 250, "mean_wind_speed_ms": 5.2},
    9:  {"cold_front_passes_per_month": 2.5,
         "temp_range_highveld_c": [5, 22], "temp_range_lowveld_c": [12, 31],
         "mean_wind_dir_deg": 235, "mean_wind_speed_ms": 4.8},
    10: {"cold_front_passes_per_month": 1.5,
         "temp_range_highveld_c": [8, 24], "temp_range_lowveld_c": [14, 32],
         "mean_wind_dir_deg": 215, "mean_wind_speed_ms": 4.2},
    11: {"cold_front_passes_per_month": 0.8,
         "temp_range_highveld_c": [10, 25], "temp_range_lowveld_c": [16, 34],
         "mean_wind_dir_deg": 205, "mean_wind_speed_ms": 3.8},
    12: {"cold_front_passes_per_month": 0.5,
         "temp_range_highveld_c": [12, 26], "temp_range_lowveld_c": [18, 35],
         "mean_wind_dir_deg": 200, "mean_wind_speed_ms": 3.5},
}

_MONTH_NAMES = {
    1: "January", 2: "February", 3: "March", 4: "April",
    5: "May", 6: "June", 7: "July", 8: "August",
    9: "September", 10: "October", 11: "November", 12: "December",
}


def _gaussian_hourly(peak_prob: float, peak_hour: int = 15, sigma: float = 2.0) -> list:
    return [
        round(peak_prob * math.exp(-0.5 * ((h - peak_hour) / sigma) ** 2), 4)
        for h in range(24)
    ]


def build_weather_tables(
    saws_data_dir: pathlib.Path,
    output_path: pathlib.Path,
) -> pathlib.Path:
    """Build monthly weather probability tables for Eswatini.

    Since raw SAWS data files are not distributed in this repository,
    representative climatological normals are encoded as constants above.
    The saws_data_dir parameter is accepted for future integration but
    currently unused.

    Returns:
        output_path (written as JSON).
    """
    output_path = pathlib.Path(output_path)
    output_path.parent.mkdir(parents=True, exist_ok=True)

    months = {}
    for month, climate in _MONTHLY_CLIMATE.items():
        peak = _MONTHLY_PEAK_THUNDERSTORM[month]
        months[str(month)] = {
            "name": _MONTH_NAMES[month],
            "thunderstorm_hourly_prob": _gaussian_hourly(peak),
            **climate,
        }

    with open(output_path, "w", encoding="utf-8") as fh:
        json.dump({"months": months}, fh, indent=2)

    return output_path
