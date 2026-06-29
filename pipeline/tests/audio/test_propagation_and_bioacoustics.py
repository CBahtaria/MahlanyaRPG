import json
import pathlib
import pytest
from pipeline.audio.compute_propagation_tables import compute_propagation_tables
from pipeline.audio.build_bioacoustic_library import build_bioacoustic_library


class TestPropagationTables:
    def test_returns_path(self, tmp_path):
        out = compute_propagation_tables(tmp_path / "prop.json")
        assert isinstance(out, pathlib.Path)

    def test_file_created(self, tmp_path):
        out = compute_propagation_tables(tmp_path / "prop.json")
        assert out.exists()

    def test_has_octave_bands(self, tmp_path):
        out = compute_propagation_tables(tmp_path / "prop.json")
        data = json.loads(out.read_text())
        assert len(data["octave_bands_hz"]) == 8

    def test_has_conditions(self, tmp_path):
        out = compute_propagation_tables(tmp_path / "prop.json")
        data = json.loads(out.read_text())
        assert len(data["conditions"]) > 0

    def test_each_condition_has_absorption(self, tmp_path):
        out = compute_propagation_tables(tmp_path / "prop.json")
        data = json.loads(out.read_text())
        for cond in data["conditions"]:
            assert "absorption_db_per_m" in cond
            assert len(cond["absorption_db_per_m"]) == 8

    def test_sound_speed_increases_with_temperature(self, tmp_path):
        out = compute_propagation_tables(tmp_path / "prop.json")
        data = json.loads(out.read_text())
        speeds = {c["temp_c"]: c["sound_speed_ms"]
                  for c in data["conditions"] if c["wind_speed_ms"] == 0}
        temps = sorted(speeds.keys())
        for i in range(len(temps) - 1):
            assert speeds[temps[i]] < speeds[temps[i+1]]

    def test_inversion_multiplier_positive(self, tmp_path):
        out = compute_propagation_tables(tmp_path / "prop.json")
        data = json.loads(out.read_text())
        for cond in data["conditions"]:
            assert cond["inversion_range_multiplier"] >= 1.0

    def test_inversion_stronger_than_normal(self, tmp_path):
        """Positive gradient (inversion) must give higher range multiplier."""
        out = compute_propagation_tables(tmp_path / "prop.json")
        data = json.loads(out.read_text())
        by_grad = {}
        for cond in data["conditions"]:
            g = cond["temp_gradient_c_per_100m"]
            by_grad.setdefault(g, []).append(cond["inversion_range_multiplier"])
        grad_keys = sorted(by_grad.keys())
        negative = grad_keys[0]
        positive = [g for g in grad_keys if g > 0]
        if positive:
            assert by_grad[negative][0] < by_grad[positive[-1]][0]

    def test_hf_absorption_increases_with_frequency(self, tmp_path):
        """Higher octave bands must have higher absorption."""
        out = compute_propagation_tables(tmp_path / "prop.json")
        data = json.loads(out.read_text())
        cond = data["conditions"][0]
        abs_vals = cond["absorption_db_per_m"]
        # Not strictly monotone but generally increasing; check first vs last
        assert abs_vals[0] < abs_vals[-1]

    def test_output_parent_created(self, tmp_path):
        nested = tmp_path / "a" / "b" / "prop.json"
        compute_propagation_tables(nested)
        assert nested.exists()


class TestBioacousticLibrary:
    def test_returns_path(self, tmp_path):
        out = build_bioacoustic_library(tmp_path / "bio.json")
        assert isinstance(out, pathlib.Path)

    def test_file_created(self, tmp_path):
        out = build_bioacoustic_library(tmp_path / "bio.json")
        assert out.exists()

    def test_28_bird_species(self, tmp_path):
        out = build_bioacoustic_library(tmp_path / "bio.json")
        data = json.loads(out.read_text())
        assert len(data["bird_species"]) == 28

    def test_each_species_has_freq_range(self, tmp_path):
        out = build_bioacoustic_library(tmp_path / "bio.json")
        data = json.loads(out.read_text())
        for sp in data["bird_species"]:
            assert "freq_hz" in sp
            lo, hi = sp["freq_hz"]
            assert lo < hi

    def test_each_species_has_active_windows(self, tmp_path):
        out = build_bioacoustic_library(tmp_path / "bio.json")
        data = json.loads(out.read_text())
        for sp in data["bird_species"]:
            assert "active_windows" in sp
            assert len(sp["active_windows"]) >= 1

    def test_each_species_has_biome_zones(self, tmp_path):
        out = build_bioacoustic_library(tmp_path / "bio.json")
        data = json.loads(out.read_text())
        for sp in data["bird_species"]:
            assert "biome_zones" in sp
            assert len(sp["biome_zones"]) >= 1

    def test_invertebrate_chorus_present(self, tmp_path):
        out = build_bioacoustic_library(tmp_path / "bio.json")
        data = json.loads(out.read_text())
        assert "invertebrate_chorus" in data
        assert len(data["invertebrate_chorus"]) >= 2

    def test_hadeda_ibis_in_library(self, tmp_path):
        """Hadeda Ibis is the iconic dawn-chorus species for Eswatini."""
        out = build_bioacoustic_library(tmp_path / "bio.json")
        data = json.loads(out.read_text())
        names = [sp["name"] for sp in data["bird_species"]]
        assert any("Hadeda" in n for n in names)

    def test_nocturnal_species_present(self, tmp_path):
        """At least one nocturnal species must be present (owls)."""
        out = build_bioacoustic_library(tmp_path / "bio.json")
        data = json.loads(out.read_text())
        nocturnal = [
            sp for sp in data["bird_species"]
            if any(w[0] >= 18 or w[1] <= 6 for w in sp["active_windows"])
        ]
        assert len(nocturnal) >= 2

    def test_5_biome_zones(self, tmp_path):
        out = build_bioacoustic_library(tmp_path / "bio.json")
        data = json.loads(out.read_text())
        assert len(data["biome_zones"]) == 5

    def test_output_parent_created(self, tmp_path):
        nested = tmp_path / "nested" / "bio.json"
        build_bioacoustic_library(nested)
        assert nested.exists()
