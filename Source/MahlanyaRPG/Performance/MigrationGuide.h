// Copyright Charles Bartaria. All Rights Reserved.
// MigrationGuide.h — Phase 10 Migration Notes
//
// This header documents required call site changes for Phase 10.
// It is documentation only — do not include this file in .cpp files.
//
// ──────────────────────────────────────────────────────────────────────────────
// Year Change Routing
// ──────────────────────────────────────────────────────────────────────────────
//
// BEFORE (Phase 9):
//   Calendar->AdvanceTime(Delta)            // fired all BFS events synchronously
//
// AFTER (Phase 10):
//   Orch->StartYearChange(CurrentYear + Delta)   // spreads events across ticks
//
// AMahlanyaGameState::ServerAdvanceYear() has been updated to route through
// UYearChangeOrchestrator. No other call sites should call Calendar->AdvanceTime
// directly for year-change purposes.
//
// ──────────────────────────────────────────────────────────────────────────────
// Scalability CVar Writes
// ──────────────────────────────────────────────────────────────────────────────
//
// UMahlanyaScalabilitySubsystem is the CVar *reader*.
// UHardwareAdaptiveScaler is the CVar *writer* (sets them at Initialize).
// Do not call IConsoleManager::Get().FindConsoleVariable(...)->Set(...)
// directly from subsystems — route through UHardwareAdaptiveScaler::DynamicApplyConfig.
//
// ──────────────────────────────────────────────────────────────────────────────
// OnConfigUpdated Delegate
// ──────────────────────────────────────────────────────────────────────────────
//
// UMahlanyaScalabilitySubsystem should subscribe to
// UHardwareAdaptiveScaler::OnConfigUpdated so it re-reads CVars after
// DynamicApplyConfig is called:
//
//   Scaler->OnConfigUpdated.AddUObject(this, &UMahlanyaScalabilitySubsystem::RefreshFromCVars);
//
// ──────────────────────────────────────────────────────────────────────────────
