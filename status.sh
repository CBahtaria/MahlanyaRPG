#!/bin/bash
echo "=== 🔍 Quick Status ==="
echo ""
echo "📁 DEM Tiles: $(ls ~/MahlanyaRPG/outputs/dem/tiles/*.tif 2>/dev/null | wc -l) files"
echo "📦 Disk: $(df -h ~ | tail -1 | awk '{print $5}') used"
echo ""
echo "🔄 Running:"
ps aux | grep -E "acquire_dem|erode|python" | grep -v grep | head -5
echo ""
echo "🌐 Web: http://192.168.110.200:3001"
