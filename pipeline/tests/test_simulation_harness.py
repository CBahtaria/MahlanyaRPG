"""Tests for the Python simulation harness."""
import sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).parent.parent.parent / "scripts"))

from simulation_harness import run_simulation, SimulationState, HISTORICAL_EVENTS

def test_basic_run():
    state = run_simulation(start_year=1750, num_years=5)
    assert state.game_year == 1755
    assert state.game_day == 5 * 365

def test_mfecane_fires():
    """Mfecane should fire when year reaches 1815."""
    state = run_simulation(start_year=1810, num_years=10)
    assert "EV_MfecaneBegin" in state.fired_events

def test_concession_rush_fires():
    state = run_simulation(start_year=1878, num_years=5)
    assert "EV_ConcessionRush" in state.fired_events

def test_cattle_never_negative():
    state = run_simulation(start_year=1815, num_years=50, output_path=None)
    for clan in state.clans.values():
        assert clan.cattle_count >= 0

def test_drought_index_bounded():
    state = run_simulation(start_year=1750, num_years=3)
    assert 0.0 <= state.drought_index <= 1.0

def test_reproducible_with_seed(tmp_path):
    import random
    random.seed(42)
    s1 = run_simulation(start_year=1750, num_years=2)
    random.seed(42)
    s2 = run_simulation(start_year=1750, num_years=2)
    assert s1.game_year == s2.game_year
    assert s1.fired_events == s2.fired_events
