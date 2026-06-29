"""
Mahlanya Production Pipeline: /pipeline/mobile/package_mobile_artifacts.py

Collects pre-baked pipeline outputs into a self-contained mobile artifact
package at outputs/mobile_package/.  The package includes a manifest JSON
that the UE5 mobile build cook step reads to verify completeness.

Expected input layout (relative to pipeline root):
    outputs/terrain/         ← .r16 heightmap tiles + tiles_manifest.json
    outputs/audio/           ← IR_*.wav impulse responses
    outputs/atmosphere/      ← sky_lut_*.json, lunar_calendar_*.json
    outputs/history/         ← knowledge_graph.ndjson
    outputs/settlements/     ← settlements/*.json (settlement manifests)
    outputs/audio/atmospheric_propagation.json
    outputs/audio/bioacoustic_library.json
    outputs/atmosphere/weather_seasonal_table.json

Output:
    outputs/mobile_package/
        terrain/             ← .r16 tiles
        audio/               ← .wav IRs
        atmosphere/          ← LUT JSONs
        history/             ← knowledge_graph.ndjson
        settlements/         ← settlement JSONs
        manifest.json        ← inventory of all included files + sizes
"""

from __future__ import annotations

import hashlib
import json
import pathlib
import shutil
from dataclasses import dataclass, field
from typing import Optional


# ── Source → destination subdirectory mappings ───────────────────────────────

# (glob_pattern relative to outputs_dir, destination subdir in mobile_package)
_ARTIFACT_GLOBS: list[tuple[str, str]] = [
    ("terrain/*.r16",                        "terrain"),
    ("terrain/tiles_manifest.json",          "terrain"),
    ("audio/IR_*.wav",                       "audio"),
    ("audio/atmospheric_propagation.json",   "audio"),
    ("audio/bioacoustic_library.json",       "audio"),
    ("atmosphere/sky_lut_*.json",            "atmosphere"),
    ("atmosphere/lunar_calendar_*.json",     "atmosphere"),
    ("atmosphere/starfield_*.json",          "atmosphere"),
    ("atmosphere/weather_seasonal_table.json", "atmosphere"),
    ("history/knowledge_graph.ndjson",       "history"),
    ("settlements/**/*.json",                "settlements"),
]

# Files that MUST be present for the package to be considered valid.
_REQUIRED_ARTIFACTS: list[str] = [
    "terrain/tiles_manifest.json",
    "audio/atmospheric_propagation.json",
    "audio/bioacoustic_library.json",
    "atmosphere/weather_seasonal_table.json",
    "history/knowledge_graph.ndjson",
]


@dataclass
class PackageManifest:
    """Inventory written to mobile_package/manifest.json."""

    schema_version: str = "1.0"
    files: list[dict] = field(default_factory=list)
    missing_optional: list[str] = field(default_factory=list)
    missing_required: list[str] = field(default_factory=list)

    def is_valid(self) -> bool:
        return len(self.missing_required) == 0

    def to_dict(self) -> dict:
        return {
            "schema_version": self.schema_version,
            "total_files": len(self.files),
            "missing_optional": self.missing_optional,
            "missing_required": self.missing_required,
            "valid": self.is_valid(),
            "files": self.files,
        }


def _sha256(path: pathlib.Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as fh:
        for chunk in iter(lambda: fh.read(65536), b""):
            h.update(chunk)
    return h.hexdigest()


def _copy_file(src: pathlib.Path, dst: pathlib.Path) -> dict:
    """Copy src → dst; create parent dirs; return manifest entry."""
    dst.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(src, dst)
    return {
        "path": dst.name,
        "subdir": str(dst.parent.name),
        "size_bytes": src.stat().st_size,
        "sha256": _sha256(src),
    }


def package_mobile_artifacts(
    outputs_dir: pathlib.Path,
    package_dir: Optional[pathlib.Path] = None,
) -> PackageManifest:
    """
    Collect pre-baked artifacts into a mobile package directory.

    Args:
        outputs_dir:  Root of the pipeline outputs/ tree.
        package_dir:  Destination; defaults to outputs_dir / "mobile_package".

    Returns:
        PackageManifest with inventory of copied files and any missing items.
    """
    outputs_dir = pathlib.Path(outputs_dir)
    if package_dir is None:
        package_dir = outputs_dir / "mobile_package"

    package_dir.mkdir(parents=True, exist_ok=True)

    manifest = PackageManifest()
    found_dest_paths: set[str] = set()

    for glob_pattern, dest_subdir in _ARTIFACT_GLOBS:
        dest_root = package_dir / dest_subdir
        matched = sorted(outputs_dir.glob(glob_pattern))

        for src_path in matched:
            if src_path.is_file():
                dst_path = dest_root / src_path.name
                entry = _copy_file(src_path, dst_path)
                entry["subdir"] = dest_subdir
                entry["path"] = f"{dest_subdir}/{src_path.name}"
                manifest.files.append(entry)
                found_dest_paths.add(entry["path"])

    # Check required artifacts
    for required_rel in _REQUIRED_ARTIFACTS:
        if required_rel not in found_dest_paths:
            manifest.missing_required.append(required_rel)

    # Write manifest
    manifest_path = package_dir / "manifest.json"
    manifest_path.write_text(
        json.dumps(manifest.to_dict(), indent=2, ensure_ascii=False),
        encoding="utf-8",
    )

    return manifest


def main() -> int:
    """CLI entry point: package_mobile_artifacts.py [outputs_dir] [package_dir]"""
    import sys

    outputs_dir = pathlib.Path(sys.argv[1]) if len(sys.argv) > 1 else pathlib.Path("outputs")
    package_dir = pathlib.Path(sys.argv[2]) if len(sys.argv) > 2 else None

    manifest = package_mobile_artifacts(outputs_dir, package_dir)

    print(f"Packaged {len(manifest.files)} files")
    if manifest.missing_required:
        print(f"MISSING REQUIRED: {manifest.missing_required}", file=sys.stderr)
        return 1
    if manifest.missing_optional:
        print(f"Missing optional: {manifest.missing_optional}")
    print("Package valid." if manifest.is_valid() else "Package INVALID.")
    return 0 if manifest.is_valid() else 1


if __name__ == "__main__":
    raise SystemExit(main())
