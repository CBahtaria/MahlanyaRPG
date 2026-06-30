#!/usr/bin/env bash
# MahlanyaRPG — Steam Deployment Script
# Usage: ./scripts/automation/steam_deploy.sh [--branch internal_qa|public_beta|default]
#
# Prerequisites:
#   - STEAM_USERNAME and STEAM_PASSWORD env vars set (use CI secrets)
#   - SteamCMD installed at /usr/games/steamcmd or ~/steamcmd/steamcmd.sh
#   - UE5 Shipping build output at Build/Windows/
#   - VDF configs in config/builds/steam/
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"
VDF_DIR="${REPO_ROOT}/config/builds/steam"
BUILD_DIR="${REPO_ROOT}/Build/Windows"
BRANCH="${1:-}"
DEPLOY_BRANCH="${BRANCH#--branch=}"
DEPLOY_BRANCH="${DEPLOY_BRANCH:-internal_qa}"

# Locate SteamCMD
if command -v steamcmd &>/dev/null; then
    STEAMCMD="steamcmd"
elif [ -f "${HOME}/steamcmd/steamcmd.sh" ]; then
    STEAMCMD="${HOME}/steamcmd/steamcmd.sh"
elif [ -f "/usr/games/steamcmd" ]; then
    STEAMCMD="/usr/games/steamcmd"
else
    echo "ERROR: SteamCMD not found. Install: https://developer.valvesoftware.com/wiki/SteamCMD"
    exit 1
fi

# Validate environment
: "${STEAM_USERNAME:?STEAM_USERNAME env var required}"
: "${STEAM_PASSWORD:?STEAM_PASSWORD env var required}"

if [ ! -d "${BUILD_DIR}" ]; then
    echo "ERROR: Build output not found at ${BUILD_DIR}"
    echo "Run UE5 package step first: UnrealEditor-Cmd ... -run=BuildCookRun -platform=Win64 -configuration=Shipping"
    exit 1
fi

if [ ! -f "${VDF_DIR}/app_build.vdf" ]; then
    echo "ERROR: app_build.vdf not found at ${VDF_DIR}"
    exit 1
fi

echo "=== MahlanyaRPG Steam Deployment ==="
echo "Branch: ${DEPLOY_BRANCH}"
echo "Build:  ${BUILD_DIR}"
echo "VDF:    ${VDF_DIR}/app_build.vdf"
echo ""

# Inject branch into a temp VDF copy
TEMP_VDF="$(mktemp /tmp/app_build_XXXXXX.vdf)"
sed "s/\"SetLive\".*\".*\"/\"SetLive\" \"${DEPLOY_BRANCH}\"/" \
    "${VDF_DIR}/app_build.vdf" > "${TEMP_VDF}"

# Run SteamCMD upload
"${STEAMCMD}" \
    +login "${STEAM_USERNAME}" "${STEAM_PASSWORD}" \
    +run_app_build "${TEMP_VDF}" \
    +quit

rm -f "${TEMP_VDF}"
echo ""
echo "=== Deployment to '${DEPLOY_BRANCH}' complete ==="
