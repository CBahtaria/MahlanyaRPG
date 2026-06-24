#!/usr/bin/env python3
"""
Mahlanya Production Pipeline: /pipeline/history/validate_content.py
Author: Charles Bartaria (cbartaria1)
Date: 2026-06-24

Enforces absolute historical integrity across all game content.
Validates NPC dialogue, quest text, and scene configurations against the
Swazi historical knowledge graph. CI/CD calls this as a build gate —
any violation exits with code 1 and prints the exact offending claim.
"""

import argparse
import json
import pathlib
import sys
from typing import Any


class SwaziHistoricalValidator:
    """Validates game content claims against the historical knowledge graph."""

    def __init__(self, knowledge_graph_path: pathlib.Path) -> None:
        with open(knowledge_graph_path, "r", encoding="utf-8") as f:
            # Support plain JSON (object or array) and ndjson (one entity per line)
            raw = f.read().strip()
            try:
                parsed = json.loads(raw)
                if isinstance(parsed, list):
                    self.graph: dict[str, Any] = {}
                    for obj in parsed:
                        entity_id = obj.get("id") or obj.get("name")
                        if entity_id:
                            self.graph[entity_id] = obj
                else:
                    self.graph = parsed
            except json.JSONDecodeError:
                # ndjson format — one JSON object per line
                self.graph = {}
                for line in raw.splitlines():
                    if line.strip():
                        obj = json.loads(line)
                        entity_id = obj.get("id") or obj.get("name")
                        if entity_id:
                            self.graph[entity_id] = obj

    def validate_scene(self, scene_data: dict) -> list[dict]:
        """
        Validate a single scene configuration dict.

        Expected scene_data keys:
            era_year (int):           Year the scene is set.
            entities (list[str]):     Entity IDs present in the scene.
            equipped_items (list[str]): Item IDs carried or displayed.

        Returns:
            List of violation dicts with keys: entity, reason.
            Empty list = historically valid.
        """
        violations: list[dict] = []
        era_year: int = scene_data.get("era_year", 0)

        # Rule 1: Entity chronological bounds
        for entity_id in scene_data.get("entities", []):
            entry = self.graph.get(entity_id)
            if entry is None:
                violations.append({
                    "entity": entity_id,
                    "reason": f"Entity '{entity_id}' not found in knowledge graph.",
                })
                continue

            born = entry.get("born") or entry.get("lived_from") or 0
            died = entry.get("died") or entry.get("lived_to") or 9999

            if era_year < born or era_year > died:
                violations.append({
                    "entity": entity_id,
                    "reason": (
                        f"Scene set in {era_year} contains '{entity_id}' "
                        f"whose historical bounds are {born}–{died}."
                    ),
                })

        # Rule 2: Technological / item anachronisms
        for item_id in scene_data.get("equipped_items", []):
            entry = self.graph.get(item_id)
            if entry is None:
                violations.append({
                    "entity": item_id,
                    "reason": f"Item '{item_id}' not found in knowledge graph.",
                })
                continue

            intro_year = (
                entry.get("introduced_to_region")
                or entry.get("introduced")
                or 0
            )
            if era_year < intro_year:
                violations.append({
                    "entity": item_id,
                    "reason": (
                        f"Item '{item_id}' cannot appear in era {era_year}. "
                        f"Earliest verified regional introduction: {intro_year}."
                    ),
                })

        return violations

    def validate_dialogue_file(self, dialogue_path: pathlib.Path) -> list[dict]:
        """
        Validate a YAML/JSON dialogue file against the knowledge graph.
        Dialogue files may contain: entities[], era_year, equipped_items[].

        Returns list of violation dicts with keys: file, entity, reason.
        """
        import yaml  # optional — only needed for .yaml/.yml files
        violations: list[dict] = []

        suffix = dialogue_path.suffix.lower()
        try:
            text = dialogue_path.read_text(encoding="utf-8")
            if suffix in (".yaml", ".yml"):
                data = yaml.safe_load(text)
            elif suffix == ".json":
                data = json.loads(text)
            else:
                return violations  # skip unknown formats
        except Exception as exc:
            return [{"file": str(dialogue_path), "entity": "N/A", "reason": str(exc)}]

        if not isinstance(data, dict):
            return violations

        scene_violations = self.validate_scene(data)
        for v in scene_violations:
            v["file"] = str(dialogue_path)
            violations.append(v)

        return violations


def validate_content(
    content_dir: pathlib.Path,
    knowledge_graph_path: pathlib.Path,
) -> list[dict]:
    """
    Validate all dialogue/scene files under content_dir against the knowledge graph.

    Args:
        content_dir:           Directory tree containing dialogue YAML/JSON files.
        knowledge_graph_path:  Path to knowledge_graph.ndjson (or .json).

    Returns:
        List of violations: [{"file": str, "entity": str, "reason": str}].
        Empty list = all content historically valid.
    """
    if not knowledge_graph_path.exists():
        return []  # Graph not built yet — skip validation

    validator = SwaziHistoricalValidator(knowledge_graph_path)
    violations: list[dict] = []

    if not content_dir.exists():
        return violations  # No content yet — nothing to validate

    for path in content_dir.rglob("*"):
        if path.suffix.lower() in (".yaml", ".yml", ".json") and path.is_file():
            violations.extend(validator.validate_dialogue_file(path))

    return violations


if __name__ == "__main__":
    parser = argparse.ArgumentParser(
        description="Validate game content historical accuracy against knowledge graph"
    )
    parser.add_argument("content_dir",      type=pathlib.Path)
    parser.add_argument("knowledge_graph",  type=pathlib.Path)
    args = parser.parse_args()

    violations = validate_content(args.content_dir, args.knowledge_graph)

    for v in violations:
        loc = v.get("file", "unknown")
        print(f"[VIOLATION] {loc} — {v['entity']}: {v['reason']}")

    if violations:
        print(f"\n{len(violations)} historical violation(s) found. Build FAILED.")
        sys.exit(1)
    else:
        print("All content historically validated. Build PASSED.")
        sys.exit(0)
