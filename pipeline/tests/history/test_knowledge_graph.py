"""Tests for pipeline/history/build_knowledge_graph.py"""
import json
import pathlib
import pytest

from pipeline.history.build_knowledge_graph import (
    build_knowledge_graph,
    existed_during,
    get_period_accurate_title,
    query_entity,
    query_relations,
)


DATA_DIR = pathlib.Path(__file__).parents[3] / "pipeline" / "history" / "data"


@pytest.fixture
def graph_path(tmp_path):
    return build_knowledge_graph(DATA_DIR, tmp_path / "knowledge_graph.ndjson")


class TestBuildKnowledgeGraph:
    def test_returns_path(self, tmp_path):
        out = build_knowledge_graph(DATA_DIR, tmp_path / "kg.ndjson")
        assert out == tmp_path / "kg.ndjson"

    def test_output_file_exists(self, graph_path):
        assert graph_path.exists()

    def test_output_is_valid_ndjson(self, graph_path):
        for line in graph_path.read_text().splitlines():
            if line.strip():
                json.loads(line)  # must not raise

    def test_entities_have_ids(self, graph_path):
        entities = [
            json.loads(l) for l in graph_path.read_text().splitlines()
            if l.strip() and "from" not in json.loads(l)
        ]
        assert len(entities) > 0
        for e in entities:
            assert "id" in e

    def test_persons_present(self, graph_path):
        lines = [json.loads(l) for l in graph_path.read_text().splitlines() if l.strip()]
        person_ids = {o["id"] for o in lines if o.get("entity_type") == "Person"}
        assert "NB001" in person_ids
        assert "SW001" in person_ids
        assert "ML001" in person_ids

    def test_places_present(self, graph_path):
        lines = [json.loads(l) for l in graph_path.read_text().splitlines() if l.strip()]
        place_ids = {o["id"] for o in lines if o.get("entity_type") == "Place"}
        assert "PL_Lobamba" in place_ids
        assert "PL_Lubombo" in place_ids

    def test_battles_present(self, graph_path):
        lines = [json.loads(l) for l in graph_path.read_text().splitlines() if l.strip()]
        battle_ids = {o["id"] for o in lines if o.get("entity_type") == "Battle"}
        assert "BT001" in battle_ids

    def test_events_present(self, graph_path):
        lines = [json.loads(l) for l in graph_path.read_text().splitlines() if l.strip()]
        event_ids = {o["id"] for o in lines if o.get("entity_type") == "Event"}
        assert "EV001" in event_ids
        assert "EV002" in event_ids

    def test_items_present(self, graph_path):
        lines = [json.loads(l) for l in graph_path.read_text().splitlines() if l.strip()]
        item_ids = {o["id"] for o in lines if o.get("entity_type") == "Item"}
        assert "ITEM_Umshiza" in item_ids
        assert "ITEM_Firearm_Flintlock" in item_ids

    def test_relations_written(self, graph_path):
        lines = [json.loads(l) for l in graph_path.read_text().splitlines() if l.strip()]
        rels = [o for o in lines if "from" in o and "rel" in o and "to" in o]
        assert len(rels) >= 5

    def test_succession_chain_written(self, graph_path):
        lines = [json.loads(l) for l in graph_path.read_text().splitlines() if l.strip()]
        rels = [o for o in lines if o.get("rel") == "succeeded"]
        froms = {r["from"] for r in rels}
        assert "SW001" in froms
        assert "MS001" in froms

    def test_creates_parent_dir(self, tmp_path):
        nested = tmp_path / "nested" / "deep" / "kg.ndjson"
        build_knowledge_graph(DATA_DIR, nested)
        assert nested.exists()

    def test_overwrite_existing(self, tmp_path):
        out = tmp_path / "kg.ndjson"
        out.write_text("old content\n")
        build_knowledge_graph(DATA_DIR, out)
        data = out.read_text()
        assert "old content" not in data
        assert "NB001" in data


class TestQueryEntity:
    def test_finds_known_entity(self, graph_path):
        entity = query_entity(graph_path, "NB001")
        assert entity is not None
        assert entity["name"] == "Ngwane III"

    def test_returns_none_for_unknown(self, graph_path):
        assert query_entity(graph_path, "DOES_NOT_EXIST") is None

    def test_returns_none_when_graph_missing(self, tmp_path):
        assert query_entity(tmp_path / "nope.ndjson", "NB001") is None

    def test_finds_place(self, graph_path):
        entity = query_entity(graph_path, "PL_Lobamba")
        assert entity is not None
        assert entity["place_type"] == "Royal Kraal"

    def test_finds_item(self, graph_path):
        entity = query_entity(graph_path, "ITEM_Umshiza")
        assert entity is not None
        assert entity["introduced_to_region"] == 1600


class TestQueryRelations:
    def test_finds_relations_for_entity(self, graph_path):
        rels = query_relations(graph_path, "SW001")
        assert len(rels) > 0

    def test_filters_by_rel_type(self, graph_path):
        rels = query_relations(graph_path, "SW001", rel_type="succeeded")
        assert all(r["rel"] == "succeeded" for r in rels)

    def test_finds_both_from_and_to(self, graph_path):
        rels = query_relations(graph_path, "NB001")
        has_from = any(r.get("from") == "NB001" for r in rels)
        has_to = any(r.get("to") == "NB001" for r in rels)
        assert has_from or has_to

    def test_returns_empty_for_unknown(self, graph_path):
        assert query_relations(graph_path, "GHOST_ENTITY") == []

    def test_returns_empty_when_graph_missing(self, tmp_path):
        assert query_relations(tmp_path / "nope.ndjson", "SW001") == []


class TestExistedDuring:
    def test_person_alive_during_reign(self):
        entity = {"entity_type": "Person", "born": 1795, "died": 1839}
        assert existed_during(entity, 1820) is True

    def test_person_not_yet_born(self):
        entity = {"entity_type": "Person", "born": 1795, "died": 1839}
        assert existed_during(entity, 1790) is False

    def test_person_already_dead(self):
        entity = {"entity_type": "Person", "born": 1795, "died": 1839}
        assert existed_during(entity, 1850) is False

    def test_place_inside_era_active(self):
        entity = {"entity_type": "Place", "era_active": [1820, 1906]}
        assert existed_during(entity, 1870) is True

    def test_place_before_era_active(self):
        entity = {"entity_type": "Place", "era_active": [1820, 1906]}
        assert existed_during(entity, 1800) is False

    def test_event_during_period(self):
        entity = {"entity_type": "Event", "period": [1815, 1840]}
        assert existed_during(entity, 1825) is True

    def test_event_before_period(self):
        entity = {"entity_type": "Event", "period": [1815, 1840]}
        assert existed_during(entity, 1810) is False

    def test_unknown_entity_type_always_true(self):
        entity = {"entity_type": "Unknown"}
        assert existed_during(entity, 1800) is True

    def test_person_born_year_inclusive(self):
        entity = {"entity_type": "Person", "born": 1795, "died": 1839}
        assert existed_during(entity, 1795) is True

    def test_person_death_year_inclusive(self):
        entity = {"entity_type": "Person", "born": 1795, "died": 1839}
        assert existed_during(entity, 1839) is True


class TestGetPeriodAccurateTitle:
    def test_returns_title_during_lifetime(self):
        entity = {"entity_type": "Person", "title": "King", "born": 1795, "died": 1839}
        assert get_period_accurate_title(entity, 1820) == "King"

    def test_flags_anachronistic_title(self):
        entity = {"entity_type": "Person", "title": "King", "born": 1795, "died": 1839}
        result = get_period_accurate_title(entity, 1850)
        assert "anachronistic" in result

    def test_non_person_returns_title(self):
        entity = {"entity_type": "Place", "title": "Royal Kraal"}
        assert get_period_accurate_title(entity, 1900) == "Royal Kraal"

    def test_entity_without_title_returns_empty(self):
        entity = {"entity_type": "Person", "born": 1795, "died": 1839}
        assert get_period_accurate_title(entity, 1820) == ""
