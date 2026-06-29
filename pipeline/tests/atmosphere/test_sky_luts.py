import json
import pathlib
import pytest
from pipeline.atmosphere.compute_sky_luts import (
    compute_sky_luts,
    export_lunar_calendar,
    export_star_field,
)


class TestComputeSkyLuts:
    def test_returns_tuple_of_paths(self, tmp_path):
        result = compute_sky_luts(-26.0, tmp_path)
        assert isinstance(result, tuple)
        assert len(result) == 2

    def test_highveld_lut_created(self, tmp_path):
        compute_sky_luts(-26.0, tmp_path)
        assert (tmp_path / "sky_lut_highveld.json").exists()

    def test_lowveld_lut_created(self, tmp_path):
        compute_sky_luts(-26.0, tmp_path)
        assert (tmp_path / "sky_lut_lowveld.json").exists()

    def test_lut_dimensions(self, tmp_path):
        """LUT must be 91×10×16 with default parameters."""
        compute_sky_luts(-26.0, tmp_path)
        data = json.loads((tmp_path / "sky_lut_highveld.json").read_text())
        radiance = data["radiance"]
        assert len(radiance) == 91
        assert len(radiance[0]) == 10
        assert len(radiance[0][0]) == 16

    def test_lut_values_in_range(self, tmp_path):
        compute_sky_luts(-26.0, tmp_path)
        data = json.loads((tmp_path / "sky_lut_highveld.json").read_text())
        for elev_row in data["radiance"]:
            for turb_row in elev_row:
                for val in turb_row:
                    assert 0.0 <= val <= 1.0, f"Value {val} out of [0,1]"

    def test_altitude_bands_differ(self, tmp_path):
        """Highveld and Lowveld LUTs must produce different radiance values."""
        compute_sky_luts(-26.0, tmp_path)
        high = json.loads((tmp_path / "sky_lut_highveld.json").read_text())
        low = json.loads((tmp_path / "sky_lut_lowveld.json").read_text())
        # At 45° sun elevation, turbidity 1.5 (index 0), 720nm (index 15).
        # 380nm saturates at 1.0 for both bands; 720nm is in the measurable range.
        assert high["radiance"][45][0][15] != low["radiance"][45][0][15]

    def test_zero_sun_elevation_is_low_radiance(self, tmp_path):
        """Horizon sun (0°) must produce near-zero radiance."""
        compute_sky_luts(-26.0, tmp_path)
        data = json.loads((tmp_path / "sky_lut_highveld.json").read_text())
        # index 0 = 0° elevation
        for turb_row in data["radiance"][0]:
            for val in turb_row:
                assert val < 0.05, f"Horizon radiance {val} too high"

    def test_lunar_calendar_written(self, tmp_path):
        compute_sky_luts(-26.0, tmp_path)
        assert (tmp_path / "lunar_calendar_1750_1910.json").exists()

    def test_lunar_calendar_entry_count(self, tmp_path):
        compute_sky_luts(-26.0, tmp_path)
        data = json.loads((tmp_path / "lunar_calendar_1750_1910.json").read_text())
        assert isinstance(data, list)
        assert len(data) > 50000  # 161 years × ~365 days

    def test_lunar_calendar_phase_range(self, tmp_path):
        compute_sky_luts(-26.0, tmp_path)
        data = json.loads((tmp_path / "lunar_calendar_1750_1910.json").read_text())
        for entry in data[:200]:
            assert 0.0 <= entry["phase"] < 1.0
            assert "phase_name" in entry
            assert "date" in entry

    def test_starfield_written(self, tmp_path):
        compute_sky_luts(-26.0, tmp_path)
        assert (tmp_path / "starfield_26S_J1850.json").exists()

    def test_starfield_count(self, tmp_path):
        compute_sky_luts(-26.0, tmp_path)
        data = json.loads((tmp_path / "starfield_26S_J1850.json").read_text())
        assert data["count"] >= 7000

    def test_starfield_no_stars_below_horizon(self, tmp_path):
        """At 26°S all stars must have dec ≥ -64°."""
        compute_sky_luts(-26.0, tmp_path)
        data = json.loads((tmp_path / "starfield_26S_J1850.json").read_text())
        for star in data["stars"]:
            assert star["dec_deg"] >= -64.0

    def test_export_lunar_calendar(self, tmp_path):
        path = export_lunar_calendar(1800, 1810, tmp_path / "lunar.json")
        assert path.exists()
        data = json.loads(path.read_text())
        assert isinstance(data, list)
        assert len(data) > 3000  # 11 years

    def test_export_star_field(self, tmp_path):
        path = export_star_field(-26.0, 1850.0, tmp_path / "stars.json")
        assert path.exists()
        data = json.loads(path.read_text())
        assert "stars" in data
        assert data["count"] >= 1000

    def test_output_dir_created(self, tmp_path):
        nested = tmp_path / "a" / "b"
        compute_sky_luts(-26.0, nested)
        assert nested.exists()

    def test_custom_steps(self, tmp_path):
        """Custom step counts propagate to LUT shape."""
        compute_sky_luts(-26.0, tmp_path / "custom",
                          sun_elevation_steps=5, turbidity_steps=3, wavelength_bands=4)
        data = json.loads((tmp_path / "custom" / "sky_lut_highveld.json").read_text())
        assert len(data["radiance"]) == 5
        assert len(data["radiance"][0]) == 3
        assert len(data["radiance"][0][0]) == 4
