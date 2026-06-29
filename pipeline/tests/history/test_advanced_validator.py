"""
Tests for pipeline/advanced_validator.py — Task 10.8
"""

import json
import pytest

from pipeline.advanced_validator import HistoricalValidator


# ---------------------------------------------------------------------------
# Helper
# ---------------------------------------------------------------------------

def make_data(tmp_path, persons, relations):
    """Write persons.json and relations.json to tmp_path."""
    (tmp_path / "persons.json").write_text(json.dumps({"persons": persons}))
    (tmp_path / "relations.json").write_text(json.dumps({"relations": relations}))


# ---------------------------------------------------------------------------
# Minimal valid fixtures
# ---------------------------------------------------------------------------

def _valid_person(pid="P1", name="Alice", born=1800, died=1850, clan="Dlamini"):
    p = {"id": pid, "name": name, "born": born}
    if died is not None:
        p["died"] = died
    if clan is not None:
        p["clan"] = clan
    return p


# ===========================================================================
# 1. Person consistency tests
# ===========================================================================

def test_valid_person_passes(tmp_path):
    """A person with born=1800, died=1850 should produce no errors or warnings."""
    persons = [_valid_person(born=1800, died=1850)]
    make_data(tmp_path, persons, [])
    v = HistoricalValidator(str(tmp_path))
    result = v.load_and_validate()
    assert result is True
    assert v.errors == []
    assert v.warnings == []


def test_person_invalid_born(tmp_path):
    """born=-5 should produce an error."""
    persons = [_valid_person(born=-5, died=None)]
    make_data(tmp_path, persons, [])
    v = HistoricalValidator(str(tmp_path))
    result = v.load_and_validate()
    assert result is False
    assert any("born" in e.lower() or "-5" in e for e in v.errors)


def test_person_died_before_born(tmp_path):
    """died=1750 with born=1800 should produce an error."""
    persons = [_valid_person(born=1800, died=1750)]
    make_data(tmp_path, persons, [])
    v = HistoricalValidator(str(tmp_path))
    result = v.load_and_validate()
    assert result is False
    assert len(v.errors) >= 1


def test_person_extreme_age(tmp_path):
    """born=1700, died=1880 (180 years) should warn but not error."""
    persons = [_valid_person(born=1700, died=1880)]
    make_data(tmp_path, persons, [])
    v = HistoricalValidator(str(tmp_path))
    result = v.load_and_validate()
    assert result is True          # no errors
    assert len(v.errors) == 0
    assert len(v.warnings) >= 1
    assert any("extreme age" in w.lower() or "180" in w for w in v.warnings)


def test_person_missing_born(tmp_path):
    """A person dict without 'born' key should produce an error."""
    persons = [{"id": "P1", "name": "NoBorn"}]
    make_data(tmp_path, persons, [])
    v = HistoricalValidator(str(tmp_path))
    result = v.load_and_validate()
    assert result is False
    assert any("born" in e.lower() for e in v.errors)


# ===========================================================================
# 2. Referential integrity tests
# ===========================================================================

def test_missing_participant(tmp_path):
    """A relation referencing a nonexistent person ID should produce an error."""
    persons = [_valid_person("P1")]
    relations = [{"id": "R1", "type": "ALLIANCE", "participants": ["P1", "GHOST"], "year": 1820}]
    make_data(tmp_path, persons, relations)
    v = HistoricalValidator(str(tmp_path))
    result = v.load_and_validate()
    assert result is False
    assert any("GHOST" in e for e in v.errors)


def test_duplicate_participant(tmp_path):
    """A relation with the same person ID twice should produce an error."""
    persons = [_valid_person("P1")]
    relations = [{"id": "R1", "type": "ALLIANCE", "participants": ["P1", "P1"], "year": 1820}]
    make_data(tmp_path, persons, relations)
    v = HistoricalValidator(str(tmp_path))
    result = v.load_and_validate()
    assert result is False
    assert any("duplicate" in e.lower() for e in v.errors)


# ===========================================================================
# 3. Temporal consistency tests
# ===========================================================================

def test_event_before_person_born(tmp_path):
    """relation.year < person.born should produce an error."""
    persons = [_valid_person("P1", born=1830, died=1890)]
    relations = [{"id": "R1", "type": "BATTLE", "participants": ["P1"], "year": 1810}]
    make_data(tmp_path, persons, relations)
    v = HistoricalValidator(str(tmp_path))
    result = v.load_and_validate()
    assert result is False
    assert any("not yet born" in e for e in v.errors)


def test_event_after_person_died(tmp_path):
    """relation.year > person.died should produce an error."""
    persons = [_valid_person("P1", born=1800, died=1850)]
    relations = [{"id": "R1", "type": "BATTLE", "participants": ["P1"], "year": 1870}]
    make_data(tmp_path, persons, relations)
    v = HistoricalValidator(str(tmp_path))
    result = v.load_and_validate()
    assert result is False
    assert any("already dead" in e for e in v.errors)


# ===========================================================================
# 4. Genealogical integrity tests
# ===========================================================================

def test_parent_born_after_child(tmp_path):
    """LINEAGE parent.born > child.born should produce an error."""
    parent = _valid_person("P1", born=1860, died=1920)
    child  = _valid_person("P2", born=1840, died=1900)
    relations = [{"id": "R1", "type": "LINEAGE", "participants": ["P1", "P2"], "year": 1855}]
    make_data(tmp_path, [parent, child], relations)
    v = HistoricalValidator(str(tmp_path))
    result = v.load_and_validate()
    assert result is False
    assert any("LINEAGE" in e and "not before" in e for e in v.errors)


def test_parent_too_young(tmp_path):
    """Parent age at child birth < 12 should produce an error."""
    parent = _valid_person("P1", born=1840, died=1920)
    child  = _valid_person("P2", born=1848, died=1910)  # parent only 8 years older
    relations = [{"id": "R1", "type": "LINEAGE", "participants": ["P1", "P2"], "year": 1848}]
    make_data(tmp_path, [parent, child], relations)
    v = HistoricalValidator(str(tmp_path))
    result = v.load_and_validate()
    assert result is False
    assert any("implausibly young" in e for e in v.errors)


def test_genealogy_cycle_detected(tmp_path):
    """A→B→C→A cycle in LINEAGE relations should produce an error."""
    pa = _valid_person("PA", born=1700, died=1760)
    pb = _valid_person("PB", born=1720, died=1780)
    pc = _valid_person("PC", born=1740, died=1800)
    # PA→PB, PB→PC, PC→PA creates a cycle
    relations = [
        {"id": "R1", "type": "LINEAGE", "participants": ["PA", "PB"], "year": 1720},
        {"id": "R2", "type": "LINEAGE", "participants": ["PB", "PC"], "year": 1740},
        {"id": "R3", "type": "LINEAGE", "participants": ["PC", "PA"], "year": 1700},
    ]
    make_data(tmp_path, [pa, pb, pc], relations)
    v = HistoricalValidator(str(tmp_path))
    result = v.load_and_validate()
    assert result is False
    assert any("cycle" in e.lower() for e in v.errors)


# ===========================================================================
# 5. Cultural constraints tests
# ===========================================================================

def test_same_clan_marriage_error(tmp_path):
    """Two people with the same clan in a MARRIAGE relation should produce an error."""
    p1 = _valid_person("P1", clan="Dlamini", born=1800, died=1870)
    p2 = _valid_person("P2", clan="Dlamini", born=1805, died=1875)
    relations = [{"id": "R1", "type": "MARRIAGE", "participants": ["P1", "P2"], "year": 1825}]
    make_data(tmp_path, [p1, p2], relations)
    v = HistoricalValidator(str(tmp_path))
    result = v.load_and_validate()
    assert result is False
    assert any("endogamy" in e.lower() or "exogamy" in e.lower() for e in v.errors)


def test_different_clan_marriage_ok(tmp_path):
    """Two people with different clans in a MARRIAGE relation should produce no error."""
    p1 = _valid_person("P1", clan="Dlamini",  born=1800, died=1870)
    p2 = _valid_person("P2", clan="Nkosi",    born=1805, died=1875)
    relations = [{"id": "R1", "type": "MARRIAGE", "participants": ["P1", "P2"], "year": 1825}]
    make_data(tmp_path, [p1, p2], relations)
    v = HistoricalValidator(str(tmp_path))
    result = v.load_and_validate()
    assert result is True
    assert v.errors == []


# ===========================================================================
# 6. Loading / file-not-found / malformed JSON tests
# ===========================================================================

def test_missing_persons_file(tmp_path):
    """A data_path with no persons.json should raise FileNotFoundError."""
    # Write only relations.json
    (tmp_path / "relations.json").write_text(json.dumps({"relations": []}))
    v = HistoricalValidator(str(tmp_path))
    with pytest.raises((FileNotFoundError, OSError)):
        v._load_data()


def test_missing_relations_file(tmp_path):
    """A data_path with no relations.json should raise FileNotFoundError."""
    persons = [_valid_person()]
    (tmp_path / "persons.json").write_text(json.dumps({"persons": persons}))
    v = HistoricalValidator(str(tmp_path))
    with pytest.raises((FileNotFoundError, OSError)):
        v._load_data()


def test_malformed_json(tmp_path):
    """An invalid persons.json should raise json.JSONDecodeError."""
    (tmp_path / "persons.json").write_text("{ this is not valid json !!!")
    (tmp_path / "relations.json").write_text(json.dumps({"relations": []}))
    v = HistoricalValidator(str(tmp_path))
    with pytest.raises(json.JSONDecodeError):
        v._load_data()


# ===========================================================================
# 7. Report return-value tests
# ===========================================================================

def test_report_returns_true_on_pass(tmp_path):
    """Valid data: load_and_validate() returns True."""
    persons = [_valid_person("P1", born=1800, died=1860)]
    make_data(tmp_path, persons, [])
    v = HistoricalValidator(str(tmp_path))
    assert v.load_and_validate() is True


def test_report_returns_false_on_error(tmp_path):
    """Invalid data (missing born): load_and_validate() returns False."""
    persons = [{"id": "P1", "name": "NoBorn"}]
    make_data(tmp_path, persons, [])
    v = HistoricalValidator(str(tmp_path))
    assert v.load_and_validate() is False


# ===========================================================================
# 8. Additional coverage
# ===========================================================================

def test_multiple_persons_all_valid(tmp_path):
    """Multiple persons all passing should produce no errors and return True."""
    persons = [
        _valid_person("P1", born=1800, died=1860),
        _valid_person("P2", born=1820, died=1880),
        _valid_person("P3", born=1840, died=None),
    ]
    make_data(tmp_path, persons, [])
    v = HistoricalValidator(str(tmp_path))
    assert v.load_and_validate() is True


def test_relation_no_year_skips_temporal_check(tmp_path):
    """A relation without a year field should not produce temporal errors."""
    persons = [_valid_person("P1", born=1800, died=1860)]
    # Relation has no year — temporal check must skip gracefully
    relations = [{"id": "R1", "type": "ALLIANCE", "participants": ["P1"]}]
    make_data(tmp_path, persons, relations)
    v = HistoricalValidator(str(tmp_path))
    result = v.load_and_validate()
    assert result is True
    assert v.errors == []


def test_lineage_valid_parent_child(tmp_path):
    """Valid LINEAGE relation (parent older than child, age ≥ 12) passes cleanly."""
    parent = _valid_person("P1", born=1800, died=1870)
    child  = _valid_person("P2", born=1825, died=1890)
    relations = [{"id": "R1", "type": "LINEAGE", "participants": ["P1", "P2"], "year": 1825}]
    make_data(tmp_path, [parent, child], relations)
    v = HistoricalValidator(str(tmp_path))
    assert v.load_and_validate() is True


def test_succession_predecessor_older_passes(tmp_path):
    """SUCCESSION with predecessor born before successor produces no errors."""
    p1 = _valid_person("P1", born=1800, died=1860)
    p2 = _valid_person("P2", born=1840, died=1900)
    relations = [{"id": "R1", "type": "SUCCESSION", "participants": ["P1", "P2"], "year": 1860}]
    make_data(tmp_path, [p1, p2], relations)
    v = HistoricalValidator(str(tmp_path))
    assert v.load_and_validate() is True


def test_succession_predecessor_younger_errors(tmp_path):
    """SUCCESSION where predecessor born >= successor born should error."""
    p1 = _valid_person("P1", born=1850, died=1900)
    p2 = _valid_person("P2", born=1800, died=1860)
    relations = [{"id": "R1", "type": "SUCCESSION", "participants": ["P1", "P2"], "year": 1860}]
    make_data(tmp_path, [p1, p2], relations)
    v = HistoricalValidator(str(tmp_path))
    result = v.load_and_validate()
    assert result is False
    assert any("SUCCESSION" in e for e in v.errors)


def test_marriage_missing_clan_no_error(tmp_path):
    """MARRIAGE where participants lack clan fields should not trigger endogamy error."""
    p1 = {"id": "P1", "name": "Alice", "born": 1800, "died": 1860}
    p2 = {"id": "P2", "name": "Bob",   "born": 1800, "died": 1860}
    relations = [{"id": "R1", "type": "MARRIAGE", "participants": ["P1", "P2"], "year": 1825}]
    make_data(tmp_path, [p1, p2], relations)
    v = HistoricalValidator(str(tmp_path))
    assert v.load_and_validate() is True


def test_empty_persons_and_relations(tmp_path):
    """Empty data sets should pass with no errors or warnings."""
    make_data(tmp_path, [], [])
    v = HistoricalValidator(str(tmp_path))
    result = v.load_and_validate()
    assert result is True
    assert v.errors == []
    assert v.warnings == []


def test_person_born_zero_is_invalid(tmp_path):
    """born=0 should produce an error (must be > 0)."""
    persons = [_valid_person(born=0, died=None)]
    make_data(tmp_path, persons, [])
    v = HistoricalValidator(str(tmp_path))
    result = v.load_and_validate()
    assert result is False
    assert len(v.errors) >= 1
