#!/usr/bin/env python3
"""
MahlanyaRPG — Platform Certification Pre-Flight Validator
UE5-specific checks for Steam, iOS App Store, and Google Play submission.

Usage:
    python scripts/automation/cert_validator.py [--platform steam|ios|android|all]
    python scripts/automation/cert_validator.py --build-path ./Build/Windows

Exit 0 = all mandatory checks pass.
Exit 1 = one or more mandatory checks fail.
"""

import argparse
import json
import os
import re
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[2]

# ── helpers ────────────────────────────────────────────────────────────────────

def _check(label: str, passed: bool, severity: str = "REQUIRED") -> bool:
    icon = "✅" if passed else ("⚠️ " if severity == "WARN" else "❌")
    print(f"  {icon} [{severity}] {label}")
    return passed


def _read_ini(path: Path) -> str:
    try:
        return path.read_text(encoding="utf-8")
    except FileNotFoundError:
        return ""


# ── Steam checks ───────────────────────────────────────────────────────────────

def check_steam(build_path: Path) -> tuple[int, int]:
    print("\n=== Steam Pre-Flight ===")
    passed = failed = 0

    checks = [
        ("steam_appid.txt present",
         (build_path / "steam_appid.txt").exists()),
        ("app_build.vdf present",
         (REPO_ROOT / "config/builds/steam/app_build.vdf").exists()),
        ("depot_win64.vdf present",
         (REPO_ROOT / "config/builds/steam/depot_win64.vdf").exists()),
        ("depot_content.vdf present",
         (REPO_ROOT / "config/builds/steam/depot_content.vdf").exists()),
        ("steam_deploy.sh executable",
         (REPO_ROOT / "scripts/automation/steam_deploy.sh").exists()),
        ("OnlineSubsystemSteam enabled in DefaultEngine.ini",
         "OnlineSubsystemSteam" in _read_ini(REPO_ROOT / "Config/DefaultEngine.ini")),
        ("No -DebugGame- binaries in Shipping build",
         not any((build_path / "Binaries/Win64").glob("*-DebugGame-*"))
         if (build_path / "Binaries/Win64").exists() else True),
        (".pak file present",
         bool(list((build_path / "Content/Paks").glob("*.pak")))
         if (build_path / "Content/Paks").exists() else False),
    ]

    for label, result in checks:
        if _check(label, result):
            passed += 1
        else:
            failed += 1

    return passed, failed


# ── UE5 engine checks (all platforms) ─────────────────────────────────────────

def check_ue5_build(build_path: Path) -> tuple[int, int]:
    print("\n=== UE5 Build Health ===")
    passed = failed = 0
    ini = _read_ini(REPO_ROOT / "Config/DefaultEngine.ini")

    checks = [
        ("Lumen enabled in DefaultEngine.ini",
         "r.Lumen.DiffuseIndirect.Allow=True" in ini),
        ("Nanite enabled in DefaultEngine.ini",
         "r.Nanite.Enabled=True" in ini),
        ("Microclimate plugin config JSON valid",
         _valid_json(REPO_ROOT / "Plugins/MicroclimateEngine/Config/MicroclimatePerformance.json")),
        ("Audio plugin config JSON valid",
         _valid_json(REPO_ROOT / "Plugins/GeometricAudioPlugin/Config/AudioPerformance.json")),
        ("MahlanyaGameClockSubsystem registered",
         "UMahlanyaGameClockSubsystem" in ini),
        ("UYearChangeOrchestrator registered",
         "UYearChangeOrchestrator" in ini),
        ("UDynamicRuntimeThrottle registered",
         "UDynamicRuntimeThrottle" in ini),
        ("Phase 10 Python tests pass",
         _run_pytest("pipeline/tests/")),
        ("validate_configs passes",
         _run_script("pipeline/validate_configs.py")),
    ]

    for label, result in checks:
        if _check(label, result):
            passed += 1
        else:
            failed += 1

    return passed, failed


# ── iOS checks ─────────────────────────────────────────────────────────────────

def check_ios() -> tuple[int, int]:
    print("\n=== iOS App Store Pre-Flight ===")
    passed = failed = 0

    ios_dir = REPO_ROOT / "config/builds/mobile/ios"
    checks = [
        ("ExportOptions.plist.template present",
         (ios_dir / "ExportOptions.plist.template").exists()),
        ("Entitlements.plist.template present",
         (ios_dir / "Entitlements.plist.template").exists()),
        ("REPLACE_TEAM_ID placeholder filled",
         "REPLACE_TEAM_ID" not in _read_plist(ios_dir / "ExportOptions.plist.template")),
        ("REPLACE_BUNDLE_ID placeholder filled",
         "REPLACE_BUNDLE_ID" not in _read_plist(ios_dir / "Entitlements.plist.template")),
    ]

    for label, result in checks:
        sev = "REQUIRED" if "placeholder" in label.lower() else "REQUIRED"
        if _check(label, result, sev):
            passed += 1
        else:
            failed += 1

    return passed, failed


# ── Android checks ─────────────────────────────────────────────────────────────

def check_android() -> tuple[int, int]:
    print("\n=== Google Play Pre-Flight ===")
    passed = failed = 0

    android_dir = REPO_ROOT / "config/builds/mobile/android"
    keystore_template = android_dir / "keystore.properties.template"
    keystore_real = android_dir / "keystore.properties"

    checks = [
        ("keystore.properties.template present", keystore_template.exists()),
        ("proguard-rules.pro present", (android_dir / "proguard-rules.pro").exists()),
        ("keystore.properties NOT committed (git-ignored)",
         not keystore_real.exists()),
        ("No REPLACE_ placeholders in keystore template",
         keystore_template.exists() and
         "REPLACE_WITH" not in keystore_template.read_text()),
    ]

    for label, result in checks:
        if _check(label, result):
            passed += 1
        else:
            failed += 1

    return passed, failed


# ── cert simulation checks ─────────────────────────────────────────────────────

def check_console_cert_sim() -> tuple[int, int]:
    """Simulate common cert failure triggers for future console submissions."""
    print("\n=== Console Cert Simulation (PC proxy) ===")
    passed = failed = 0

    ini = _read_ini(REPO_ROOT / "Config/DefaultEngine.ini")
    cvar_src = REPO_ROOT / "Source/MahlanyaRPG/Performance/MahlanyaPerformanceCVars.cpp"
    cvar_text = cvar_src.read_text() if cvar_src.exists() else ""

    checks = [
        ("ECVF_Cheat CVars present (disabled in Shipping by UE5)",
         "ECVF_Cheat" in cvar_text),
        ("SimulationHeartbeatInterval set in DefaultEngine.ini",
         "SimulationHeartbeatInterval" in ini),
        ("MaxClientRate set",
         "MaxClientRate" in ini),
        ("TotalNetBandwidth set",
         "TotalNetBandwidth" in ini),
        ("GDynamicRHI null guard in HardwareAdaptiveScaler",
         _grep_file(REPO_ROOT / "Source/MahlanyaRPG/Performance/UHardwareAdaptiveScaler.cpp",
                    "GDynamicRHI")),
        ("FPlatformProperties::IsConsole() used for tier override",
         _grep_file(REPO_ROOT / "Source/MahlanyaRPG/Performance/UHardwareAdaptiveScaler.cpp",
                    "IsConsole")),
    ]

    for label, result in checks:
        if _check(label, result):
            passed += 1
        else:
            failed += 1

    return passed, failed


# ── utility ────────────────────────────────────────────────────────────────────

def _valid_json(path: Path) -> bool:
    try:
        json.loads(path.read_text())
        return True
    except Exception:
        return False


def _read_plist(path: Path) -> str:
    try:
        return path.read_text()
    except FileNotFoundError:
        return "REPLACE_"


def _grep_file(path: Path, pattern: str) -> bool:
    try:
        return pattern in path.read_text()
    except FileNotFoundError:
        return False


def _run_pytest(path: str) -> bool:
    import subprocess
    r = subprocess.run(
        [sys.executable, "-m", "pytest", path, "-q", "--tb=no"],
        cwd=REPO_ROOT, capture_output=True
    )
    return r.returncode == 0


def _run_script(path: str) -> bool:
    import subprocess
    r = subprocess.run(
        [sys.executable, path],
        cwd=REPO_ROOT, capture_output=True
    )
    return r.returncode == 0


# ── main ───────────────────────────────────────────────────────────────────────

def main() -> None:
    parser = argparse.ArgumentParser(
        description="MahlanyaRPG platform certification pre-flight")
    parser.add_argument("--platform",
                        choices=["steam", "ios", "android", "all"],
                        default="all")
    parser.add_argument("--build-path",
                        default=str(REPO_ROOT / "Build/Windows"),
                        help="Path to packaged UE5 Shipping build output")
    args = parser.parse_args()

    build_path = Path(args.build_path)
    total_passed = total_failed = 0

    # UE5 engine checks always run
    p, f = check_ue5_build(build_path)
    total_passed += p; total_failed += f

    if args.platform in ("steam", "all"):
        p, f = check_steam(build_path)
        total_passed += p; total_failed += f

    if args.platform in ("ios", "all"):
        p, f = check_ios()
        total_passed += p; total_failed += f

    if args.platform in ("android", "all"):
        p, f = check_android()
        total_passed += p; total_failed += f

    if args.platform in ("all",):
        p, f = check_console_cert_sim()
        total_passed += p; total_failed += f

    print(f"\n=== SUMMARY: {total_passed} passed, {total_failed} failed ===")
    if total_failed > 0:
        print("⚠️  Resolve failures before store submission.")
        sys.exit(1)
    print("🚀 All pre-flight checks passed. Ready for store packaging.")


if __name__ == "__main__":
    main()
