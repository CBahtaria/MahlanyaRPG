"""
Build-time validator for Phase 10 config files.
Run before UE5 compilation to catch missing entries early.
Exit 0 = all checks pass. Exit 1 = any check fails.
"""

import json
import re
from pathlib import Path


def validate_ini_sections(ini_path: str, required_sections: list[str]) -> list[str]:
    """Returns list of missing section headers from the INI file."""
    with open(ini_path, "r", encoding="utf-8") as f:
        content = f.read()

    # Collect all [SectionName] headers found in the file
    found = set()
    for line in content.splitlines():
        stripped = line.strip()
        if stripped.startswith("[") and stripped.endswith("]"):
            found.add(stripped[1:-1])

    return [s for s in required_sections if s not in found]


def validate_json_configs(json_paths: list[str]) -> list[str]:
    """Returns list of invalid/missing JSON file paths."""
    invalid = []
    for path in json_paths:
        try:
            with open(path, "r", encoding="utf-8") as f:
                json.load(f)
        except (FileNotFoundError, json.JSONDecodeError):
            invalid.append(path)
    return invalid


def validate_device_profiles(game_ini_path: str, min_count: int = 10) -> bool:
    """Returns True if >=min_count DeviceProfileName entries found in the INI."""
    with open(game_ini_path, "r", encoding="utf-8") as f:
        content = f.read()

    count = len(re.findall(r"DeviceProfileName=", content))
    return count >= min_count


def validate_cvar_count(cpp_path: str, min_count: int = 14) -> bool:
    """Counts TAutoConsoleVariable occurrences in the .cpp file. Returns True if >=min_count."""
    with open(cpp_path, "r", encoding="utf-8") as f:
        content = f.read()

    count = len(re.findall(r"TAutoConsoleVariable", content))
    return count >= min_count


def validate_log_channel_count(header_path: str, min_count: int = 8) -> bool:
    """Counts DECLARE_LOG_CATEGORY_EXTERN occurrences in the header. Returns True if >=min_count."""
    with open(header_path, "r", encoding="utf-8") as f:
        content = f.read()

    count = len(re.findall(r"DECLARE_LOG_CATEGORY_EXTERN", content))
    return count >= min_count


def validate_subsystem_registrations(engine_ini_path: str) -> list[str]:
    """Returns list of required subsystem class names missing from DefaultEngine.ini."""
    required = ["UHardwareAdaptiveScaler", "UDynamicRuntimeThrottle", "UYearChangeOrchestrator"]

    with open(engine_ini_path, "r", encoding="utf-8") as f:
        content = f.read()

    return [cls for cls in required if cls not in content]


def validate_privacy_manifests() -> list[str]:
    """Returns list of errors if privacy manifests are missing or malformed."""
    errors = []

    ios_privacy = Path("config/builds/mobile/ios/PrivacyInfo.xcprivacy")
    if not ios_privacy.exists():
        errors.append("MISSING: config/builds/mobile/ios/PrivacyInfo.xcprivacy (required for App Store)")
    else:
        content = ios_privacy.read_text()
        if "NSPrivacyTracking" not in content:
            errors.append("INVALID: PrivacyInfo.xcprivacy missing NSPrivacyTracking key")
        if "NSPrivacyAccessedAPITypes" not in content:
            errors.append("INVALID: PrivacyInfo.xcprivacy missing NSPrivacyAccessedAPITypes key")

    android_manifest = Path("config/builds/mobile/android/AndroidManifest.additions.xml")
    if not android_manifest.exists():
        errors.append("MISSING: config/builds/mobile/android/AndroidManifest.additions.xml")

    return errors


def run_all_checks() -> bool:
    """Runs all validators, prints [PASS]/[FAIL] per check, returns True if no errors."""
    all_passed = True

    # Check 1: Required INI sections in DefaultEngine.ini
    missing = validate_ini_sections("Config/DefaultEngine.ini", [
        "/Script/Engine.Engine",
        "/Script/Engine.WorldSettings",
        "/Script/Engine.GameNetworkManager",
    ])
    if missing:
        print(f"[FAIL] DefaultEngine.ini missing sections: {missing}")
        all_passed = False
    else:
        print("[PASS] DefaultEngine.ini required sections present")

    # Check 2: Required INI sections in DefaultScalability.ini
    missing = validate_ini_sections("Config/DefaultScalability.ini", [
        "MahlanyaSimulation@0",
        "MahlanyaSimulation@1",
        "MahlanyaSimulation@2",
        "MahlanyaSimulation@3",
    ])
    if missing:
        print(f"[FAIL] DefaultScalability.ini missing sections: {missing}")
        all_passed = False
    else:
        print("[PASS] DefaultScalability.ini MahlanyaSimulation groups present")

    # Check 3: JSON config validity
    invalid = validate_json_configs([
        "Plugins/MicroclimateEngine/Config/MicroclimatePerformance.json",
        "Plugins/GeometricAudioPlugin/Config/AudioPerformance.json",
    ])
    if invalid:
        print(f"[FAIL] Invalid/missing JSON configs: {invalid}")
        all_passed = False
    else:
        print("[PASS] Plugin JSON configs valid")

    # Check 4: Device profile count
    if not validate_device_profiles("Config/DefaultGame.ini", min_count=10):
        print("[FAIL] DefaultGame.ini has fewer than 10 DeviceProfileName entries")
        all_passed = False
    else:
        print("[PASS] Device profiles count >= 10")

    # Check 5: CVar count
    if not validate_cvar_count(
        "Source/MahlanyaRPG/Performance/MahlanyaPerformanceCVars.cpp", min_count=14
    ):
        print("[FAIL] MahlanyaPerformanceCVars.cpp has fewer than 14 TAutoConsoleVariable registrations")
        all_passed = False
    else:
        print("[PASS] CVar count >= 14")

    # Check 6: Log channel count
    if not validate_log_channel_count(
        "Source/MahlanyaRPG/Core/MahlanyaLogChannels.h", min_count=8
    ):
        print("[FAIL] MahlanyaLogChannels.h has fewer than 8 DECLARE_LOG_CATEGORY_EXTERN")
        all_passed = False
    else:
        print("[PASS] Log channel count >= 8")

    # Check 7: Subsystem registrations
    missing = validate_subsystem_registrations("Config/DefaultEngine.ini")
    if missing:
        print(f"[FAIL] DefaultEngine.ini missing subsystem registrations: {missing}")
        all_passed = False
    else:
        print("[PASS] All subsystem registrations present")

    # Check 8: Privacy manifests (iOS App Store + Android)
    errors = validate_privacy_manifests()
    if errors:
        for err in errors:
            print(f"[FAIL] {err}")
        all_passed = False
    else:
        print("[PASS] Mobile privacy manifests present and valid")

    if all_passed:
        print("\n[PASS] All config checks passed.")
    else:
        print("\n[FAIL] One or more config checks failed.")

    return all_passed


if __name__ == "__main__":
    import sys
    sys.exit(0 if run_all_checks() else 1)
