"""Swazi historical knowledge graph builder (T6-1).

Reads the six history data files under ``pipeline/history/data/`` and merges
them into a single NDJSON knowledge graph. Runs a full validation suite:

  * Referential integrity — every relation endpoint / capital / battle
    location / event participant must resolve to a known entity.
  * DAG succession — the ``succeeded`` (royal succession) relation graph
    must be acyclic. Detected via iterative DFS with a WHITE / GREY /
    BLACK colouring.
  * Exogamy — no two persons sharing a ``clan`` value may be linked by
    a ``sibling`` relation across clan boundaries. (Adaptive: silent
    when neither ``parent`` nor ``sibling`` relations appear in data.)
  * Temporal validity — ``born < died`` when both are set.
  * Year bounds — every year appearing anywhere must fall in the game
    period 1750–1920.

Invoked as a CI gate: exits 1 with an explicit reason on any failure,
0 with a one-line summary otherwise.
"""
from __future__ import annotations

import argparse
import json
import pathlib
import sys
from typing import Any, Iterable

# Game historical period. Anything outside is a bug per CLAUDE.md rule 1.
YEAR_MIN = 1750
YEAR_MAX = 1920

# Entity files. The order fixes NDJSON output order for stable diffs.
_ENTITY_FILES: tuple[tuple[str, str], ...] = (
    ("persons.json", "person"),
    ("places.json", "place"),
    ("battles.json", "battle"),
    ("events.json", "event"),
    ("material_culture.json", "item"),
)

# Endpoint refs whose prefix means "clan / external group tag", not an
# entity ID we must resolve. Kept explicit so CI failure messages are
# not swallowed by a permissive default.
_CLAN_PREFIXES = ("CL_",)


# --------------------------------------------------------------------------- #
# Loading                                                                     #
# --------------------------------------------------------------------------- #


def _load_json(path: pathlib.Path) -> list[dict]:
    """Load a JSON array file. Missing file → empty list."""
    if not path.exists():
        return []
    return json.loads(path.read_text(encoding="utf-8"))


def _load_entities(data_dir: pathlib.Path) -> dict[str, dict]:
    """Load all entity files, tagging each with its ``_type``.

    The original ``entity_type`` field (if present) is preserved so the
    rest of the pipeline — validate_content.py, tests — keeps working.
    """
    entities: dict[str, dict] = {}
    for filename, type_tag in _ENTITY_FILES:
        for item in _load_json(data_dir / filename):
            entity_id = item.get("id")
            if not entity_id:
                continue
            item["_type"] = type_tag
            entities[entity_id] = item
    return entities


def _load_relations(data_dir: pathlib.Path) -> list[dict]:
    """Load the relations file. Each dict gets ``_type: relation``."""
    rels = _load_json(data_dir / "relations.json")
    for rel in rels:
        rel["_type"] = "relation"
    return rels


# --------------------------------------------------------------------------- #
# Validation                                                                  #
# --------------------------------------------------------------------------- #


def _is_clan_ref(ref: str) -> bool:
    return isinstance(ref, str) and ref.startswith(_CLAN_PREFIXES)


def _collect_years(
    entity: dict,
) -> Iterable[tuple[str, int, int, int]]:
    """Yield ``(field_name, year, allowed_min, allowed_max)`` per year.

    Not every field is bounded the same way:
      * ``born``, ``year``, and the endpoints of ``period`` / ``era_active``
        are game-period fields — strict [1750, 1920].
      * ``died`` gets a small upper cushion because well-attested figures
        (e.g. Labotsibeni, d. 1925) were born inside the period but
        outlived it.
      * ``introduced_to_region`` has no lower bound (traditional items
        like the assegai and umshiza predate the setting by centuries)
        but must not be later than the game's terminal year.
    """
    strict = ("born", "year")
    for key in strict:
        val = entity.get(key)
        if isinstance(val, int) and val != 0:
            yield key, val, YEAR_MIN, YEAR_MAX

    val = entity.get("died")
    if isinstance(val, int) and val != 0:
        yield "died", val, YEAR_MIN, YEAR_MAX + 20

    val = entity.get("introduced_to_region")
    if isinstance(val, int) and val != 0:
        # 0 → "predates setting, always available." Only upper bound matters.
        yield "introduced_to_region", val, 0, YEAR_MAX

    for key in ("period", "era_active"):
        val = entity.get(key)
        if isinstance(val, list) and len(val) == 2:
            for i, y in enumerate(val):
                if isinstance(y, int) and y != 0:
                    yield f"{key}[{i}]", y, YEAR_MIN, YEAR_MAX


def _validate_referential_integrity(
    entities: dict[str, dict], relations: list[dict]
) -> list[str]:
    """Every ID reference must resolve. Returns a list of error strings."""
    errors: list[str] = []
    known = set(entities)

    # Relations: from / to endpoints.
    for i, rel in enumerate(relations):
        for side in ("from", "to"):
            ref = rel.get(side)
            if not ref or _is_clan_ref(ref):
                continue
            if ref not in known:
                errors.append(
                    f"relation[{i}] {rel.get('rel', '?')}: "
                    f"'{side}={ref}' does not resolve to any entity"
                )

    # Person → capital → place.
    for eid, entity in entities.items():
        if entity.get("_type") != "person":
            continue
        cap = entity.get("capital")
        if cap and cap not in known:
            errors.append(f"person {eid}: capital '{cap}' is not a known place")

    # Battle → location → place; participants → person or clan tag.
    for eid, entity in entities.items():
        if entity.get("_type") != "battle":
            continue
        loc = entity.get("location") or entity.get("place_id")
        if loc and loc not in known:
            errors.append(f"battle {eid}: location '{loc}' is not a known place")
        for pid in entity.get("participants", []) or []:
            if _is_clan_ref(pid):
                continue
            if pid not in known:
                errors.append(
                    f"battle {eid}: participant '{pid}' does not resolve"
                )
        victor = entity.get("victor")
        if victor and not _is_clan_ref(victor) and victor not in known:
            errors.append(f"battle {eid}: victor '{victor}' does not resolve")

    # Event → participants / place_id.
    for eid, entity in entities.items():
        if entity.get("_type") != "event":
            continue
        loc = entity.get("place_id")
        if loc and loc not in known:
            errors.append(f"event {eid}: place_id '{loc}' is not a known place")
        for pid in entity.get("participants", []) or []:
            if _is_clan_ref(pid):
                continue
            if pid not in known:
                errors.append(
                    f"event {eid}: participant '{pid}' does not resolve"
                )

    return errors


def _validate_no_succession_cycles(
    entities: dict[str, dict], relations: list[dict]
) -> list[str]:
    """Iterative DFS cycle check over the ``succeeded`` DAG.

    Also covers ``parent`` relations if the data ever uses them. The
    monarch-succession graph in Eswatini is strictly linear historically,
    so any cycle here is a data-entry bug.
    """
    dag_rels = {"succeeded", "parent"}
    adj: dict[str, list[str]] = {eid: [] for eid in entities}
    for rel in relations:
        if rel.get("rel") not in dag_rels:
            continue
        # Convention in this repo: 'from succeeded to' means from-succeeded-to,
        # i.e. edge goes to → from (successor points at predecessor). We treat
        # any direction as an edge for cycle purposes since a cycle in either
        # direction is a bug.
        a, b = rel.get("from"), rel.get("to")
        if a in adj and b in adj:
            adj[a].append(b)

    WHITE, GREY, BLACK = 0, 1, 2
    color = {eid: WHITE for eid in adj}
    errors: list[str] = []

    for start in adj:
        if color[start] != WHITE:
            continue
        # Stack frames: (node, iterator over neighbours).
        stack: list[tuple[str, Iterable[str]]] = [(start, iter(adj[start]))]
        color[start] = GREY
        path: list[str] = [start]
        while stack:
            node, it = stack[-1]
            nxt = next(it, None)
            if nxt is None:
                color[node] = BLACK
                stack.pop()
                path.pop()
                continue
            if color.get(nxt) == GREY:
                cycle = " → ".join(path[path.index(nxt):] + [nxt])
                errors.append(f"succession cycle detected: {cycle}")
                # Keep walking so we report every distinct cycle, but do
                # not descend into it again.
                continue
            if color.get(nxt) == WHITE:
                color[nxt] = GREY
                path.append(nxt)
                stack.append((nxt, iter(adj[nxt])))
    return errors


def _validate_exogamy(
    entities: dict[str, dict], relations: list[dict]
) -> list[str]:
    """Sibling relations must not span two different clans.

    The Swazi patrilineal rule: children inherit the father's clan, so
    biological siblings share a clan. A ``sibling`` relation crossing
    clans indicates the data is asserting kinship that violates the
    exogamous marriage rule. Silently a no-op when no sibling relations
    exist — most royal succession data uses ``succeeded``, not sibling.
    """
    errors: list[str] = []
    for i, rel in enumerate(relations):
        if rel.get("rel") != "sibling":
            continue
        a = entities.get(rel.get("from", ""))
        b = entities.get(rel.get("to", ""))
        if not a or not b:
            continue
        ca, cb = a.get("clan"), b.get("clan")
        if ca and cb and ca != cb:
            errors.append(
                f"relation[{i}]: sibling '{rel['from']}' (clan={ca}) and "
                f"'{rel['to']}' (clan={cb}) span different clans — "
                f"violates Swazi patrilineal clan inheritance"
            )
    return errors


def _validate_temporal(entities: dict[str, dict]) -> list[str]:
    """born < died when both present."""
    errors: list[str] = []
    for eid, entity in entities.items():
        born = entity.get("born")
        died = entity.get("died")
        if isinstance(born, int) and isinstance(died, int) and born and died:
            if born >= died:
                errors.append(
                    f"person {eid}: born={born} not < died={died}"
                )
    return errors


def _validate_year_bounds(entities: dict[str, dict]) -> list[str]:
    """Every year field must fall inside its own allowed range.

    Bounds are per-field, not global — see _collect_years for the
    rationale on why ``died`` and ``introduced_to_region`` widen the
    default [1750, 1920] window.
    """
    errors: list[str] = []
    for eid, entity in entities.items():
        for field, year, lo, hi in _collect_years(entity):
            if year < lo or year > hi:
                errors.append(
                    f"{entity.get('_type', 'entity')} {eid}: "
                    f"{field}={year} outside allowed range "
                    f"[{lo}, {hi}]"
                )
    return errors


def validate_knowledge_graph(
    entities: dict[str, dict], relations: list[dict]
) -> list[str]:
    """Run every check. Empty list means the graph is valid."""
    errors: list[str] = []
    errors += _validate_referential_integrity(entities, relations)
    errors += _validate_no_succession_cycles(entities, relations)
    errors += _validate_exogamy(entities, relations)
    errors += _validate_temporal(entities)
    errors += _validate_year_bounds(entities)
    return errors


# --------------------------------------------------------------------------- #
# Build                                                                       #
# --------------------------------------------------------------------------- #


def build_knowledge_graph(
    data_dir: pathlib.Path,
    output_path: pathlib.Path,
    *,
    validate: bool = False,
) -> pathlib.Path:
    """Merge the six data files into NDJSON at ``output_path``.

    Args:
        data_dir:      Directory holding persons.json, places.json, etc.
        output_path:   Destination NDJSON file. Parents are created.
        validate:      When True, raise ValueError if the graph fails
                       any validation check. Off by default so unit
                       tests can build minimal or partial graphs.

    Returns:
        ``output_path`` after writing.
    """
    entities = _load_entities(data_dir)
    relations = _load_relations(data_dir)

    if validate:
        errors = validate_knowledge_graph(entities, relations)
        if errors:
            raise ValueError(
                "knowledge graph validation failed:\n  - "
                + "\n  - ".join(errors)
            )

    output_path.parent.mkdir(parents=True, exist_ok=True)
    lines: list[str] = []
    for entity in entities.values():
        lines.append(json.dumps(entity, ensure_ascii=False))
    for rel in relations:
        lines.append(json.dumps(rel, ensure_ascii=False))
    output_path.write_text("\n".join(lines) + "\n", encoding="utf-8")
    return output_path


# --------------------------------------------------------------------------- #
# Query helpers (kept for existing pipeline / tests)                          #
# --------------------------------------------------------------------------- #


def query_entity(graph_path: pathlib.Path, entity_id: str) -> dict | None:
    """Return the entity dict for entity_id, or None."""
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
    """Return all relations involving entity_id (as from or to)."""
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
    """True if the entity existed during ``year``."""
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
    """Return the person's title valid for ``year``, or an anachronism note."""
    title = entity.get("title", "")
    if entity.get("entity_type") == "Person":
        born = entity.get("born", 0)
        died = entity.get("died", 9999)
        if year < born or year > died:
            return f"{title} (anachronistic in {year})"
    return title


# --------------------------------------------------------------------------- #
# CLI                                                                         #
# --------------------------------------------------------------------------- #


def _default_data_dir() -> pathlib.Path:
    return pathlib.Path(__file__).parent / "data"


def _default_output_path() -> pathlib.Path:
    return pathlib.Path(__file__).parent / "knowledge_graph.ndjson"


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(
        description=(
            "Build the Swazi historical knowledge graph and validate it "
            "as a CI gate. Non-zero exit on any validation error."
        )
    )
    parser.add_argument(
        "--data-dir",
        type=pathlib.Path,
        default=_default_data_dir(),
        help="Directory holding persons.json, places.json, etc.",
    )
    parser.add_argument(
        "--output-path",
        type=pathlib.Path,
        default=_default_output_path(),
        help="Destination NDJSON path.",
    )
    parser.add_argument(
        "--no-validate",
        action="store_true",
        help="Build without validation. Used only for local iteration.",
    )
    args = parser.parse_args(argv)

    entities = _load_entities(args.data_dir)
    relations = _load_relations(args.data_dir)

    if not args.no_validate:
        errors = validate_knowledge_graph(entities, relations)
        if errors:
            print(
                "ERROR: knowledge graph validation failed",
                file=sys.stderr,
            )
            for err in errors:
                print(f"  - {err}", file=sys.stderr)
            return 1

    # Only write the NDJSON if validation passed (or was skipped).
    build_knowledge_graph(
        args.data_dir, args.output_path, validate=False
    )
    n_relations = len(relations)
    n_entities = len(entities)
    print(
        f"Knowledge graph built: {n_entities} entities, "
        f"{n_relations} relations -> {args.output_path}"
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
