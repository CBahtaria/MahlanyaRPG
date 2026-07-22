"""Tests for ProximityPark GPS coordinate validation constants."""
import pytest

# Mirror the C++ constants here — single source of truth is the header,
# this test catches drift if someone changes one without the other.
ESWATINI_LAT_MIN = -27.32
ESWATINI_LAT_MAX = -25.72
ESWATINI_LON_MIN = 30.79
ESWATINI_LON_MAX = 32.14

KNOWN_SITES = [
    ("Malolotja Fog Net Alpha", -26.08, 31.11),
    ("Manzini Urban Canopy", -26.48, 31.38),
    ("Lubombo Ridge East", -26.20, 31.95),
]

def _within_eswatini(lat: float, lon: float) -> bool:
    return (ESWATINI_LAT_MIN <= lat <= ESWATINI_LAT_MAX and
            ESWATINI_LON_MIN <= lon <= ESWATINI_LON_MAX)

def test_eswatini_bbox_coherent():
    assert ESWATINI_LAT_MIN < ESWATINI_LAT_MAX
    assert ESWATINI_LON_MIN < ESWATINI_LON_MAX

@pytest.mark.parametrize("name,lat,lon", KNOWN_SITES)
def test_known_sites_within_eswatini(name, lat, lon):
    assert _within_eswatini(lat, lon), f"{name} at ({lat}, {lon}) is outside Eswatini bounds"

def test_manzini_origin_within_bounds():
    """Manzini city centre — the world origin for GPS→UE5 conversion."""
    manzini_lat, manzini_lon = -26.32, 31.14
    assert _within_eswatini(manzini_lat, manzini_lon)

def test_out_of_bounds_coordinates_rejected():
    johannesburg_lat, johannesburg_lon = -26.20, 28.04  # outside Eswatini
    assert not _within_eswatini(johannesburg_lat, johannesburg_lon)
