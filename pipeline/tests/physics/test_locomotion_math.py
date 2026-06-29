"""
Phase 2 locomotion physics — pure-Python math validation.

All formulae mirror the C++ implementations (FTerrainFrictionRegistry,
ComputeAnisotropicFriction, slope instability, altitude fatigue, footprint
depth).  No UE5 import is required; no pipeline imports are used.

Run:
    python -m pytest pipeline/tests/physics/ -v
"""

import math

import pytest

# ---------------------------------------------------------------------------
# Section 1: Terrain Friction Registry
# ---------------------------------------------------------------------------

FRICTION_REGISTRY = {
    "PM_GraniteWet":   {"StaticDry": 0.65, "StaticWet": 0.38, "DynamicDry": 0.55, "DynamicWet": 0.28, "AnisotropyAxis": 0,   "AnisotropyRatio": 1.0},
    "PM_GraniteDry":   {"StaticDry": 0.82, "StaticWet": 0.82, "DynamicDry": 0.72, "DynamicWet": 0.72, "AnisotropyAxis": 0,   "AnisotropyRatio": 1.0},
    "PM_ClayWet":      {"StaticDry": 0.18, "StaticWet": 0.06, "DynamicDry": 0.12, "DynamicWet": 0.04, "AnisotropyAxis": 0,   "AnisotropyRatio": 1.0},
    "PM_ClayDry":      {"StaticDry": 0.55, "StaticWet": 0.55, "DynamicDry": 0.42, "DynamicWet": 0.42, "AnisotropyAxis": 0,   "AnisotropyRatio": 1.0},
    "PM_ShaleWet":     {"StaticDry": 0.45, "StaticWet": 0.22, "DynamicDry": 0.35, "DynamicWet": 0.14, "AnisotropyAxis": 235, "AnisotropyRatio": 3.2},
    "PM_GrasslandDry": {"StaticDry": 0.60, "StaticWet": 0.60, "DynamicDry": 0.48, "DynamicWet": 0.48, "AnisotropyAxis": 0,   "AnisotropyRatio": 1.0},
    "PM_BurnedGrass":  {"StaticDry": 0.12, "StaticWet": 0.08, "DynamicDry": 0.09, "DynamicWet": 0.06, "AnisotropyAxis": 0,   "AnisotropyRatio": 1.0},
    "PM_RiverSand":    {"StaticDry": 0.38, "StaticWet": 0.28, "DynamicDry": 0.30, "DynamicWet": 0.20, "AnisotropyAxis": 0,   "AnisotropyRatio": 1.0},
}

# ---------------------------------------------------------------------------
# Helper formulae (mirrors of C++ implementations)
# ---------------------------------------------------------------------------

def compute_anisotropic_friction(mat: dict, move_bearing_deg: float,
                                  is_wet: bool, is_moving: bool) -> float:
    """
    Mirror of FTerrainFrictionRegistry + ComputeAnisotropicFriction in C++.
    move_bearing_deg: compass bearing of movement direction (0=East, 90=North-ish
                      OR atan2(Y,X) in degrees — use consistent convention)
    """
    # Select base friction
    if is_wet and is_moving:
        base = mat["DynamicWet"]
    elif is_wet:
        base = mat["StaticWet"]
    elif is_moving:
        base = mat["DynamicDry"]
    else:
        base = mat["StaticDry"]

    ratio = mat["AnisotropyRatio"]
    if ratio <= 1.01:
        return base  # isotropic fast path

    axis = mat["AnisotropyAxis"]
    delta = abs(move_bearing_deg - axis) % 360
    if delta > 180:
        delta = 360 - delta
    # Clamp to [0, 90] — symmetrical about axis
    delta = min(delta, 180 - delta)
    cos_f = math.cos(math.radians(delta))
    multiplier = (1.0 / ratio) + (1.0 - 1.0 / ratio) * (cos_f ** 2)
    return base * multiplier


def slope_instability_margin(slope_angle_deg: float, effective_friction: float) -> float:
    """Required friction = sin(slope_angle). Margin > 0 = slip risk."""
    required = math.sin(math.radians(slope_angle_deg))
    return required - effective_friction


def altitude_fatigue_multiplier(altitude_m: float,
                                 baseline_m: float = 800,
                                 scale_m: float = 1000) -> float:
    """1.0 at sea level; +40% at 1800m (1000m above 800m baseline)."""
    return 1.0 + max(0.0, (altitude_m - baseline_m) / scale_m) * 0.40


def footprint_depth_m(mass_kg: float, area_m2: float, hardness_pa: float) -> float:
    return mass_kg / (area_m2 * hardness_pa)


# ===========================================================================
# Registry validation — 8 tests
# ===========================================================================

def test_registry_pm_granite_dry():
    mat = FRICTION_REGISTRY["PM_GraniteDry"]
    assert mat["StaticDry"] == pytest.approx(0.82)
    assert mat["StaticWet"] == pytest.approx(0.82)
    assert mat["AnisotropyRatio"] == pytest.approx(1.0)


def test_registry_pm_clay_wet():
    """PM_ClayWet has the lowest StaticWet of all materials (0.06)."""
    mat = FRICTION_REGISTRY["PM_ClayWet"]
    assert mat["StaticWet"] == pytest.approx(0.06)
    all_static_wet = [m["StaticWet"] for m in FRICTION_REGISTRY.values()]
    assert mat["StaticWet"] == min(all_static_wet)


def test_registry_pm_shale_wet():
    mat = FRICTION_REGISTRY["PM_ShaleWet"]
    assert mat["AnisotropyAxis"] == 235
    assert mat["AnisotropyRatio"] == pytest.approx(3.2)


def test_registry_burned_grass_lowest_friction():
    """PM_BurnedGrass StaticDry=0.12 is the lowest of all dry static values."""
    mat = FRICTION_REGISTRY["PM_BurnedGrass"]
    assert mat["StaticDry"] == pytest.approx(0.12)
    all_static_dry = [m["StaticDry"] for m in FRICTION_REGISTRY.values()]
    assert mat["StaticDry"] == min(all_static_dry)


def test_registry_all_static_dry_positive():
    for name, mat in FRICTION_REGISTRY.items():
        assert mat["StaticDry"] > 0, f"{name} StaticDry must be > 0"


def test_registry_all_wet_leq_dry():
    for name, mat in FRICTION_REGISTRY.items():
        assert mat["StaticWet"] <= mat["StaticDry"], (
            f"{name}: StaticWet ({mat['StaticWet']}) > StaticDry ({mat['StaticDry']})"
        )
        assert mat["DynamicWet"] <= mat["DynamicDry"], (
            f"{name}: DynamicWet ({mat['DynamicWet']}) > DynamicDry ({mat['DynamicDry']})"
        )


def test_registry_all_dynamic_leq_static():
    for name, mat in FRICTION_REGISTRY.items():
        assert mat["DynamicDry"] <= mat["StaticDry"], (
            f"{name}: DynamicDry ({mat['DynamicDry']}) > StaticDry ({mat['StaticDry']})"
        )
        assert mat["DynamicWet"] <= mat["StaticWet"], (
            f"{name}: DynamicWet ({mat['DynamicWet']}) > StaticWet ({mat['StaticWet']})"
        )


def test_registry_shale_ratio_3_2():
    assert FRICTION_REGISTRY["PM_ShaleWet"]["AnisotropyRatio"] == pytest.approx(3.2)


# ===========================================================================
# Anisotropic friction tests — 6 tests
# ===========================================================================

def test_isotropic_material_ignores_bearing():
    """PM_GraniteDry (ratio=1.0) returns the same friction at any bearing."""
    mat = FRICTION_REGISTRY["PM_GraniteDry"]
    bearings = [0, 45, 90, 135, 180, 225, 270, 315]
    results = [compute_anisotropic_friction(mat, b, is_wet=False, is_moving=False)
               for b in bearings]
    assert all(r == pytest.approx(results[0]) for r in results), (
        f"Isotropic material returned different friction values: {results}"
    )


def test_shale_at_axis_gives_max_friction():
    """
    At bearing == axis (235°), cos(delta=0) = 1, so multiplier = 1.0.
    Result = DynamicWet base = 0.14.
    """
    mat = FRICTION_REGISTRY["PM_ShaleWet"]
    f = compute_anisotropic_friction(mat, move_bearing_deg=235.0,
                                     is_wet=True, is_moving=True)
    assert f == pytest.approx(0.14, rel=1e-6)


def test_shale_perpendicular_to_axis_gives_min_friction():
    """
    At bearing = 235 + 90 = 325°, delta = 90°, cos(90°) = 0.
    multiplier = 1/3.2 = 0.3125
    result = 0.14 * 0.3125 = 0.04375
    """
    mat = FRICTION_REGISTRY["PM_ShaleWet"]
    f = compute_anisotropic_friction(mat, move_bearing_deg=325.0,
                                     is_wet=True, is_moving=True)
    assert f == pytest.approx(0.04375, rel=1e-5)


def test_shale_at_axis_vs_perpendicular_ratio():
    """Friction ratio along axis vs. perpendicular should equal AnisotropyRatio (3.2)."""
    mat = FRICTION_REGISTRY["PM_ShaleWet"]
    f_axis = compute_anisotropic_friction(mat, move_bearing_deg=235.0,
                                          is_wet=True, is_moving=True)
    f_perp = compute_anisotropic_friction(mat, move_bearing_deg=325.0,
                                          is_wet=True, is_moving=True)
    ratio = f_axis / f_perp
    assert ratio == pytest.approx(mat["AnisotropyRatio"], rel=1e-5)


def test_wet_friction_less_than_dry():
    """For PM_GraniteWet, wet static friction < dry static friction."""
    mat = FRICTION_REGISTRY["PM_GraniteWet"]
    f_wet = compute_anisotropic_friction(mat, move_bearing_deg=0.0,
                                         is_wet=True, is_moving=False)
    f_dry = compute_anisotropic_friction(mat, move_bearing_deg=0.0,
                                         is_wet=False, is_moving=False)
    assert f_wet < f_dry


def test_moving_friction_less_than_static():
    """For PM_ClayWet, dynamic friction < static friction (same wetness)."""
    mat = FRICTION_REGISTRY["PM_ClayWet"]
    f_static = compute_anisotropic_friction(mat, move_bearing_deg=0.0,
                                            is_wet=True, is_moving=False)
    f_dynamic = compute_anisotropic_friction(mat, move_bearing_deg=0.0,
                                             is_wet=True, is_moving=True)
    assert f_dynamic < f_static


# ===========================================================================
# Slope instability tests — 5 tests
# ===========================================================================

def test_wet_clay_35deg_slope_slips():
    """
    PM_ClayWet static wet = 0.06.  sin(35°) ≈ 0.574.
    margin = 0.574 - 0.06 = 0.514 > 0  →  slip.
    """
    margin = slope_instability_margin(35.0, FRICTION_REGISTRY["PM_ClayWet"]["StaticWet"])
    assert margin > 0, f"Expected slip (margin > 0), got {margin}"


def test_dry_granite_35deg_slope_stable():
    """
    PM_GraniteDry static dry = 0.82.  sin(35°) ≈ 0.574.
    margin = 0.574 - 0.82 < 0  →  no slip.
    """
    margin = slope_instability_margin(35.0, FRICTION_REGISTRY["PM_GraniteDry"]["StaticDry"])
    assert margin < 0, f"Expected stable (margin < 0), got {margin}"


def test_flat_terrain_never_slips():
    """On a 0° slope sin(0°) = 0; margin = 0 - friction <= 0 for any material."""
    for name, mat in FRICTION_REGISTRY.items():
        margin = slope_instability_margin(0.0, mat["StaticDry"])
        assert margin <= 0, f"{name} on flat terrain should never slip, got margin={margin}"


def test_slip_threshold_0_15():
    """
    C++ threshold: only trigger slip when margin > 0.15.
    PM_ClayWet at 35° exceeds 0.15.
    PM_BurnedGrass at 10° does NOT exceed 0.15.
    """
    # PM_ClayWet at 35° — should exceed threshold
    margin_clay_35 = slope_instability_margin(35.0, FRICTION_REGISTRY["PM_ClayWet"]["StaticWet"])
    assert margin_clay_35 > 0.15, (
        f"PM_ClayWet at 35° should exceed 0.15 threshold, got {margin_clay_35}"
    )

    # PM_BurnedGrass at 10° — sin(10°) ≈ 0.1736, StaticDry=0.12, margin ≈ 0.054
    margin_grass_10 = slope_instability_margin(10.0, FRICTION_REGISTRY["PM_BurnedGrass"]["StaticDry"])
    assert margin_grass_10 <= 0.15, (
        f"PM_BurnedGrass at 10° should NOT exceed 0.15 threshold, got {margin_grass_10}"
    )


def test_required_friction_formula():
    """Spot-check sin values used by slope_instability_margin."""
    assert math.sin(math.radians(35)) == pytest.approx(0.5736, abs=1e-4)
    assert math.sin(math.radians(20)) == pytest.approx(0.3420, abs=1e-4)


# ===========================================================================
# Altitude fatigue tests — 5 tests
# ===========================================================================

def test_sea_level_multiplier_is_one():
    assert altitude_fatigue_multiplier(0) == pytest.approx(1.0)


def test_baseline_altitude_no_penalty():
    """At exactly the 800m baseline there is zero additional fatigue."""
    assert altitude_fatigue_multiplier(800) == pytest.approx(1.0)


def test_highveld_40pct_penalty():
    """1800m is 1000m above the 800m baseline → multiplier = 1.0 + 0.40 = 1.40."""
    assert altitude_fatigue_multiplier(1800) == pytest.approx(1.40)


def test_lowveld_no_penalty():
    """400m is below the 800m baseline → no penalty → multiplier = 1.0."""
    assert altitude_fatigue_multiplier(400) == pytest.approx(1.0)


def test_extreme_altitude_scales():
    """2800m is 2000m above baseline → multiplier = 1.0 + (2000/1000)*0.40 = 1.80."""
    assert altitude_fatigue_multiplier(2800) == pytest.approx(1.80)


# ===========================================================================
# Footprint depth tests — 6 tests
# ===========================================================================

def test_wet_clay_depth_realistic():
    """75 kg, 0.025 m², hardness 50 000 Pa → depth = 75/(0.025*50000) = 0.06 m."""
    depth = footprint_depth_m(mass_kg=75, area_m2=0.025, hardness_pa=50_000)
    assert depth == pytest.approx(0.06)


def test_granite_depth_negligible():
    """75 kg, 0.025 m², hardness 5 000 000 Pa → depth = 0.0006 m (0.06 cm)."""
    depth = footprint_depth_m(mass_kg=75, area_m2=0.025, hardness_pa=5_000_000)
    assert depth == pytest.approx(0.0006)


def test_depth_proportional_to_mass():
    """Heavier character leaves deeper print on the same surface."""
    depth_light = footprint_depth_m(mass_kg=60,  area_m2=0.025, hardness_pa=100_000)
    depth_heavy = footprint_depth_m(mass_kg=100, area_m2=0.025, hardness_pa=100_000)
    assert depth_heavy > depth_light


def test_depth_inversely_proportional_to_hardness():
    """Harder surface → shallower print (same mass and area)."""
    depth_soft = footprint_depth_m(mass_kg=75, area_m2=0.025, hardness_pa=50_000)
    depth_hard = footprint_depth_m(mass_kg=75, area_m2=0.025, hardness_pa=5_000_000)
    assert depth_hard < depth_soft


def test_river_sand_intermediate_depth():
    """75 kg, 0.025 m², hardness 100 000 Pa → depth = 0.03 m."""
    depth = footprint_depth_m(mass_kg=75, area_m2=0.025, hardness_pa=100_000)
    assert depth == pytest.approx(0.03)


def test_dry_grass_depth():
    """75 kg, 0.025 m², hardness 200 000 Pa → depth = 75/(0.025*200000) = 0.015 m."""
    depth = footprint_depth_m(mass_kg=75, area_m2=0.025, hardness_pa=200_000)
    assert depth == pytest.approx(0.015)
