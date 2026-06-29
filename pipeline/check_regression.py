"""
Compares a baseline performance metrics JSON against a current metrics JSON.
Fails if any metric regressed by more than max_regression_fraction (default 0.15 = 15%).
Usage: python pipeline/check_regression.py baseline.json current.json
Exit 0 = no regression. Exit 1 = regression detected. Exit 2 = wrong arguments.
"""

import json

# Metrics where a higher value is worse (latency, memory, etc.)
HIGHER_IS_WORSE = {"frame_time_ms", "memory_mb", "event_batch_ms", "network_batch_ms"}

# Metrics where a higher value is better (fps, trust, headroom, etc.)
HIGHER_IS_BETTER = {"fps", "trust_score", "headroom_fraction"}


def load_metrics(path: str) -> dict:
    """
    Loads {metric_name: value} from a JSON file.
    Raises FileNotFoundError if file missing.
    Raises ValueError if JSON is malformed or root is not a dict.
    """
    try:
        with open(path, "r") as f:
            raw = f.read()
    except FileNotFoundError:
        raise FileNotFoundError(f"Metrics file not found: {path}")

    try:
        data = json.loads(raw)
    except json.JSONDecodeError as exc:
        raise ValueError(f"Malformed JSON in {path}: {exc}") from exc

    if not isinstance(data, dict):
        raise ValueError(
            f"Expected a JSON object at root of {path}, got {type(data).__name__}"
        )

    return data


def check_regression(
    baseline_path: str,
    current_path: str,
    max_regression: float = 0.15,
) -> tuple:
    """
    Returns (passed: bool, messages: list[str]).
    - Loads both files via load_metrics()
    - For each metric in baseline: check if current has it (warn if missing, continue)
    - For higher_is_worse: regression if current > baseline * (1 + max_regression)
    - For higher_is_better: regression if current < baseline * (1 - max_regression)
    - Special case: if baseline value is 0, skip the check
    - Messages include: "[PASS] metric: X -> Y" or "[FAIL] metric REGRESSED: X -> Y (threshold: Z)"
    - passed = True only if no regressions found
    """
    baseline = load_metrics(baseline_path)
    current = load_metrics(current_path)

    passed = True
    messages = []

    for metric, base_val in baseline.items():
        if metric not in current:
            messages.append(f"[WARN] {metric}: missing from current metrics (skipping)")
            continue

        cur_val = current[metric]

        # Skip check when baseline is zero to avoid meaningless thresholds
        if base_val == 0:
            messages.append(f"[SKIP] {metric}: baseline is 0, skipping threshold check")
            continue

        if metric in HIGHER_IS_WORSE:
            threshold = base_val * (1 + max_regression)
            if cur_val > threshold:
                passed = False
                messages.append(
                    f"[FAIL] {metric} REGRESSED: {base_val} -> {cur_val}"
                    f" (threshold: {threshold:.4f})"
                )
            else:
                messages.append(f"[PASS] {metric}: {base_val} -> {cur_val}")
        elif metric in HIGHER_IS_BETTER:
            threshold = base_val * (1 - max_regression)
            if cur_val < threshold:
                passed = False
                messages.append(
                    f"[FAIL] {metric} REGRESSED: {base_val} -> {cur_val}"
                    f" (threshold: {threshold:.4f})"
                )
            else:
                messages.append(f"[PASS] {metric}: {base_val} -> {cur_val}")
        else:
            # Unknown metric — report but do not regress
            messages.append(
                f"[INFO] {metric}: {base_val} -> {cur_val} (classification unknown, skipping)"
            )

    return passed, messages


def main():
    import sys

    if len(sys.argv) != 3:
        print("Usage: python check_regression.py <baseline.json> <current.json>")
        sys.exit(2)

    passed, messages = check_regression(sys.argv[1], sys.argv[2])
    for msg in messages:
        print(msg)
    sys.exit(0 if passed else 1)


if __name__ == "__main__":
    main()
