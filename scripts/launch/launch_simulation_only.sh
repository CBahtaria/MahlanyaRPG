#!/usr/bin/env bash
# MahlanyaRPG — Headless simulation mode (no GPU required)
# Runs the game clock, economy, ecology, history, and narrative subsystems
# with -nullrhi (no renderer). Output logged to Saved/Logs/SimulationOnly.log
# Usage: ./launch_simulation_only.sh [--days N]
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
GAME_BIN="$SCRIPT_DIR/../../MahlanyaRPG/Binaries/Linux/MahlanyaRPGServer"
GAME_MAP="SimulationOnlyMap"
DAYS="${1:-365}"

if [[ ! -f "$GAME_BIN" ]]; then
    echo "ERROR: Server binary not found at $GAME_BIN"
    echo "       Build the dedicated server target from UE5:"
    echo "       UnrealBuildTool MahlanyaRPGServer Linux Development"
    exit 1
fi

echo "Starting headless simulation for $DAYS game-days..."
exec "$GAME_BIN" \
    "$GAME_MAP" \
    -nullrhi \
    -game \
    -log \
    -LogCmds="LogMahlanyaSimulation Verbose" \
    -SimulationDays="$DAYS" \
    "$@"
