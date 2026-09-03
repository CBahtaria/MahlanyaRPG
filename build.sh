#!/bin/bash
set -e

echo "=== 🏗️ MahlanyaRPG Build Started ==="
echo "Time: $(date)"

cd ~/MahlanyaRPG

# Pull latest
git pull

# Setup environment
export PYTHONPATH="${PYTHONPATH}:$HOME/MahlanyaRPG:$HOME/MahlanyaRPG/pipeline"
export PATH="${PATH}:$HOME/MahlanyaRPG/pipeline"

# Run full pipeline
cd pipeline
echo "📦 Running full pipeline..."

# Run each target individually with proper parameters
make terrain-acquire
make terrain-hardness
make terrain-erode EROSION_ITERS=5000
make terrain-rivers
make terrain-export
make history-graph
make history-validate

# Build web
echo "🌐 Building web viewer..."
cd ~/MahlanyaRPG/web
npm install
npm run build

echo "✅ Build complete: $(date)"
