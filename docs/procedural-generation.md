# Procedural Map Generation — WaveFunctionCollapse

## Current approach
Centroidal Voronoi Tessellation (CVT) for settlement placement constrained by Swazi social tensors.
Terrain: Copernicus DEM 10m + 5,000 geomorphological passes.

## WFC integration plan

WaveFunctionCollapse (mxgmn/WaveFunctionCollapse) generates locally-valid tile maps by:
1. Sample real 19th-century Swazi homestead layouts (from archival maps)
2. Extract valid adjacency rules (which tiles can neighbor which)
3. Run WFC to generate new valid homestead layouts that are statistically consistent with historical samples

### UE5 implementation path
- WFC runs as a Python offline tool: generate tilemap → export JSON → import to UE5 as DataTable
- Tiles: grass, homestead-center (inkhandla), livestock-pen, cultivation-field, path, boundary-fence
- Constraint: inkhandla (central house) must be surrounded by at minimum 2 umsamo (shrine) tiles

### Why not runtime WFC
UE5 PCG (Procedural Content Generation) framework supports runtime generation, but WFC's
constraint propagation on 19th-century historical adjacency rules is better run offline to
validate against the KnowledgeGraphPlugin CI check.

### Integration point
WFC output feeds into the existing settlement placement pipeline before CVT, not after.
CVT governs macro-placement (village locations relative to terrain + water).
WFC governs micro-layout (individual homestead internal structure).
