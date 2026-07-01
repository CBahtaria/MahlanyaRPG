# Mahlanya RPG — Press Kit

## Fact Sheet

**Developer:** BRT Inc. (Charles Bartaria)
**Location:** Manzini, Kingdom of Eswatini
**Platforms:** PC (Steam), iOS, Android (console future phase)
**Genre:** Historical 3D RPG
**Setting:** Kingdom of Eswatini (Swaziland), 1750–1906
**Players:** 1–4 (co-op)
**Engine:** Unreal Engine 5
**Release:** TBD

---

## Description

Mahlanya RPG is a historical open-world RPG set in the Kingdom of Eswatini across 156 years of documented Swazi history. Players take the role of Mahlanya — a young Swazi man whose life spans pre-colonial kingdom formation (~1750), the Mfecane upheaval (1815–1840), the Battle of Lubombo (1846), and the colonial resistance period ending with British annexation in 1906. The game is built on Unreal Engine 5 with terrain derived from real GIS elevation data, and enforces historical accuracy through a knowledge graph that prevents NPCs from asserting historically false claims.

The economic core of the game is cattle: the currency, social capital, and spiritual resource of the Swazi political economy. Cultural protocols — Inhlonipho avoidance vocabulary, greeting hierarchies, ceremonial obligations — govern NPC interactions throughout. The game is voiced in siSwati with English subtitles. A pre-baked mobile tier delivers the same world at adaptive fidelity on iOS and Android, fully offline-capable.

---

## Key Features

- **Geophysically accurate terrain:** Highveld, Middleveld, Lowveld, and Lubombo zones derived from real GIS elevation data via a Zig SIMD erosion pipeline; seasonal hydrology simulated
- **Cattle-based economy:** Cattle as currency, political capital, and spiritual resource; herd management, lobola negotiations, tribute diplomacy, and cattle raid missions drive the political economy
- **Cultural protocol enforcement:** Inhlonipho vocabulary rules govern all NPC dialogue; correct greetings, address hierarchies, and ceremonial participation affect quest availability and standing
- **Historical accuracy via knowledge graph:** 10 simulation plugins; no NPC can assert a historically false claim; emergent narrative generated from simulation state (drought, colonial pressure, seasonal economy)
- **Four playable historical eras:** Pre-colonial formation, Mfecane upheaval, Battle of Lubombo (playable, not cutscene), colonial resistance — each with distinct political pressures and narrative structures
- **Co-op multiplayer (2–4 players):** Cattle raid and diplomatic escort missions with shared cultural protocol rules and combined cattle-holdings economy
- **Pre-baked mobile tier:** iOS and Android builds with hardware-adaptive NPC scaling (15–200 NPCs), 60fps target on mid-range devices, full offline playability, touch-optimised controls
- **Cross-platform save:** Steam Cloud on PC; iOS and Android saves compatible; 12 achievement milestones tracking historical and cultural progression

---

## The Developer

Charles Bartaria is the founder of BRT Inc. and the lead developer of Mahlanya RPG. He was born and raised in Manzini, Kingdom of Eswatini — the same country the game depicts — and built the project as an explicitly Swazi-authored account of Swazi history at a time when no game set in Eswatini exists. BRT Inc. operates from Manzini and the development pipeline is grounded in locally sourced historical research, GIS data covering Eswatini's actual terrain, and cultural consultation on ceremony depiction. Mahlanya RPG is not a game about Africa as a generic setting: it is about one specific kingdom, one specific history, and the specific people who lived through it.

---

## Technical Highlights

- **Zig SIMD terrain pipeline:** Custom erosion simulation producing heightfields from GIS source data; Nanite Landscape geometry at PC Ultra tier
- **10 runtime simulation plugins:** Weather, cattle health, political pressure, seasonal economy, historical event triggers, NPC social graph, Inhlonipho protocol state, Voronoi settlement placement, audio propagation, scalability tier manager
- **Trilingual asset pipeline:** siSwati voice, English subtitle/gloss, and Zulu reference layer for Mfecane-era NPC populations
- **Scientific data sources:** SRTM and local DEM elevation data for Eswatini; historical population distribution records for settlement seeding; documented primary sources for knowledge graph events
- **Lumen global illumination + 64-ray geometric audio** at Ultra; hardware-adaptive path to LowEnd mobile (pre-baked lighting, pre-computed audio occlusion)

---

## Media Contact

hello@brtinc.dev | https://brtinc.dev
