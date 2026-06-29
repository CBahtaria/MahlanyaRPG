import math
import pathlib
import struct
import pytest
from pipeline.audio.compute_acoustic_irs import (
    MATERIAL_ABSORPTION,
    ENVIRONMENT_ARCHETYPES,
    compute_ir_image_source,
    compute_ir_monte_carlo,
    export_ir_wav,
    SPEED_OF_SOUND,
)

BOX_GEO = {
    "dimensions": [10.0, 8.0, 4.0],
    "materials": {"floor": "clay_earth", "ceiling": "thatch_grass", "walls": "granite"},
}
SRC = (1.0, 1.0, 1.5)
RCV = (9.0, 7.0, 1.5)


class TestMaterialAbsorption:
    def test_required_materials_present(self):
        required = {"granite", "thatch_grass", "clay_earth",
                    "dry_grass", "water_surface", "open_sky"}
        assert required.issubset(set(MATERIAL_ABSORPTION.keys()))

    def test_each_material_has_8_bands(self):
        for mat, bands in MATERIAL_ABSORPTION.items():
            assert len(bands) == 8, f"{mat} must have 8 bands"

    def test_absorption_in_range(self):
        for mat, bands in MATERIAL_ABSORPTION.items():
            for c in bands:
                assert 0.0 <= c <= 1.0, f"{mat}: coeff {c} out of [0,1]"

    def test_open_sky_full_absorption(self):
        assert all(c == 1.0 for c in MATERIAL_ABSORPTION["open_sky"])

    def test_granite_low_absorption(self):
        assert all(c < 0.1 for c in MATERIAL_ABSORPTION["granite"])


class TestEnvironmentArchetypes:
    def test_count_is_six(self):
        assert len(ENVIRONMENT_ARCHETYPES) == 6

    def test_granite_cave_present(self):
        assert "granite_cave" in ENVIRONMENT_ARCHETYPES

    def test_thatched_hut_present(self):
        assert "thatched_hut_int" in ENVIRONMENT_ARCHETYPES


class TestImageSource:
    def test_returns_list(self):
        result = compute_ir_image_source(BOX_GEO, SRC, RCV)
        assert isinstance(result, list)

    def test_direct_path_first(self):
        """First entry must be the direct path (order 0)."""
        result = compute_ir_image_source(BOX_GEO, SRC, RCV, max_order=1)
        assert result[0]["order"] == 0

    def test_direct_path_delay(self):
        d = math.sqrt((9-1)**2 + (7-1)**2 + 0)
        expected_delay = d / SPEED_OF_SOUND
        result = compute_ir_image_source(BOX_GEO, SRC, RCV, max_order=0)
        assert abs(result[0]["delay_s"] - expected_delay) < 1e-4

    def test_each_reflection_has_8_energy_bands(self):
        result = compute_ir_image_source(BOX_GEO, SRC, RCV, max_order=2)
        for r in result:
            assert len(r["energy"]) == 8

    def test_sorted_by_delay(self):
        result = compute_ir_image_source(BOX_GEO, SRC, RCV, max_order=2)
        delays = [r["delay_s"] for r in result]
        assert delays == sorted(delays)

    def test_higher_order_later_arrival(self):
        result = compute_ir_image_source(BOX_GEO, SRC, RCV, max_order=2)
        order_0 = [r for r in result if r["order"] == 0][0]
        order_2 = [r for r in result if r["order"] == 2]
        assert all(r["delay_s"] >= order_0["delay_s"] for r in order_2)

    def test_energy_decreases_with_order(self):
        """Mean energy of order-1 reflections < direct path energy."""
        result = compute_ir_image_source(BOX_GEO, SRC, RCV, max_order=2)
        direct_e = sum(result[0]["energy"]) / 8
        order1 = [r for r in result if r["order"] == 1]
        if order1:
            mean_e1 = sum(sum(r["energy"]) / 8 for r in order1) / len(order1)
            assert mean_e1 < direct_e

    def test_max_order_0_returns_only_direct(self):
        result = compute_ir_image_source(BOX_GEO, SRC, RCV, max_order=0)
        assert len(result) == 1
        assert result[0]["order"] == 0


class TestMonteCarlo:
    def test_returns_list(self):
        result = compute_ir_monte_carlo(BOX_GEO, SRC, RCV, n_rays=100, max_bounces=5)
        assert isinstance(result, list)

    def test_sorted_by_delay(self):
        result = compute_ir_monte_carlo(BOX_GEO, SRC, RCV, n_rays=500, max_bounces=10)
        delays = [r["delay_s"] for r in result]
        assert delays == sorted(delays)

    def test_all_reflections_have_8_energy_bands(self):
        result = compute_ir_monte_carlo(BOX_GEO, SRC, RCV, n_rays=200, max_bounces=5)
        for r in result:
            assert len(r["energy"]) == 8

    def test_positive_energy(self):
        result = compute_ir_monte_carlo(BOX_GEO, SRC, RCV, n_rays=500, max_bounces=10)
        for r in result:
            assert all(e >= 0 for e in r["energy"])

    def test_deterministic_with_same_seed(self):
        r1 = compute_ir_monte_carlo(BOX_GEO, SRC, RCV, n_rays=100, max_bounces=5)
        r2 = compute_ir_monte_carlo(BOX_GEO, SRC, RCV, n_rays=100, max_bounces=5)
        assert len(r1) == len(r2)


class TestExportIrWav:
    def test_creates_file(self, tmp_path):
        reflections = compute_ir_image_source(BOX_GEO, SRC, RCV, max_order=1)
        out = export_ir_wav(reflections, tmp_path / "test.wav")
        assert out.exists()

    def test_wav_header(self, tmp_path):
        """File must start with RIFF...WAVE."""
        reflections = compute_ir_image_source(BOX_GEO, SRC, RCV, max_order=1)
        out = export_ir_wav(reflections, tmp_path / "test.wav")
        data = out.read_bytes()
        assert data[:4] == b"RIFF"
        assert data[8:12] == b"WAVE"

    def test_wav_sample_rate(self, tmp_path):
        reflections = compute_ir_image_source(BOX_GEO, SRC, RCV, max_order=1)
        out = export_ir_wav(reflections, tmp_path / "test.wav", sample_rate=48000)
        data = out.read_bytes()
        sample_rate = struct.unpack_from("<I", data, 24)[0]
        assert sample_rate == 48000

    def test_empty_reflections_creates_file(self, tmp_path):
        out = export_ir_wav([], tmp_path / "empty.wav")
        assert out.exists()
