#!/usr/bin/env python3
"""
MahlanyaRPG Simulation Harness — dev testing without UE5.

Mirrors the logic of:
  UMahlanyaGameClockSubsystem (game clock, day/year advance)
  UEconomySimulatorSubsystem  (cattle economy, drought stress, concession spread)
  UEcologySimulatorSubsystem  (Lotka-Volterra population grid stub)
  UHistoricalCalendarSubsystem (historical events by year)

No GPU required. Runs on any Python 3.11+ machine.
Usage:
  python scripts/simulation_harness.py --years 10
  python scripts/simulation_harness.py --years 100 --start-year 1815
"""

import argparse, json, math, random, sys
from dataclasses import dataclass, field, asdict
from pathlib import Path
from typing import Optional

# ── Data classes mirroring UE5 structs ──────────────────────────────────────

@dataclass
class ClanState:
    clan_id: str
    cattle_count: int = 200
    political_strength: float = 0.5
    colonial_pressure: float = 0.0
    drought_stress: float = 0.0

@dataclass
class HistoricalEvent:
    event_id: str
    year: int
    description: str
    fired: bool = False

@dataclass
class SimulationState:
    game_year: int = 1750
    game_day: int = 0
    day_of_year: int = 0
    clans: dict = field(default_factory=dict)
    fired_events: list = field(default_factory=list)
    rain_accumulation_mm: float = 0.0
    drought_index: float = 0.0

# ── Historical events (mirrors pipeline/history/data/events.json) ────────────

HISTORICAL_EVENTS = [
    HistoricalEvent("EV_MfecaneBegin",     1815, "Mfecane begins — waves of displacement from the north"),
    HistoricalEvent("EV_LubomboBattle",    1846, "Battle of Lubombo — Mswati II repels Gaza incursion"),
    HistoricalEvent("EV_ConcessionRush",   1880, "Colonial concession rush begins — land rights under pressure"),
    HistoricalEvent("EV_BhunuTrial",       1898, "King Bhunu's trial by colonial authorities"),
    HistoricalEvent("EV_Partition",        1906, "Eswatini partitioned under colonial administration"),
]

# ── Economy simulation (mirrors UEconomySimulatorSubsystem) ─────────────────

def simulate_economy_tick(state: SimulationState, days: float = 1.0) -> list[str]:
    """One game-day economy tick. Returns list of narrative events."""
    events = []
    for clan_id, clan in state.clans.items():
        # Drought stress reduces cattle
        if clan.drought_stress > 0.5 and random.random() < 0.02 * days:
            lost = max(1, int(clan.cattle_count * 0.05))
            clan.cattle_count = max(0, clan.cattle_count - lost)
            events.append(f"[Economy] Clan {clan_id}: drought killed {lost} cattle → {clan.cattle_count} remain")

        # Cattle raid probability: drought × weakness
        raid_prob = clan.drought_stress * (1.0 - clan.political_strength) * 0.3 * days
        if random.random() < raid_prob:
            raided = max(1, int(clan.cattle_count * 0.10))
            clan.cattle_count = max(0, clan.cattle_count - raided)
            events.append(f"[Raid] Clan {clan_id} raided — lost {raided} cattle → {clan.cattle_count} remain")

        # Colonial concession infection
        if clan.colonial_pressure > 0.6 and clan.political_strength < 0.4:
            if random.random() < 0.01 * days:
                clan.colonial_pressure = min(1.0, clan.colonial_pressure + 0.05)
                events.append(f"[Colonial] Clan {clan_id} concession pressure rising → {clan.colonial_pressure:.2f}")

    return events

# ── Weather / drought (mirrors UMicroclimateSubsystem) ──────────────────────

def simulate_weather_tick(state: SimulationState, day_of_year: int) -> None:
    """Seasonal rainfall model. Rainy season Oct–Mar (days 274–365 + 0–90)."""
    in_rainy_season = day_of_year > 274 or day_of_year < 90
    if in_rainy_season:
        state.rain_accumulation_mm += random.uniform(0, 15)
    else:
        state.rain_accumulation_mm = max(0, state.rain_accumulation_mm - 3)
    state.rain_accumulation_mm = min(state.rain_accumulation_mm, 400)

    saturation = min(1.0, state.rain_accumulation_mm / 200.0)
    state.drought_index = max(0.0, 1.0 - saturation)
    for clan in state.clans.values():
        clan.drought_stress = state.drought_index

# ── Historical calendar (mirrors UHistoricalCalendarSubsystem) ───────────────

def check_historical_events(state: SimulationState) -> list[str]:
    messages = []
    for ev in HISTORICAL_EVENTS:
        if not ev.fired and state.game_year >= ev.year:
            ev.fired = True
            state.fired_events.append(ev.event_id)
            messages.append(f"[History] *** {ev.event_id} ({ev.year}): {ev.description} ***")
    return messages

# ── Main simulation loop ─────────────────────────────────────────────────────

def run_simulation(start_year: int, num_years: int, output_path: Optional[Path] = None) -> SimulationState:
    state = SimulationState(game_year=start_year)

    # Seed clans (mirrors Economy subsystem initialization)
    for clan_id in ["Nkosi_Dlamini", "Matsebula", "Motsa", "Mdluli", "Fakudze"]:
        state.clans[clan_id] = ClanState(
            clan_id=clan_id,
            cattle_count=random.randint(80, 300),
            political_strength=random.uniform(0.3, 0.8),
            colonial_pressure=0.0 if start_year < 1870 else random.uniform(0.0, 0.3),
        )

    days_per_year = 365
    total_days = num_years * days_per_year
    print(f"\nMahlanyaRPG Simulation Harness")
    print(f"Start: {start_year} | Duration: {num_years} years ({total_days:,} game-days)")
    print("=" * 60)

    for day in range(total_days):
        state.game_day += 1
        state.day_of_year = day % days_per_year

        simulate_weather_tick(state, state.day_of_year)
        eco_events = simulate_economy_tick(state)
        for e in eco_events:
            print(e)

        # Year boundary — fire at last day of each year
        if state.day_of_year == days_per_year - 1:
            state.game_year += 1
            hist_events = check_historical_events(state)
            for e in hist_events:
                print(e)
            total_cattle = sum(c.cattle_count for c in state.clans.values())
            print(f"[Year {state.game_year}] Total cattle: {total_cattle:,} | Drought: {state.drought_index:.2f}")

    if output_path:
        output_path.parent.mkdir(parents=True, exist_ok=True)
        with open(output_path, "w") as f:
            json.dump({
                "game_year": state.game_year,
                "game_day": state.game_day,
                "clans": {k: asdict(v) for k, v in state.clans.items()},
                "fired_events": state.fired_events,
                "drought_index": state.drought_index,
            }, f, indent=2)
        print(f"\nSimulation state saved to {output_path}")

    return state

def main():
    parser = argparse.ArgumentParser(description="MahlanyaRPG simulation harness (no GPU required)")
    parser.add_argument("--years",       type=int, default=10,   help="Number of game-years to simulate")
    parser.add_argument("--start-year",  type=int, default=1750, help="Starting game year")
    parser.add_argument("--output",      type=str, default=None, help="JSON output path for final state")
    parser.add_argument("--seed",        type=int, default=None, help="Random seed for reproducibility")
    args = parser.parse_args()

    if args.seed is not None:
        random.seed(args.seed)

    output = Path(args.output) if args.output else Path("Saved/SimulationState_final.json")
    state = run_simulation(args.start_year, args.years, output)

    total_cattle = sum(c.cattle_count for c in state.clans.values())
    print(f"\nFinal state: Year {state.game_year} | {total_cattle:,} cattle | {len(state.fired_events)} historical events fired")
    return 0

if __name__ == "__main__":
    sys.exit(main())
