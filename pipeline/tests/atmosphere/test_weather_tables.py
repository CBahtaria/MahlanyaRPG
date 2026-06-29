import json
import pathlib
import pytest
from pipeline.atmosphere.build_weather_tables import build_weather_tables


class TestBuildWeatherTables:
    def test_returns_path(self, tmp_path):
        result = build_weather_tables(tmp_path / "saws", tmp_path / "weather.json")
        assert isinstance(result, pathlib.Path)

    def test_file_created(self, tmp_path):
        out = tmp_path / "weather.json"
        build_weather_tables(tmp_path / "saws", out)
        assert out.exists()

    def test_has_12_months(self, tmp_path):
        out = tmp_path / "weather.json"
        build_weather_tables(tmp_path / "saws", out)
        data = json.loads(out.read_text())
        assert len(data["months"]) == 12

    def test_each_month_has_24_hourly_probs(self, tmp_path):
        out = tmp_path / "weather.json"
        build_weather_tables(tmp_path / "saws", out)
        data = json.loads(out.read_text())
        for key, month_data in data["months"].items():
            probs = month_data["thunderstorm_hourly_prob"]
            assert len(probs) == 24, f"Month {key} has {len(probs)} values"

    def test_hourly_probs_in_range(self, tmp_path):
        out = tmp_path / "weather.json"
        build_weather_tables(tmp_path / "saws", out)
        data = json.loads(out.read_text())
        for key, month_data in data["months"].items():
            for prob in month_data["thunderstorm_hourly_prob"]:
                assert 0.0 <= prob <= 1.0, f"Month {key}: {prob} out of [0,1]"

    def test_wet_season_peak_higher_than_dry(self, tmp_path):
        """January (wet) peak must exceed July (dry) peak."""
        out = tmp_path / "weather.json"
        build_weather_tables(tmp_path / "saws", out)
        data = json.loads(out.read_text())
        jan_peak = max(data["months"]["1"]["thunderstorm_hourly_prob"])
        jul_peak = max(data["months"]["7"]["thunderstorm_hourly_prob"])
        assert jan_peak > jul_peak

    def test_peak_in_afternoon_window(self, tmp_path):
        """January thunderstorm peak should be at 13:00–18:00."""
        out = tmp_path / "weather.json"
        build_weather_tables(tmp_path / "saws", out)
        data = json.loads(out.read_text())
        probs = data["months"]["1"]["thunderstorm_hourly_prob"]
        peak_hour = probs.index(max(probs))
        assert 13 <= peak_hour <= 18

    def test_winter_has_more_cold_fronts(self, tmp_path):
        out = tmp_path / "weather.json"
        build_weather_tables(tmp_path / "saws", out)
        data = json.loads(out.read_text())
        jun = data["months"]["6"]["cold_front_passes_per_month"]
        jan = data["months"]["1"]["cold_front_passes_per_month"]
        assert jun > jan

    def test_temperature_ranges_present_and_valid(self, tmp_path):
        out = tmp_path / "weather.json"
        build_weather_tables(tmp_path / "saws", out)
        data = json.loads(out.read_text())
        for m in data["months"].values():
            assert "temp_range_highveld_c" in m
            lo, hi = m["temp_range_highveld_c"]
            assert lo < hi

    def test_output_parent_created(self, tmp_path):
        nested = tmp_path / "a" / "b" / "weather.json"
        build_weather_tables(tmp_path / "saws", nested)
        assert nested.exists()

    def test_month_names_present(self, tmp_path):
        out = tmp_path / "weather.json"
        build_weather_tables(tmp_path / "saws", out)
        data = json.loads(out.read_text())
        assert data["months"]["1"]["name"] == "January"
        assert data["months"]["7"]["name"] == "July"
