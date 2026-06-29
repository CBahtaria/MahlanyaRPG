"""
Tests for pipeline/check_regression.py — performance regression detector.
"""

import json
import subprocess
import sys
import pytest

# Ensure pipeline package is importable when running from project root
sys.path.insert(0, str(__import__("pathlib").Path(__file__).resolve().parents[2]))

from pipeline.check_regression import load_metrics, check_regression


# ---------------------------------------------------------------------------
# Helper
# ---------------------------------------------------------------------------

def write_metrics(tmp_path, filename, data):
    p = tmp_path / filename
    p.write_text(json.dumps(data))
    return str(p)


# ---------------------------------------------------------------------------
# Baseline fixture data
# ---------------------------------------------------------------------------

BASELINE = {
    "frame_time_ms": 16.5,
    "fps": 60.1,
    "memory_mb": 1024.0,
    "event_batch_ms": 1.8,
    "network_batch_ms": 0.9,
    "trust_score": 0.95,
    "headroom_fraction": 0.72,
}


# ---------------------------------------------------------------------------
# Test 1: identical metrics — everything passes
# ---------------------------------------------------------------------------

def test_no_regressions_passes(tmp_path):
    bf = write_metrics(tmp_path, "baseline.json", BASELINE)
    cf = write_metrics(tmp_path, "current.json", BASELINE)
    passed, messages = check_regression(bf, cf)
    assert passed is True
    assert len(messages) > 0
    assert all("[FAIL]" not in m for m in messages)


# ---------------------------------------------------------------------------
# Test 2: frame_time_ms increases by 20% — regression
# ---------------------------------------------------------------------------

def test_frame_time_regression_fails(tmp_path):
    current = dict(BASELINE)
    current["frame_time_ms"] = BASELINE["frame_time_ms"] * 1.20  # 20% worse
    bf = write_metrics(tmp_path, "baseline.json", BASELINE)
    cf = write_metrics(tmp_path, "current.json", current)
    passed, messages = check_regression(bf, cf)
    assert passed is False
    fail_msgs = [m for m in messages if "[FAIL]" in m and "frame_time_ms" in m]
    assert len(fail_msgs) == 1


# ---------------------------------------------------------------------------
# Test 3: fps drops by 20% — regression (higher_is_better)
# ---------------------------------------------------------------------------

def test_fps_regression_fails(tmp_path):
    current = dict(BASELINE)
    current["fps"] = BASELINE["fps"] * 0.80  # 20% drop
    bf = write_metrics(tmp_path, "baseline.json", BASELINE)
    cf = write_metrics(tmp_path, "current.json", current)
    passed, messages = check_regression(bf, cf)
    assert passed is False
    fail_msgs = [m for m in messages if "[FAIL]" in m and "fps" in m]
    assert len(fail_msgs) == 1


# ---------------------------------------------------------------------------
# Test 4: frame_time_ms increases by 10% — within threshold, passes
# ---------------------------------------------------------------------------

def test_within_threshold_passes(tmp_path):
    current = dict(BASELINE)
    current["frame_time_ms"] = BASELINE["frame_time_ms"] * 1.10  # 10% worse, under 15%
    bf = write_metrics(tmp_path, "baseline.json", BASELINE)
    cf = write_metrics(tmp_path, "current.json", current)
    passed, messages = check_regression(bf, cf)
    assert passed is True
    # Should have a PASS message for frame_time_ms
    pass_msgs = [m for m in messages if "[PASS]" in m and "frame_time_ms" in m]
    assert len(pass_msgs) == 1


# ---------------------------------------------------------------------------
# Test 5: current missing one metric — warns but still passes
# ---------------------------------------------------------------------------

def test_missing_current_metric_warns_but_passes(tmp_path):
    current = dict(BASELINE)
    del current["memory_mb"]
    bf = write_metrics(tmp_path, "baseline.json", BASELINE)
    cf = write_metrics(tmp_path, "current.json", current)
    passed, messages = check_regression(bf, cf)
    assert passed is True
    warn_msgs = [m for m in messages if "[WARN]" in m and "memory_mb" in m]
    assert len(warn_msgs) == 1


# ---------------------------------------------------------------------------
# Test 6: baseline file does not exist — raises FileNotFoundError
# ---------------------------------------------------------------------------

def test_missing_baseline_file_raises(tmp_path):
    cf = write_metrics(tmp_path, "current.json", BASELINE)
    with pytest.raises(FileNotFoundError):
        check_regression(str(tmp_path / "nonexistent_baseline.json"), cf)


# ---------------------------------------------------------------------------
# Test 7: malformed JSON in current file — raises ValueError
# ---------------------------------------------------------------------------

def test_malformed_json_raises(tmp_path):
    bf = write_metrics(tmp_path, "baseline.json", BASELINE)
    bad_path = tmp_path / "bad.json"
    bad_path.write_text("{not valid json")
    with pytest.raises(ValueError):
        check_regression(bf, str(bad_path))


# ---------------------------------------------------------------------------
# Test 8: all metrics improved — should all pass
# ---------------------------------------------------------------------------

def test_all_metrics_improved_passes(tmp_path):
    current = {
        "frame_time_ms": BASELINE["frame_time_ms"] * 0.90,   # faster
        "fps": BASELINE["fps"] * 1.10,                        # higher fps
        "memory_mb": BASELINE["memory_mb"] * 0.95,            # less memory
        "event_batch_ms": BASELINE["event_batch_ms"] * 0.85,  # faster
        "network_batch_ms": BASELINE["network_batch_ms"] * 0.80,
        "trust_score": BASELINE["trust_score"] * 1.02,
        "headroom_fraction": BASELINE["headroom_fraction"] * 1.05,
    }
    bf = write_metrics(tmp_path, "baseline.json", BASELINE)
    cf = write_metrics(tmp_path, "current.json", current)
    passed, messages = check_regression(bf, cf)
    assert passed is True
    assert all("[FAIL]" not in m for m in messages)


# ---------------------------------------------------------------------------
# Test 9: baseline value is 0.0 — check is skipped, no ZeroDivisionError
# ---------------------------------------------------------------------------

def test_baseline_zero_skipped(tmp_path):
    baseline = dict(BASELINE)
    baseline["event_batch_ms"] = 0.0
    current = dict(BASELINE)
    current["event_batch_ms"] = 999.0  # huge value, but baseline is 0 so should be skipped
    bf = write_metrics(tmp_path, "baseline.json", baseline)
    cf = write_metrics(tmp_path, "current.json", current)
    # Should not raise ZeroDivisionError and should still pass
    passed, messages = check_regression(bf, cf)
    skip_msgs = [m for m in messages if "event_batch_ms" in m and ("SKIP" in m or "skip" in m.lower())]
    assert len(skip_msgs) == 1
    # Other metrics are identical so overall should pass
    assert passed is True


# ---------------------------------------------------------------------------
# Test 10: CLI wrong arg count — exit code 2
# ---------------------------------------------------------------------------

def test_cli_wrong_arg_count(tmp_path):
    script = str(
        __import__("pathlib").Path(__file__).resolve().parents[2]
        / "pipeline"
        / "check_regression.py"
    )
    result = subprocess.run(
        [sys.executable, script],  # no args
        capture_output=True,
        text=True,
    )
    assert result.returncode == 2

    result2 = subprocess.run(
        [sys.executable, script, "only_one_arg.json"],  # one arg
        capture_output=True,
        text=True,
    )
    assert result2.returncode == 2


# ---------------------------------------------------------------------------
# Test 11 (bonus): trust_score regression
# ---------------------------------------------------------------------------

def test_trust_score_regression_fails(tmp_path):
    current = dict(BASELINE)
    current["trust_score"] = BASELINE["trust_score"] * 0.80  # 20% drop
    bf = write_metrics(tmp_path, "baseline.json", BASELINE)
    cf = write_metrics(tmp_path, "current.json", current)
    passed, messages = check_regression(bf, cf)
    assert passed is False
    fail_msgs = [m for m in messages if "[FAIL]" in m and "trust_score" in m]
    assert len(fail_msgs) == 1


# ---------------------------------------------------------------------------
# Test 12 (bonus): multiple regressions reported in messages
# ---------------------------------------------------------------------------

def test_multiple_regressions_reported(tmp_path):
    current = dict(BASELINE)
    current["frame_time_ms"] = BASELINE["frame_time_ms"] * 1.30  # 30% worse
    current["memory_mb"] = BASELINE["memory_mb"] * 1.25          # 25% worse
    bf = write_metrics(tmp_path, "baseline.json", BASELINE)
    cf = write_metrics(tmp_path, "current.json", current)
    passed, messages = check_regression(bf, cf)
    assert passed is False
    fail_msgs = [m for m in messages if "[FAIL]" in m]
    assert len(fail_msgs) == 2


# ---------------------------------------------------------------------------
# Test 13 (bonus): memory_mb regression
# ---------------------------------------------------------------------------

def test_memory_mb_regression_fails(tmp_path):
    current = dict(BASELINE)
    current["memory_mb"] = BASELINE["memory_mb"] * 1.20  # 20% more memory used
    bf = write_metrics(tmp_path, "baseline.json", BASELINE)
    cf = write_metrics(tmp_path, "current.json", current)
    passed, messages = check_regression(bf, cf)
    assert passed is False
    fail_msgs = [m for m in messages if "[FAIL]" in m and "memory_mb" in m]
    assert len(fail_msgs) == 1


# ---------------------------------------------------------------------------
# Test 14 (bonus): JSON root is a list — raises ValueError
# ---------------------------------------------------------------------------

def test_json_root_list_raises(tmp_path):
    bf = write_metrics(tmp_path, "baseline.json", BASELINE)
    list_path = tmp_path / "list.json"
    list_path.write_text(json.dumps([1, 2, 3]))
    with pytest.raises(ValueError):
        check_regression(bf, str(list_path))


# ---------------------------------------------------------------------------
# Test 15 (bonus): extra metrics in current (not in baseline) are ignored
# ---------------------------------------------------------------------------

def test_extra_current_metrics_ignored(tmp_path):
    current = dict(BASELINE)
    current["some_new_metric"] = 42.0  # not in baseline
    bf = write_metrics(tmp_path, "baseline.json", BASELINE)
    cf = write_metrics(tmp_path, "current.json", current)
    passed, messages = check_regression(bf, cf)
    assert passed is True
    # No message should fail for some_new_metric
    fail_msgs = [m for m in messages if "[FAIL]" in m and "some_new_metric" in m]
    assert len(fail_msgs) == 0
