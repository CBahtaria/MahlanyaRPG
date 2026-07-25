# MahlanyaRPG — Project CLAUDE.md

Scientifically-grounded 3D RPG built on UE5 + Zig + Python. Real Copernicus 10m DEM of Eswatini, Swazi cultural geometry, geomorphological erosion sim. Phase 10 production-hardening complete; pre-launch.

## Hard lines (non-negotiable)

1. **Historical accuracy CI gate is authoritative.** `scripts/historical_accuracy_check.py` must pass. Any anachronism (post-1902 imported technology in pre-colonial settlement layouts, non-Swazi social geometry in villages, wrong crops for latitude/altitude) is a bug, not a stylistic choice.
2. **Performance regression bounds are tight.** ±15% tolerance on all 19 performance CVars across the 5 hardware tiers. A commit that widens frame-time variance on the UltraLowEnd tier is a bug, even if it helps Ultra.
3. **No Nanite bypass.** All static meshes route through Nanite. Legacy LOD chains are not accepted for new content. If a mesh cannot be Naniteified, it does not ship.
4. **Zig SIMD compute layer owns the erosion / hydrology math.** Do not port it back into Python for convenience. The 5000-pass geomorphology sim is the moat; keep it in Zig.
5. **Cultural review before merge on any settlement geometry change.** `docs/cultural-review-protocol.md` — reviewer must not be the author.
6. **UE5 commandlet outputs are deterministic.** Same seed + same DEM tile → same output bytes. Non-determinism in `Source/Mahlanya/Commandlets/*` is a bug.
7. **316 tests must pass.** No skipping. If a test flakes, fix it or delete it with justification in the commit message; do not paper over with retries.
8. **Steam achievements go through SDK; no local unlock hacks.** Any achievement earnable client-side without server verification is a bug.

## Model routing

Use **Opus 4.7** for changes to `Source/Mahlanya/Simulation/`, `zig/erosion/`, or `python/historical_accuracy/`. Sonnet 4.6 for UI, dialogue, quest scripting, and asset pipeline work. Haiku for renaming, refactors, and boilerplate.

## What "done" looks like

- `python3 scripts/pipeline_full.py` → all 316 tests pass.
- `python3 scripts/historical_accuracy_check.py` → 0 anachronisms.
- `python3 scripts/performance_regression.py --tolerance 0.15` → all tiers within bounds.
- New content has a lore reference in `docs/lore/` OR a Copernicus/OSM ground-truth citation.
- No `TODO(cultural-review)` in shipped code.

## What "wrong" looks like

- A Zulu ceremonial structure in a Swazi settlement (they are related but distinct cultures; the game is set in what is now Eswatini).
- A crop introduced in the 20th century used in a pre-1900 quest.
- A `catch (...)` block that swallows a simulation exception.
- A Nanite-incompatible mesh added because "it was faster to author."
- A performance CVar change that helps one tier by regressing another beyond 15%.

## UE5 Technical Rules

**Rendering pipeline:**
- Lumen GI is the authoritative indirect lighting path. Do not bake static lighting for any interior. If Lumen quality is unacceptable on UltraLowEnd, tune `r.Lumen.Reflections.Allow` and `r.Lumen.DiffuseIndirect.Allow` CVars per tier — do not switch to baked.
- Virtual Shadow Maps (VSM) only. No cascaded shadow maps for new code. VSM page size budget is set in `Config/DefaultEngine.ini` and must not be raised without a performance regression sign-off.
- TSR (Temporal Super Resolution) over DLSS/FSR for shipped builds — no third-party upscaler plugins without explicit approval.
- World Partition with HLOD enabled. New actors must have a correct HLOD layer assigned; uncategorised actors are CI-blocked.

**Blueprint vs C++ boundary:**
- Gameplay logic that touches simulation data (`UGeomorphComponent`, `UHydrologyState`, `UErosionSolver`) must be C++. No Blueprint calling into the Zig bridge — go through the C++ wrapper layer.
- UI, dialogue trees, quest flow: Blueprints are fine. Do not port Blueprint quest logic to C++ for performance reasons without a profiler trace proving it.
- Never use `KismetSystemLibrary::PrintString` in shipped code. Use `UE_LOG(LogMahlanya, ...)` with appropriate verbosity.

**Asset pipeline:**
- All `.uasset` modifications must pass `UnrealEditor -run=DerivedDataCache` before commit to prevent DDC cache misses in CI.
- Texture streaming pool is capped at 2 GB for the UltraLowEnd tier. Any texture array or atlas exceeding 4096×4096 needs a streaming mip chain.
- Audio: MetaSounds only for new SFX. Legacy SoundCue assets may stay but must not be duplicated.

**C++ hard lines:**
- No `UPROPERTY(EditAnywhere)` on simulation-layer structs — use `EditDefaultsOnly` or omit the specifier.
- All `FRunnable` threads must have `FScopeLock` guards on shared state. No `volatile` as a substitute for proper synchronisation.
- `check()` / `ensure()` assertions are allowed in editor/debug builds. In shipping builds, use `ensureMsgf()` and handle the false case gracefully.

## Reference

Rules lifted from `~/my-projects/personal/second-brain/about-me.md` and the phase-10 production-hardening spec at `docs/spec/phase-10.md`. Cultural protocols come from `docs/cultural-review-protocol.md` — that document is authoritative on ambiguity.
