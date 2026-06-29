"""Atmospheric acoustic propagation tables. Plan 6."""
import json
import math
import pathlib

# 8 octave band centre frequencies (Hz)
OCTAVE_BANDS_HZ = [125, 250, 500, 1000, 2000, 4000, 8000, 16000]

# Standard atmosphere high-frequency absorption [dB/km] at 20°C, 50% RH
# ISO 9613-1 approximation per octave band
_ISO_ABSORPTION_DB_KM = [0.1, 0.3, 1.0, 2.0, 4.0, 9.0, 22.0, 60.0]


def _sound_speed(temp_c: float) -> float:
    """Speed of sound in air [m/s] at temperature temp_c."""
    return 331.3 + 0.606 * temp_c


def _effective_speed(temp_c: float, wind_speed_ms: float,
                      wind_bearing_deg: float, propagation_bearing_deg: float) -> float:
    """Effective sound speed [m/s] including wind component along propagation direction."""
    c = _sound_speed(temp_c)
    angle_diff = math.radians(propagation_bearing_deg - wind_bearing_deg)
    wind_component = wind_speed_ms * math.cos(angle_diff)
    return c + wind_component


def _inversion_range_multiplier(temp_gradient_c_per_100m: float) -> float:
    """Range multiplier for anomalous propagation under temperature inversion.

    Positive gradient = warm air over cold (inversion) → long-range propagation.
    Normal lapse rate (negative gradient) → no anomalous propagation.
    """
    if temp_gradient_c_per_100m > 0:
        return 1.0 + temp_gradient_c_per_100m * 0.15
    return 1.0


def _atmospheric_absorption_db_per_m(humidity_fraction: float) -> list:
    """Per-band high-frequency absorption [dB/m].

    Humidity modulates absorption: higher humidity reduces HF absorption.
    """
    humidity_factor = 1.0 - humidity_fraction * 0.4
    return [
        (db_km * humidity_factor) / 1000.0
        for db_km in _ISO_ABSORPTION_DB_KM
    ]


def compute_propagation_tables(output_path: pathlib.Path) -> pathlib.Path:
    """Build atmospheric acoustic propagation lookup tables for Eswatini.

    Covers the relevant climate envelope:
    - Temperature: 0°C–35°C (10 steps)
    - Wind speed: 0–10 m/s (5 steps)
    - Humidity: 20%–95% (4 steps)
    - Temperature gradient: -1.0 to +1.5 °C/100m (inversion strength)

    Output JSON structure:
        {
          "octave_bands_hz": [...],
          "conditions": [
            {
              "temp_c": float,
              "wind_speed_ms": float,
              "humidity_fraction": float,
              "temp_gradient_c_per_100m": float,
              "sound_speed_ms": float,
              "inversion_range_multiplier": float,
              "absorption_db_per_m": [float]*8
            }, ...
          ]
        }
    """
    output_path = pathlib.Path(output_path)
    output_path.parent.mkdir(parents=True, exist_ok=True)

    conditions = []

    temperatures = [0, 5, 10, 15, 20, 25, 30, 35]
    wind_speeds = [0.0, 2.5, 5.0, 7.5, 10.0]
    humidities = [0.20, 0.45, 0.70, 0.95]
    # Eswatini-relevant temperature gradients: dry day (-0.65), inversion (+1.0)
    gradients = [-0.65, 0.0, 0.5, 1.0, 1.5]

    for temp in temperatures:
        for wind in wind_speeds:
            for humidity in humidities:
                for grad in gradients:
                    conditions.append({
                        "temp_c": temp,
                        "wind_speed_ms": wind,
                        "humidity_fraction": humidity,
                        "temp_gradient_c_per_100m": grad,
                        "sound_speed_ms": round(_sound_speed(temp), 3),
                        "effective_speed_northward_ms": round(
                            _effective_speed(temp, wind, 0.0, 0.0), 3),
                        "inversion_range_multiplier": round(
                            _inversion_range_multiplier(grad), 4),
                        "absorption_db_per_m": [
                            round(v, 8)
                            for v in _atmospheric_absorption_db_per_m(humidity)
                        ],
                    })

    data = {
        "octave_bands_hz": OCTAVE_BANDS_HZ,
        "n_conditions": len(conditions),
        "conditions": conditions,
    }

    with open(output_path, "w", encoding="utf-8") as fh:
        json.dump(data, fh, indent=2)

    return output_path
