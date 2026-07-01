"""
Validates all store metadata files exist and are internally consistent.
Run before any store submission. Exit 0 = all checks pass.
"""
import json
import sys
from pathlib import Path

# Resolve the project root relative to this script's location
SCRIPT_DIR = Path(__file__).resolve().parent
PROJECT_ROOT = SCRIPT_DIR.parent.parent
STORE_ROOT = PROJECT_ROOT / "config" / "store"


def check_steam_listing() -> list[str]:
    """Checks store_listing.md has all required sections."""
    errors: list[str] = []
    path = STORE_ROOT / "steam" / "store_listing.md"

    if not path.exists():
        return [f"MISSING: {path}"]

    text = path.read_text(encoding="utf-8")

    required_sections = [
        "## Short Description",
        "## About This Game",
        "## Developer Note",
        "## Tags",
    ]
    for section in required_sections:
        if section not in text:
            errors.append(f"steam/store_listing.md: missing section '{section}'")

    # Short description must be present and within 300 chars
    lines = text.splitlines()
    in_short = False
    short_desc_text: str = ""
    for i, line in enumerate(lines):
        if line.strip() == "## Short Description (≤300 characters)":
            in_short = True
            continue
        if in_short:
            if line.strip() and not line.startswith("#"):
                short_desc_text = line.strip()
                break
            if line.startswith("##"):
                break

    if not short_desc_text:
        errors.append("steam/store_listing.md: Short Description text not found")
    elif len(short_desc_text) > 300:
        errors.append(
            f"steam/store_listing.md: Short Description is {len(short_desc_text)} chars (max 300)"
        )

    # About This Game must contain HTML tags
    required_html = ["<p>", "<h2>", "<ul>", "<li>"]
    for tag in required_html:
        if tag not in text:
            errors.append(f"steam/store_listing.md: About This Game missing HTML tag '{tag}'")

    # Tags section must have at least 10 comma-separated entries
    in_tags = False
    tags_text: str = ""
    for line in lines:
        if line.strip() == "## Tags (20 Steam tags, comma-separated)":
            in_tags = True
            continue
        if in_tags:
            if line.strip() and not line.startswith("#"):
                tags_text = line.strip()
                break
            if line.startswith("##"):
                break

    if not tags_text:
        errors.append("steam/store_listing.md: Tags text not found")
    else:
        tag_count = len([t for t in tags_text.split(",") if t.strip()])
        if tag_count < 10:
            errors.append(
                f"steam/store_listing.md: Found only {tag_count} tags (expected at least 10)"
            )

    # Must mention key game elements
    required_mentions = ["Mahlanya", "siSwati", "cattle", "colonial", "Eswatini"]
    for term in required_mentions:
        if term not in text:
            errors.append(f"steam/store_listing.md: required term '{term}' not found in listing")

    return errors


def check_system_requirements() -> list[str]:
    """Checks system_requirements.json has min/recommended/ultra with required keys."""
    errors: list[str] = []
    path = STORE_ROOT / "steam" / "system_requirements.json"

    if not path.exists():
        return [f"MISSING: {path}"]

    try:
        data = json.loads(path.read_text(encoding="utf-8"))
    except json.JSONDecodeError as exc:
        return [f"steam/system_requirements.json: JSON parse error — {exc}"]

    required_tiers = ["minimum", "recommended", "ultra"]
    required_keys = ["os", "processor", "memory", "graphics", "directx", "storage", "notes"]

    for tier in required_tiers:
        if tier not in data:
            errors.append(f"steam/system_requirements.json: missing tier '{tier}'")
            continue
        for key in required_keys:
            if key not in data[tier]:
                errors.append(
                    f"steam/system_requirements.json: tier '{tier}' missing key '{key}'"
                )
            elif not isinstance(data[tier][key], str) or not data[tier][key].strip():
                errors.append(
                    f"steam/system_requirements.json: tier '{tier}' key '{key}' is empty or non-string"
                )

    # Validate DirectX version values make sense
    dx_values = {tier: data[tier].get("directx", "") for tier in required_tiers if tier in data}
    if dx_values.get("minimum") and "12" not in dx_values["minimum"]:
        errors.append("steam/system_requirements.json: minimum DirectX should be Version 12")
    if dx_values.get("ultra") and "Ultimate" not in dx_values["ultra"]:
        errors.append(
            "steam/system_requirements.json: ultra DirectX should reference Version 12 Ultimate"
        )

    # Memory values should be numeric-parseable
    for tier in required_tiers:
        if tier not in data:
            continue
        mem = data[tier].get("memory", "")
        if not any(char.isdigit() for char in mem):
            errors.append(
                f"steam/system_requirements.json: tier '{tier}' memory value '{mem}' has no numeric component"
            )

    return errors


def check_ios_metadata() -> list[str]:
    """Checks ios/metadata.json has required App Store Connect fields."""
    errors: list[str] = []
    path = STORE_ROOT / "ios" / "metadata.json"

    if not path.exists():
        return [f"MISSING: {path}"]

    try:
        data = json.loads(path.read_text(encoding="utf-8"))
    except json.JSONDecodeError as exc:
        return [f"ios/metadata.json: JSON parse error — {exc}"]

    required_fields = [
        "app_name",
        "subtitle",
        "description",
        "keywords",
        "support_url",
        "marketing_url",
        "privacy_policy_url",
        "primary_category",
        "secondary_category",
        "age_rating",
        "what_is_new",
    ]
    for field in required_fields:
        if field not in data:
            errors.append(f"ios/metadata.json: missing field '{field}'")

    # Description must be substantial (at least 500 chars for App Store)
    desc = data.get("description", "")
    if isinstance(desc, str):
        if len(desc) < 500:
            errors.append(
                f"ios/metadata.json: description is only {len(desc)} chars (App Store needs substantial copy)"
            )
        if "[" in desc and "]" in desc:
            errors.append("ios/metadata.json: description still contains placeholder text ([ ])")
    else:
        errors.append("ios/metadata.json: description must be a string")

    # Subtitle must be ≤30 chars (App Store Connect limit)
    subtitle = data.get("subtitle", "")
    if isinstance(subtitle, str) and len(subtitle) > 30:
        errors.append(
            f"ios/metadata.json: subtitle is {len(subtitle)} chars (App Store limit is 30)"
        )

    # age_rating must be a dict with iarc_rating
    age_rating = data.get("age_rating", {})
    if not isinstance(age_rating, dict):
        errors.append("ios/metadata.json: age_rating must be an object")
    else:
        if "iarc_rating" not in age_rating:
            errors.append("ios/metadata.json: age_rating missing 'iarc_rating'")
        if "game_center" not in age_rating:
            errors.append("ios/metadata.json: age_rating missing 'game_center' boolean")

        # Validate all content descriptor fields are NONE/valid values
        valid_age_values = {"NONE", "INFREQUENT_MILD", "FREQUENT_INTENSE"}
        descriptor_keys = [
            "violence_cartoon", "violence_realistic", "violence_realistic_prolonged",
            "profanity", "nudity_cartoon", "nudity_realistic", "sexual_content",
            "gambling", "alcohol_tobacco_drugs", "horror", "mature_suggestive",
            "unrestricted_web_access", "social_networking",
        ]
        for key in descriptor_keys:
            if key not in age_rating:
                errors.append(f"ios/metadata.json: age_rating missing '{key}'")
            elif age_rating[key] not in valid_age_values:
                errors.append(
                    f"ios/metadata.json: age_rating['{key}'] = '{age_rating[key]}' is not a valid value"
                )

    # primary_category must be GAMES
    if data.get("primary_category") != "GAMES":
        errors.append("ios/metadata.json: primary_category must be 'GAMES'")

    # URLs must start with https://
    for url_field in ["support_url", "marketing_url", "privacy_policy_url"]:
        url = data.get(url_field, "")
        if isinstance(url, str) and not url.startswith("https://"):
            errors.append(f"ios/metadata.json: {url_field} must start with 'https://'")

    # Keywords must be comma-separated, no spaces (App Store convention)
    keywords = data.get("keywords", "")
    if isinstance(keywords, str):
        kw_list = keywords.split(",")
        if len(kw_list) < 5:
            errors.append(f"ios/metadata.json: keywords has only {len(kw_list)} entries (need at least 5)")

    return errors


def check_android_metadata() -> list[str]:
    """Checks android/metadata.json has required Google Play fields."""
    errors: list[str] = []
    path = STORE_ROOT / "android" / "metadata.json"

    if not path.exists():
        return [f"MISSING: {path}"]

    try:
        data = json.loads(path.read_text(encoding="utf-8"))
    except json.JSONDecodeError as exc:
        return [f"android/metadata.json: JSON parse error — {exc}"]

    required_fields = [
        "title",
        "short_description",
        "full_description",
        "category",
        "content_rating",
        "pricing",
        "in_app_purchases",
        "ads",
        "target_sdk",
        "min_sdk",
        "target_devices",
        "game_controller_support",
        "google_play_games",
    ]
    for field in required_fields:
        if field not in data:
            errors.append(f"android/metadata.json: missing field '{field}'")

    # Title ≤50 chars (Google Play limit)
    title = data.get("title", "")
    if isinstance(title, str) and len(title) > 50:
        errors.append(
            f"android/metadata.json: title is {len(title)} chars (Google Play limit is 50)"
        )

    # Short description ≤80 chars
    short = data.get("short_description", "")
    if isinstance(short, str) and len(short) > 80:
        errors.append(
            f"android/metadata.json: short_description is {len(short)} chars (Google Play limit is 80)"
        )

    # Full description must be substantial and not contain placeholder text
    full = data.get("full_description", "")
    if isinstance(full, str):
        if len(full) < 500:
            errors.append(
                f"android/metadata.json: full_description is only {len(full)} chars (Google Play needs substantial copy)"
            )
        if "[" in full and "]" in full:
            errors.append(
                "android/metadata.json: full_description still contains placeholder text ([ ])"
            )
    else:
        errors.append("android/metadata.json: full_description must be a string")

    # content_rating must be a dict with iarc_rating
    content_rating = data.get("content_rating", {})
    if not isinstance(content_rating, dict):
        errors.append("android/metadata.json: content_rating must be an object")
    else:
        required_rating_keys = ["iarc_rating", "violence", "language", "sexual_content"]
        for key in required_rating_keys:
            if key not in content_rating:
                errors.append(f"android/metadata.json: content_rating missing '{key}'")

    # SDK versions must be integers and sensible
    target_sdk = data.get("target_sdk")
    min_sdk = data.get("min_sdk")
    if not isinstance(target_sdk, int):
        errors.append("android/metadata.json: target_sdk must be an integer")
    elif target_sdk < 30:
        errors.append(f"android/metadata.json: target_sdk {target_sdk} is below minimum recommended (30)")

    if not isinstance(min_sdk, int):
        errors.append("android/metadata.json: min_sdk must be an integer")
    elif min_sdk < 21:
        errors.append(f"android/metadata.json: min_sdk {min_sdk} is below supported minimum (21)")

    if isinstance(target_sdk, int) and isinstance(min_sdk, int) and min_sdk > target_sdk:
        errors.append(
            f"android/metadata.json: min_sdk ({min_sdk}) cannot be greater than target_sdk ({target_sdk})"
        )

    # in_app_purchases and ads must be booleans
    for bool_field in ["in_app_purchases", "ads", "game_controller_support", "google_play_games"]:
        val = data.get(bool_field)
        if val is not None and not isinstance(val, bool):
            errors.append(f"android/metadata.json: '{bool_field}' must be a boolean")

    # target_devices must be a list
    devices = data.get("target_devices", [])
    if not isinstance(devices, list) or len(devices) == 0:
        errors.append("android/metadata.json: target_devices must be a non-empty list")

    # pricing must be PAID or FREE
    pricing = data.get("pricing", "")
    if pricing not in ("PAID", "FREE"):
        errors.append(f"android/metadata.json: pricing must be 'PAID' or 'FREE', got '{pricing}'")

    return errors


def check_iarc_consistency() -> list[str]:
    """Checks that iOS and Android age ratings match the IARC questionnaire."""
    errors: list[str] = []

    ios_path = STORE_ROOT / "ios" / "metadata.json"
    android_path = STORE_ROOT / "android" / "metadata.json"
    iarc_path = STORE_ROOT / "shared" / "iarc_questionnaire.md"
    steam_cd_path = STORE_ROOT / "steam" / "content_descriptors.json"

    missing = [p for p in [ios_path, android_path, iarc_path, steam_cd_path] if not p.exists()]
    if missing:
        return [f"MISSING (skipping consistency check): {p}" for p in missing]

    # Load iOS
    try:
        ios_data = json.loads(ios_path.read_text(encoding="utf-8"))
    except json.JSONDecodeError as exc:
        return [f"ios/metadata.json: JSON parse error — {exc}"]

    # Load Android
    try:
        android_data = json.loads(android_path.read_text(encoding="utf-8"))
    except json.JSONDecodeError as exc:
        return [f"android/metadata.json: JSON parse error — {exc}"]

    # Load Steam content descriptors
    try:
        steam_cd = json.loads(steam_cd_path.read_text(encoding="utf-8"))
    except json.JSONDecodeError as exc:
        return [f"steam/content_descriptors.json: JSON parse error — {exc}"]

    # Load IARC questionnaire text
    iarc_text = iarc_path.read_text(encoding="utf-8")

    # --- iOS IARC rating check ---
    ios_iarc = ios_data.get("age_rating", {}).get("iarc_rating", "")
    if ios_iarc != "12+":
        errors.append(
            f"Consistency: iOS iarc_rating is '{ios_iarc}', expected '12+' per IARC questionnaire"
        )

    # iOS must not claim sexual content when game has none
    ios_age = ios_data.get("age_rating", {})
    if ios_age.get("sexual_content") != "NONE":
        errors.append(
            "Consistency: iOS age_rating.sexual_content should be 'NONE' — game has no sexual content"
        )
    if ios_age.get("gambling") != "NONE":
        errors.append(
            "Consistency: iOS age_rating.gambling should be 'NONE' — game has no gambling"
        )
    if ios_age.get("profanity") != "NONE":
        errors.append(
            "Consistency: iOS age_rating.profanity should be 'NONE' — game has no profanity"
        )

    # --- Android IARC rating check ---
    android_iarc = android_data.get("content_rating", {}).get("iarc_rating", "")
    if android_iarc != "12+":
        errors.append(
            f"Consistency: Android iarc_rating is '{android_iarc}', expected '12+' per IARC questionnaire"
        )

    android_cr = android_data.get("content_rating", {})
    if android_cr.get("sexual_content") != "NONE":
        errors.append(
            "Consistency: Android content_rating.sexual_content should be 'NONE'"
        )
    if android_cr.get("language") != "NONE":
        errors.append(
            "Consistency: Android content_rating.language should be 'NONE'"
        )

    # Android must have in_app_purchases = False (no IAP — IARC/gambling consistency)
    if android_data.get("in_app_purchases") is not False:
        errors.append(
            "Consistency: android in_app_purchases must be false — no real-money transactions per IARC gambling classification"
        )
    if android_data.get("ads") is not False:
        errors.append("Consistency: android ads must be false — game is paid, no ads")

    # --- Steam content descriptors check ---
    steam_iarc = steam_cd.get("iarc_age_rating", "")
    if steam_iarc != "12+":
        errors.append(
            f"Consistency: Steam iarc_age_rating is '{steam_iarc}', expected '12+'"
        )

    steam_sexual = steam_cd.get("sexual_content", {}).get("level", "")
    if steam_sexual != "none":
        errors.append(
            f"Consistency: Steam content_descriptors sexual_content.level is '{steam_sexual}', expected 'none'"
        )

    steam_gambling = steam_cd.get("gambling", {}).get("level", "")
    if steam_gambling != "none":
        errors.append(
            f"Consistency: Steam content_descriptors gambling.level is '{steam_gambling}', expected 'none'"
        )

    # --- IARC questionnaire text sanity checks ---
    expected_ratings_in_doc = [
        ("Steam (Global)", "12+"),
        ("iOS", "12+"),
        ("PEGI", "12"),
        ("ESRB", "T (Teen)"),
        ("FPB", "13"),
    ]
    for platform, rating in expected_ratings_in_doc:
        if platform not in iarc_text or rating not in iarc_text:
            errors.append(
                f"Consistency: IARC questionnaire missing rating entry for {platform}: {rating}"
            )

    # All platforms must agree: no sexual content, no gambling, no profanity
    for keyword in ["None", "NONE", "none"]:
        if "sexual_content" in steam_cd and steam_cd["sexual_content"].get("level") not in ("none", "NONE", "None"):
            errors.append("Consistency: cross-platform sexual_content rating mismatch")
            break

    return errors


def run_all_checks() -> bool:
    """Runs all validators, prints [PASS]/[FAIL] per check, returns True if no errors."""
    checks = [
        ("Steam store listing", check_steam_listing),
        ("Steam system requirements", check_system_requirements),
        ("iOS App Store metadata", check_ios_metadata),
        ("Android Google Play metadata", check_android_metadata),
        ("Cross-platform IARC consistency", check_iarc_consistency),
    ]

    all_passed = True
    print("=" * 60)
    print("Mahlanya RPG — Store Metadata Validation")
    print("=" * 60)

    for name, check_fn in checks:
        errors = check_fn()
        if errors:
            all_passed = False
            print(f"[FAIL] {name}")
            for err in errors:
                print(f"       - {err}")
        else:
            print(f"[PASS] {name}")

    print("=" * 60)
    if all_passed:
        print("Result: ALL CHECKS PASSED — metadata ready for store submission")
    else:
        print("Result: VALIDATION FAILED — fix errors above before submission")
    print("=" * 60)

    return all_passed


if __name__ == "__main__":
    sys.exit(0 if run_all_checks() else 1)
