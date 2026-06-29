"""Swazi historical knowledge graph builder."""
import json
import pathlib

_DATA_FILES = [
    "persons.json",
    "places.json",
    "battles.json",
    "events.json",
    "material_culture.json",
]


def _load_entities(data_dir: pathlib.Path) -> dict:
    entities: dict = {}
    for filename in _DATA_FILES:
        path = data_dir / filename
        if not path.exists():
            continue
        items = json.loads(path.read_text(encoding="utf-8"))
        for item in items:
            entity_id = item.get("id")
            if entity_id:
                entities[entity_id] = item
    return entities


def _load_relations(data_dir: pathlib.Path) -> list:
    path = data_dir / "relations.json"
    if not path.exists():
        return []
    return json.loads(path.read_text(encoding="utf-8"))


def build_knowledge_graph(
    data_dir: pathlib.Path, output_path: pathlib.Path
) -> pathlib.Path:
    """
    Build the Swazi historical knowledge graph and write it as NDJSON.

    Each line is one JSON object: either an entity (keyed by 'id') or a
    relation record (with 'from', 'rel', 'to' keys).

    Args:
        data_dir:     Directory containing persons.json, places.json, etc.
        output_path:  Destination .ndjson file.

    Returns:
        output_path after writing.
    """
    entities = _load_entities(data_dir)
    relations = _load_relations(data_dir)

    # Validate that all relation endpoints exist
    missing: list[str] = []
    for rel in relations:
        for endpoint in (rel.get("from", ""), rel.get("to", "")):
            if endpoint and endpoint not in entities:
                missing.append(endpoint)

    output_path.parent.mkdir(parents=True, exist_ok=True)

    lines: list[str] = []
    for entity in entities.values():
        lines.append(json.dumps(entity, ensure_ascii=False))
    for rel in relations:
        lines.append(json.dumps(rel, ensure_ascii=False))

    output_path.write_text("\n".join(lines) + "\n", encoding="utf-8")
    return output_path


def query_entity(graph_path: pathlib.Path, entity_id: str) -> dict | None:
    """Load graph and return the entity dict for entity_id, or None."""
    if not graph_path.exists():
        return None
    for line in graph_path.read_text(encoding="utf-8").splitlines():
        line = line.strip()
        if not line:
            continue
        obj = json.loads(line)
        if obj.get("id") == entity_id:
            return obj
    return None


def query_relations(
    graph_path: pathlib.Path, entity_id: str, rel_type: str | None = None
) -> list[dict]:
    """Return all relation records involving entity_id (as from or to)."""
    if not graph_path.exists():
        return []
    results: list[dict] = []
    for line in graph_path.read_text(encoding="utf-8").splitlines():
        line = line.strip()
        if not line:
            continue
        obj = json.loads(line)
        if "from" not in obj:
            continue
        if obj.get("from") == entity_id or obj.get("to") == entity_id:
            if rel_type is None or obj.get("rel") == rel_type:
                results.append(obj)
    return results


def existed_during(entity: dict, year: int) -> bool:
    """
    Return True if the entity existed during the given year.
    Uses born/died for persons and era_active/period/year for others.
    """
    entity_type = entity.get("entity_type", "")

    if entity_type == "Person":
        born = entity.get("born", 0)
        died = entity.get("died", 9999)
        return born <= year <= died

    if entity_type == "Place":
        era = entity.get("era_active")
        if era and len(era) == 2:
            return era[0] <= year <= era[1]
        return True

    if entity_type in ("Event", "Battle"):
        period = entity.get("period")
        if period and len(period) == 2:
            return period[0] <= year <= period[1]
        ev_year = entity.get("year", 0)
        return ev_year <= year

    return True


def get_period_accurate_title(entity: dict, year: int) -> str:
    """Return the person's title valid for the given year."""
    title = entity.get("title", "")
    if entity.get("entity_type") == "Person":
        born = entity.get("born", 0)
        died = entity.get("died", 9999)
        if year < born or year > died:
            return f"{title} (anachronistic in {year})"
    return title
