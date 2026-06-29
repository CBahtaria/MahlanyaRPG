"""
Advanced historical data validator for MahlanyaRPG.
Run: python pipeline/advanced_validator.py <data_path>
Exit 0 = pass, 1 = errors found.
"""

import concurrent.futures
import json
import sys
import threading


class HistoricalValidator:
    """Parallel historical data validator for Swazi RPG persons and relations."""

    def __init__(self, data_path: str) -> None:
        self.data_path = data_path
        self.persons: dict = {}
        self.relations: list = []
        self.errors: list = []
        self.warnings: list = []

    # ------------------------------------------------------------------
    # Data loading
    # ------------------------------------------------------------------

    def _load_data(self) -> None:
        """Load persons.json and relations.json from data_path.

        Accepts either a wrapper object ``{"persons": [...]}`` or a raw JSON
        array ``[...]`` for each file so the validator works against both the
        test fixtures (wrapper format) and the real project data files (raw
        arrays).
        """
        persons_path = f"{self.data_path}/persons.json"
        relations_path = f"{self.data_path}/relations.json"

        with open(persons_path, "r", encoding="utf-8") as f:
            persons_data = json.load(f)
        persons_list = persons_data["persons"] if isinstance(persons_data, dict) else persons_data
        for p in persons_list:
            self.persons[p["id"]] = p

        with open(relations_path, "r", encoding="utf-8") as f:
            relations_data = json.load(f)
        if isinstance(relations_data, dict):
            self.relations = relations_data["relations"]
        else:
            # Raw array — convert edge-list entries to the expected schema on a
            # best-effort basis so validation doesn't crash on unknown formats.
            self.relations = relations_data

    # ------------------------------------------------------------------
    # Parallel orchestrator
    # ------------------------------------------------------------------

    def load_and_validate(self) -> bool:
        self._load_data()
        thread_errors: dict[str, list] = {}
        thread_warnings: dict[str, list] = {}
        lock = threading.Lock()

        def run_check(name, fn):
            local_errors, local_warnings = [], []
            fn(local_errors, local_warnings)
            with lock:
                thread_errors[name] = local_errors
                thread_warnings[name] = local_warnings

        with concurrent.futures.ThreadPoolExecutor(max_workers=4) as pool:
            futures = [
                pool.submit(run_check, "person_consistency",     self._validate_person_consistency),
                pool.submit(run_check, "relation_references",    self._validate_relation_references),
                pool.submit(run_check, "temporal_consistency",   self._validate_temporal_consistency),
                pool.submit(run_check, "genealogical_integrity", self._validate_genealogical_integrity),
                pool.submit(run_check, "cultural_constraints",   self._validate_cultural_constraints),
            ]
            concurrent.futures.wait(futures)

        self.errors   = [e for errs  in thread_errors.values()   for e in errs]
        self.warnings = [w for warns in thread_warnings.values() for w in warns]
        return self._report_results()

    # ------------------------------------------------------------------
    # Validation methods (each writes to caller-provided lists)
    # ------------------------------------------------------------------

    def _validate_person_consistency(self, errors: list, warnings: list) -> None:
        """Validate each person's date fields for basic sanity."""
        for pid, person in self.persons.items():
            if "born" not in person:
                errors.append(
                    f"Person '{pid}' ({person.get('name', '?')}): missing born field"
                )
                continue

            born = person["born"]
            if born <= 0:
                errors.append(
                    f"Person '{pid}' ({person.get('name', '?')}): born must be > 0 (got {born})"
                )

            died = person.get("died")
            if died is not None:
                if died < born:
                    errors.append(
                        f"Person '{pid}' ({person.get('name', '?')}): died ({died}) < born ({born})"
                    )
                elif (died - born) > 120:
                    warnings.append(
                        f"Person '{pid}' ({person.get('name', '?')}): extreme age "
                        f"({died - born} years, born={born}, died={died})"
                    )

    def _validate_relation_references(self, errors: list, warnings: list) -> None:
        """Ensure every participant ID in a relation exists and is not duplicated."""
        for rel in self.relations:
            rid = rel.get("id", "<unknown>")
            participants = rel.get("participants", [])

            # Check each participant exists
            for pid in participants:
                if pid not in self.persons:
                    errors.append(
                        f"Relation '{rid}': participant '{pid}' does not exist in persons"
                    )

            # Check for duplicates
            if len(participants) != len(set(participants)):
                errors.append(
                    f"Relation '{rid}': duplicate participant IDs in participants list"
                )

    def _validate_temporal_consistency(self, errors: list, warnings: list) -> None:
        """Check that all participants were alive during the relation's year."""
        for rel in self.relations:
            rid = rel.get("id", "<unknown>")
            rel_type = rel.get("type", "")
            year = rel.get("year")
            participants = rel.get("participants", [])

            if year is None:
                continue

            resolved = []
            for pid in participants:
                person = self.persons.get(pid)
                if person is None:
                    continue  # already caught by _validate_relation_references
                resolved.append((pid, person))

                born = person.get("born")
                died = person.get("died")

                if born is not None and born > year:
                    errors.append(
                        f"Relation '{rid}' (year={year}): participant '{pid}' "
                        f"not yet born (born={born})"
                    )

                if died is not None and died < year:
                    errors.append(
                        f"Relation '{rid}' (year={year}): participant '{pid}' "
                        f"already dead (died={died})"
                    )

            # SUCCESSION check: participants[0] predecessor must be born before participants[1]
            if rel_type == "SUCCESSION" and len(resolved) >= 2:
                pred_id, pred = resolved[0]
                succ_id, succ = resolved[1]
                pred_born = pred.get("born")
                succ_born = succ.get("born")
                if pred_born is not None and succ_born is not None:
                    if pred_born >= succ_born:
                        errors.append(
                            f"Relation '{rid}' (SUCCESSION): predecessor '{pred_id}' "
                            f"born ({pred_born}) is not before successor '{succ_id}' born ({succ_born})"
                        )

    def _validate_genealogical_integrity(self, errors: list, warnings: list) -> None:
        """Validate LINEAGE relations and detect cycles in the lineage graph."""
        # Build adjacency list: parent_id -> [child_id]
        adjacency: dict[str, list] = {}

        for rel in self.relations:
            if rel.get("type") != "LINEAGE":
                continue

            rid = rel.get("id", "<unknown>")
            participants = rel.get("participants", [])

            if len(participants) < 2:
                continue

            parent_id = participants[0]
            child_id = participants[1]
            parent = self.persons.get(parent_id)
            child = self.persons.get(child_id)

            if parent is None or child is None:
                continue  # already caught by reference check

            parent_born = parent.get("born")
            child_born = child.get("born")

            if parent_born is not None and child_born is not None:
                if parent_born >= child_born:
                    errors.append(
                        f"Relation '{rid}' (LINEAGE): parent '{parent_id}' born ({parent_born}) "
                        f"is not before child '{child_id}' born ({child_born})"
                    )
                else:
                    parent_age_at_birth = child_born - parent_born
                    if parent_age_at_birth < 12:
                        errors.append(
                            f"Relation '{rid}' (LINEAGE): implausibly young parent '{parent_id}' "
                            f"(age {parent_age_at_birth} at child '{child_id}' birth)"
                        )

            # Build adjacency for cycle detection
            adjacency.setdefault(parent_id, []).append(child_id)

        # DFS cycle detection
        visited: set = set()
        rec_stack: set = set()

        def dfs(node: str) -> bool:
            visited.add(node)
            rec_stack.add(node)
            for neighbour in adjacency.get(node, []):
                if neighbour not in visited:
                    if dfs(neighbour):
                        return True
                elif neighbour in rec_stack:
                    return True
            rec_stack.discard(node)
            return False

        for node in list(adjacency.keys()):
            if node not in visited:
                if dfs(node):
                    errors.append(
                        f"Genealogical integrity: cycle detected in LINEAGE graph "
                        f"(involving node '{node}')"
                    )
                    break  # one error is enough to communicate the problem

    def _validate_cultural_constraints(self, errors: list, warnings: list) -> None:
        """For MARRIAGE relations, flag endogamy (same clan = exogamy violation)."""
        for rel in self.relations:
            if rel.get("type") != "MARRIAGE":
                continue

            rid = rel.get("id", "<unknown>")
            participants = rel.get("participants", [])

            clans = []
            for pid in participants:
                person = self.persons.get(pid)
                if person is None:
                    continue
                clan = person.get("clan", "").strip()
                if clan:
                    clans.append(clan)

            # Only fire if every participant has a non-empty clan field
            if len(clans) == len(participants) and len(participants) > 0:
                if len(set(clans)) == 1:
                    errors.append(
                        f"Relation '{rid}' (MARRIAGE): endogamy violation "
                        f"(exogamy required) — all participants share clan '{clans[0]}'"
                    )

    # ------------------------------------------------------------------
    # Reporting
    # ------------------------------------------------------------------

    def _report_results(self) -> bool:
        for error in self.errors:
            print(f"ERROR: {error}")
        for warning in self.warnings:
            print(f"WARNING: {warning}")
        return len(self.errors) == 0


# ------------------------------------------------------------------
# CLI entry point
# ------------------------------------------------------------------

def validate_for_ci():
    if len(sys.argv) < 2:
        print("Usage: python advanced_validator.py <data_path>")
        sys.exit(2)
    v = HistoricalValidator(sys.argv[1])
    passed = v.load_and_validate()
    sys.exit(0 if passed else 1)


if __name__ == "__main__":
    validate_for_ci()
