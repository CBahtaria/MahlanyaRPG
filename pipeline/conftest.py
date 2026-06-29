"""
Shared pytest fixtures and configuration for the Mahlanya pipeline test suite.
"""

import pathlib
import pytest


PIPELINE_ROOT = pathlib.Path(__file__).parent
PROJECT_ROOT = PIPELINE_ROOT.parent


@pytest.fixture(scope="session")
def pipeline_root() -> pathlib.Path:
    return PIPELINE_ROOT


@pytest.fixture(scope="session")
def project_root() -> pathlib.Path:
    return PROJECT_ROOT


@pytest.fixture(scope="session")
def history_data_dir() -> pathlib.Path:
    return PIPELINE_ROOT / "history" / "data"


@pytest.fixture(scope="session")
def sample_knowledge_graph(tmp_path_factory, history_data_dir):
    """Creates a minimal knowledge graph ndjson for validator tests."""
    kg = tmp_path_factory.mktemp("kg") / "knowledge_graph.ndjson"
    kg.write_text(
        '{"id":"SW001","name":"Sobhuza I","born":1795,"died":1839}\n'
        '{"id":"GUN001","name":"flintlock_musket","introduced_to_region":1840}\n',
        encoding="utf-8",
    )
    return kg


@pytest.fixture(scope="session")
def sample_scene_valid() -> dict:
    return {
        "era_year": 1820,
        "entities": ["SW001"],
        "equipped_items": [],
    }


@pytest.fixture(scope="session")
def sample_scene_chrono_violation() -> dict:
    return {
        "era_year": 1780,
        "entities": ["SW001"],  # SW001 born 1795 — anachronism
        "equipped_items": [],
    }


@pytest.fixture(scope="session")
def sample_scene_item_violation() -> dict:
    return {
        "era_year": 1820,
        "entities": [],
        "equipped_items": ["GUN001"],  # GUN001 introduced 1840 — anachronism
    }
