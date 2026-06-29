"""
Environment smoke-test: verifies that required Python packages are importable.

Libraries are split into:
  REQUIRED  — must be present for ANY pipeline test to run (pure Python + numpy/scipy)
  GEO       — require system GDAL; expected to be absent in minimal CI
  OPTIONAL  — GPU / graph; optional everywhere

CI passes when all REQUIRED imports succeed. GEO / OPTIONAL failures are reported
as warnings, not errors, so the suite stays green on machines without GDAL.
"""

import importlib
import sys

import pytest

# (module_name, pip_name_for_message, min_version_str_or_None)
REQUIRED_LIBS = [
    ("numpy",  "numpy>=1.26",  "1.26"),
    ("scipy",  "scipy>=1.12",  "1.12"),
    ("yaml",   "pyyaml>=6.0",  None),
    ("click",  "click>=8.1",   None),
    ("pytest", "pytest>=8.0",  None),
]

GEO_LIBS = [
    ("rasterio", "rasterio>=1.3"),
    ("osgeo.gdal", "gdal>=3.8 (system package required)"),
    ("shapely",  "shapely>=2.0"),
    ("geopandas","geopandas>=0.14"),
    ("pyproj",   "pyproj>=3.6"),
]

OPTIONAL_LIBS = [
    ("trimesh",  "trimesh>=4.0  (acoustic IR geometry)"),
    ("numba",    "numba>=0.59   (JIT erosion acceleration)"),
    ("neo4j",    "neo4j>=5.0    (knowledge graph I/O)"),
    ("cupy",     "cupy-cuda12x>=13.0 (GPU erosion — NVIDIA only)"),
]


def _version_ok(module, min_ver: str) -> bool:
    ver_str = getattr(module, "__version__", "0.0.0")
    try:
        actual = tuple(int(x) for x in ver_str.split(".")[:2])
        required = tuple(int(x) for x in min_ver.split(".")[:2])
        return actual >= required
    except ValueError:
        return True  # version string unparseable — assume ok


@pytest.mark.parametrize("mod_name,pip_name,min_ver", REQUIRED_LIBS)
def test_required_library(mod_name, pip_name, min_ver):
    try:
        mod = importlib.import_module(mod_name)
    except ImportError:
        pytest.fail(f"Required library not importable: {mod_name}. Install with: pip install {pip_name}")

    if min_ver:
        assert _version_ok(mod, min_ver), (
            f"{mod_name} version {getattr(mod, '__version__', '?')} < required {min_ver}. "
            f"Upgrade with: pip install '{pip_name}'"
        )


@pytest.mark.parametrize("mod_name,pip_name", GEO_LIBS)
def test_geo_library_available(mod_name, pip_name):
    """
    Geo libraries require system GDAL — skip gracefully in minimal CI.
    These MUST pass on the actual terrain pipeline build machine.
    """
    try:
        importlib.import_module(mod_name)
    except ImportError:
        pytest.skip(f"Geo library {mod_name} not installed (requires GDAL). Install: pip install {pip_name}")


@pytest.mark.parametrize("mod_name,pip_name", OPTIONAL_LIBS)
def test_optional_library_available(mod_name, pip_name):
    """GPU and graph libraries are fully optional — skip if absent."""
    try:
        importlib.import_module(mod_name)
    except ImportError:
        pytest.skip(f"Optional library {mod_name} not installed. Install: pip install {pip_name}")


def test_python_version():
    major, minor = sys.version_info[:2]
    assert (major, minor) >= (3, 11), (
        f"Python 3.11+ required. Running {major}.{minor}. "
        "Use pyenv or conda to switch versions."
    )
