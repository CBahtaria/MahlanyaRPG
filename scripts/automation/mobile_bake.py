#!/usr/bin/env python3
"""
MahlanyaRPG Mobile Asset Bake Script
Prepares the pre-baked artifact tier for iOS and Android distribution.

Usage:
    python scripts/automation/mobile_bake.py --platform android
    python scripts/automation/mobile_bake.py --platform ios
    python scripts/automation/mobile_bake.py --platform all

The mobile tier consumes pre-baked pipeline outputs (heightmaps, LUTs, IRs)
rather than running runtime compute. This script compresses them to mobile specs.
"""

import argparse
import os
import shutil
import subprocess
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[2]
CONTENT_DIR = REPO_ROOT / "Content"
BUILD_DIR = REPO_ROOT / "Build"
PIPELINE_OUTPUTS = REPO_ROOT / "pipeline" / "outputs"

# ASTC compression quality target for mobile: high quality / smaller size tradeoff
ASTC_QUALITY = "medium"  # options: fastest, fast, medium, thorough, exhaustive

# UE5 package commandlet path (set UNREAL_ENGINE_ROOT env var)
UE_ROOT = os.environ.get("UNREAL_ENGINE_ROOT", "/opt/UnrealEngine")
UE_CMD = f"{UE_ROOT}/Engine/Binaries/Linux/UnrealEditor-Cmd"


def check_prerequisites() -> list[str]:
    """Return list of missing prerequisites."""
    missing = []
    if not Path(UE_CMD).exists():
        missing.append(f"UnrealEditor-Cmd not found at {UE_CMD} (set UNREAL_ENGINE_ROOT)")
    if shutil.which("astcenc") is None:
        missing.append("astcenc not on PATH (install: apt install astc-encoder)")
    return missing


def bake_textures_astc(source_dir: Path, output_dir: Path) -> int:
    """Compress EXR/PNG textures to ASTC for mobile GPU."""
    output_dir.mkdir(parents=True, exist_ok=True)
    count = 0
    for src in source_dir.rglob("*.exr"):
        dst = output_dir / src.relative_to(source_dir).with_suffix(".astc")
        dst.parent.mkdir(parents=True, exist_ok=True)
        subprocess.run(
            ["astcenc", "-cl", str(src), str(dst), "6x6", "-medium"],
            check=True, capture_output=True
        )
        count += 1
        print(f"  Compressed: {src.name} → {dst.name}")
    return count


def package_android(uproject: Path) -> None:
    """Invoke UE5 cook + package for Android."""
    print("\n=== Android Package ===")
    cmd = [
        UE_CMD, str(uproject),
        "-run=BuildCookRun",
        "-nop4", "-utf8output",
        "-platform=Android",
        "-clientconfig=Shipping",
        "-cook", "-map=",
        "-compressed", "-iterate",
        "-stage", "-package",
        "-pak",
        "-distribution",
        "-android_package_data_type=PackageDataInsideApk",
        f"-stagingdirectory={BUILD_DIR}/Android",
        "-prereqs",
    ]
    print(f"Running: {' '.join(cmd[:6])} ...")
    result = subprocess.run(cmd, capture_output=True, text=True)
    if result.returncode != 0:
        print("STDERR:", result.stderr[-2000:])
        sys.exit(1)
    print(f"Android build complete → {BUILD_DIR}/Android")


def package_ios(uproject: Path) -> None:
    """Invoke UE5 cook + package for iOS (requires macOS/Xcode)."""
    print("\n=== iOS Package ===")
    if sys.platform != "darwin":
        print("WARNING: iOS packaging requires macOS. Skipping on non-macOS host.")
        return
    cmd = [
        UE_CMD, str(uproject),
        "-run=BuildCookRun",
        "-nop4", "-utf8output",
        "-platform=IOS",
        "-clientconfig=Shipping",
        "-cook", "-map=",
        "-compressed",
        "-stage", "-package",
        "-pak",
        "-distribution",
        f"-stagingdirectory={BUILD_DIR}/IOS",
        f"-exportoptionsplist={REPO_ROOT}/config/builds/mobile/ios/ExportOptions.plist.template",
    ]
    print(f"Running: {' '.join(cmd[:6])} ...")
    result = subprocess.run(cmd, capture_output=True, text=True)
    if result.returncode != 0:
        print("STDERR:", result.stderr[-2000:])
        sys.exit(1)
    print(f"iOS build complete → {BUILD_DIR}/IOS")


def main() -> None:
    parser = argparse.ArgumentParser(description="MahlanyaRPG mobile asset bake")
    parser.add_argument("--platform", choices=["android", "ios", "all"],
                        default="all", help="Target platform")
    args = parser.parse_args()

    uproject = REPO_ROOT / "MahlanyaRPG.uproject"
    if not uproject.exists():
        print(f"ERROR: .uproject not found at {uproject}")
        sys.exit(1)

    missing = check_prerequisites()
    if missing:
        for m in missing:
            print(f"MISSING: {m}")
        sys.exit(1)

    # Compress pipeline LUT outputs to ASTC for mobile GPU
    lut_src = PIPELINE_OUTPUTS / "atmosphere" / "LUTs"
    if lut_src.exists():
        print(f"\nCompressing LUTs → ASTC ({ASTC_QUALITY})")
        n = bake_textures_astc(lut_src, BUILD_DIR / "mobile_assets" / "LUTs")
        print(f"Compressed {n} LUT textures")

    if args.platform in ("android", "all"):
        package_android(uproject)

    if args.platform in ("ios", "all"):
        package_ios(uproject)

    print("\n=== Mobile bake complete ===")


if __name__ == "__main__":
    main()
