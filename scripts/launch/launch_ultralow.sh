#!/usr/bin/env bash
# MahlanyaRPG — UltraLowEnd launch (Linux, integrated GPU)
# Forces Vulkan with minimal feature set; 720p windowed
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
GAME_BIN="$SCRIPT_DIR/../../MahlanyaRPG/Binaries/Linux/MahlanyaRPG"

if [[ ! -f "$GAME_BIN" ]]; then
    echo "ERROR: Game binary not found at $GAME_BIN"
    echo "       Package the game first from the UE5 editor."
    exit 1
fi

exec "$GAME_BIN" \
    -vulkan \
    -ResX=1280 -ResY=720 \
    -windowed \
    -mahlanya.ForceHardwareTier=0 \
    -nologbatching \
    "$@"
