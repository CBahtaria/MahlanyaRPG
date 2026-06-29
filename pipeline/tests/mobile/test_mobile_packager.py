"""Tests for pipeline/mobile/package_mobile_artifacts.py"""
import json
import pathlib

import pytest

from pipeline.mobile.package_mobile_artifacts import (
    PackageManifest,
    _REQUIRED_ARTIFACTS,
    package_mobile_artifacts,
)


# ── Helpers ──────────────────────────────────────────────────────────────────

def _make_file(path: pathlib.Path, content: bytes = b"data") -> pathlib.Path:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(content)
    return path


def _seed_full_outputs(root: pathlib.Path) -> None:
    """Create the minimum required + a representative set of optional files."""
    _make_file(root / "terrain" / "tiles_manifest.json",     b'{"tiles":[]}')
    _make_file(root / "terrain" / "tile_000_000.r16",        b"\x00" * 2024)
    _make_file(root / "terrain" / "tile_000_001.r16",        b"\x00" * 2024)
    _make_file(root / "audio"   / "IR_GraniteCave.wav",      b"RIFF" + b"\x00" * 36)
    _make_file(root / "audio"   / "IR_ThatchedHutInt.wav",   b"RIFF" + b"\x00" * 36)
    _make_file(root / "audio"   / "atmospheric_propagation.json", b"{}")
    _make_file(root / "audio"   / "bioacoustic_library.json",    b"{}")
    _make_file(root / "atmosphere" / "sky_lut_highveld.json",    b"{}")
    _make_file(root / "atmosphere" / "sky_lut_lowveld.json",     b"{}")
    _make_file(root / "atmosphere" / "lunar_calendar_1750_1910.json", b"[]")
    _make_file(root / "atmosphere" / "starfield_26S_J1850.json",     b"[]")
    _make_file(root / "atmosphere" / "weather_seasonal_table.json",  b"{}")
    _make_file(root / "history"  / "knowledge_graph.ndjson",  b'{"id":"NB001"}\n')
    _make_file(root / "settlements" / "village_001" / "layout.json", b"{}")


# ── PackageManifest unit tests ────────────────────────────────────────────────

class TestPackageManifest:
    def test_is_valid_when_no_missing_required(self):
        m = PackageManifest()
        assert m.is_valid() is True

    def test_is_invalid_when_missing_required(self):
        m = PackageManifest()
        m.missing_required.append("history/knowledge_graph.ndjson")
        assert m.is_valid() is False

    def test_to_dict_contains_expected_keys(self):
        m = PackageManifest()
        d = m.to_dict()
        assert "schema_version" in d
        assert "files" in d
        assert "missing_required" in d
        assert "missing_optional" in d
        assert "valid" in d
        assert "total_files" in d

    def test_to_dict_valid_reflects_is_valid(self):
        m = PackageManifest()
        assert m.to_dict()["valid"] is True
        m.missing_required.append("something")
        assert m.to_dict()["valid"] is False

    def test_schema_version_is_semver_string(self):
        m = PackageManifest()
        assert isinstance(m.schema_version, str)
        assert "." in m.schema_version


# ── package_mobile_artifacts integration tests ────────────────────────────────

class TestPackageMobileArtifacts:
    def test_empty_outputs_reports_all_required_missing(self, tmp_path):
        outputs = tmp_path / "outputs"
        outputs.mkdir()
        result = package_mobile_artifacts(outputs)
        assert not result.is_valid()
        assert len(result.missing_required) == len(_REQUIRED_ARTIFACTS)

    def test_full_seed_produces_valid_package(self, tmp_path):
        outputs = tmp_path / "outputs"
        _seed_full_outputs(outputs)
        result = package_mobile_artifacts(outputs)
        assert result.is_valid(), f"Missing required: {result.missing_required}"

    def test_r16_tiles_copied(self, tmp_path):
        outputs = tmp_path / "outputs"
        _seed_full_outputs(outputs)
        pkg = tmp_path / "pkg"
        package_mobile_artifacts(outputs, pkg)
        r16_files = list((pkg / "terrain").glob("*.r16"))
        assert len(r16_files) == 2

    def test_wav_irs_copied(self, tmp_path):
        outputs = tmp_path / "outputs"
        _seed_full_outputs(outputs)
        pkg = tmp_path / "pkg"
        package_mobile_artifacts(outputs, pkg)
        wav_files = list((pkg / "audio").glob("*.wav"))
        assert len(wav_files) >= 1

    def test_knowledge_graph_ndjson_copied(self, tmp_path):
        outputs = tmp_path / "outputs"
        _seed_full_outputs(outputs)
        pkg = tmp_path / "pkg"
        package_mobile_artifacts(outputs, pkg)
        assert (pkg / "history" / "knowledge_graph.ndjson").exists()

    def test_manifest_json_written(self, tmp_path):
        outputs = tmp_path / "outputs"
        _seed_full_outputs(outputs)
        pkg = tmp_path / "pkg"
        package_mobile_artifacts(outputs, pkg)
        manifest_path = pkg / "manifest.json"
        assert manifest_path.exists()
        data = json.loads(manifest_path.read_text())
        assert "files" in data
        assert "valid" in data

    def test_manifest_total_files_matches_files_list(self, tmp_path):
        outputs = tmp_path / "outputs"
        _seed_full_outputs(outputs)
        result = package_mobile_artifacts(outputs)
        d = result.to_dict()
        assert d["total_files"] == len(d["files"])

    def test_manifest_file_entries_have_required_fields(self, tmp_path):
        outputs = tmp_path / "outputs"
        _seed_full_outputs(outputs)
        result = package_mobile_artifacts(outputs)
        for entry in result.files:
            assert "path" in entry
            assert "size_bytes" in entry
            assert "sha256" in entry
            assert isinstance(entry["size_bytes"], int)
            assert len(entry["sha256"]) == 64  # SHA-256 hex

    def test_sha256_matches_file_content(self, tmp_path):
        import hashlib
        outputs = tmp_path / "outputs"
        content = b"test content for checksum"
        _make_file(outputs / "history" / "knowledge_graph.ndjson", content)
        _make_file(outputs / "terrain" / "tiles_manifest.json",    b"{}")
        _make_file(outputs / "audio"   / "atmospheric_propagation.json", b"{}")
        _make_file(outputs / "audio"   / "bioacoustic_library.json",     b"{}")
        _make_file(outputs / "atmosphere" / "weather_seasonal_table.json", b"{}")
        result = package_mobile_artifacts(outputs)
        # Find the knowledge graph entry
        kg_entries = [e for e in result.files if "knowledge_graph" in e["path"]]
        assert len(kg_entries) == 1
        expected_sha = hashlib.sha256(content).hexdigest()
        assert kg_entries[0]["sha256"] == expected_sha

    def test_default_package_dir_is_mobile_package_subdir(self, tmp_path):
        outputs = tmp_path / "outputs"
        outputs.mkdir()
        package_mobile_artifacts(outputs)
        assert (tmp_path / "outputs" / "mobile_package").exists()

    def test_custom_package_dir_used(self, tmp_path):
        outputs = tmp_path / "outputs"
        outputs.mkdir()
        custom_pkg = tmp_path / "my_package"
        package_mobile_artifacts(outputs, custom_pkg)
        assert custom_pkg.exists()

    def test_settlements_json_copied(self, tmp_path):
        outputs = tmp_path / "outputs"
        _seed_full_outputs(outputs)
        pkg = tmp_path / "pkg"
        package_mobile_artifacts(outputs, pkg)
        settlement_files = list((pkg / "settlements").rglob("*.json"))
        assert len(settlement_files) >= 1

    def test_missing_one_required_reports_it(self, tmp_path):
        outputs = tmp_path / "outputs"
        # Provide all required except knowledge_graph.ndjson
        _make_file(outputs / "terrain" / "tiles_manifest.json",    b"{}")
        _make_file(outputs / "audio"   / "atmospheric_propagation.json", b"{}")
        _make_file(outputs / "audio"   / "bioacoustic_library.json",     b"{}")
        _make_file(outputs / "atmosphere" / "weather_seasonal_table.json", b"{}")
        result = package_mobile_artifacts(outputs)
        assert not result.is_valid()
        assert any("knowledge_graph" in m for m in result.missing_required)

    def test_idempotent_repackage(self, tmp_path):
        outputs = tmp_path / "outputs"
        _seed_full_outputs(outputs)
        pkg = tmp_path / "pkg"
        result1 = package_mobile_artifacts(outputs, pkg)
        result2 = package_mobile_artifacts(outputs, pkg)
        assert result1.is_valid()
        assert result2.is_valid()
        assert result1.to_dict()["total_files"] == result2.to_dict()["total_files"]

    def test_sky_luts_copied(self, tmp_path):
        outputs = tmp_path / "outputs"
        _seed_full_outputs(outputs)
        pkg = tmp_path / "pkg"
        package_mobile_artifacts(outputs, pkg)
        lut_files = list((pkg / "atmosphere").glob("sky_lut_*.json"))
        assert len(lut_files) == 2  # highveld + lowveld

    def test_atmospheric_propagation_copied(self, tmp_path):
        outputs = tmp_path / "outputs"
        _seed_full_outputs(outputs)
        pkg = tmp_path / "pkg"
        package_mobile_artifacts(outputs, pkg)
        assert (pkg / "audio" / "atmospheric_propagation.json").exists()
