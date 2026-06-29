// Copyright Charles Bartaria. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"
#include "Stats/Stats.h"

#if MAHLANYA_ENABLE_INSIGHTS
#include "Trace/Trace.h"
UE_TRACE_CHANNEL_EXTERN(SimulationChannel);
#endif

// ── Phase identifiers ─────────────────────────────────────────────────────────
namespace ESimPhase
{
    enum Type : uint32
    {
        Ecology_LV          = 0,
        History_Events      = 1,
        Cultural_Protocol   = 2,
        Economy_Monthly     = 3,
        YearChange_Orchestrate = 4,
        Trust_Degraded      = 5,
        Trust_Restored      = 6,
        Throttle_Changed    = 7,
        Count               = 8
    };
}

// ── Cycle stat group ──────────────────────────────────────────────────────────
DECLARE_STATS_GROUP(TEXT("MahlanyaSim"), STATGROUP_MahlanyaSim, STATCAT_Advanced);

DECLARE_CYCLE_STAT_EXTERN(TEXT("Ecology LV"),          STAT_Ecology_LV,             STATGROUP_MahlanyaSim, MAHLANYARPG_API);
DECLARE_CYCLE_STAT_EXTERN(TEXT("History Events"),      STAT_History_Events,          STATGROUP_MahlanyaSim, MAHLANYARPG_API);
DECLARE_CYCLE_STAT_EXTERN(TEXT("Cultural Protocol"),   STAT_Cultural_Protocol,       STATGROUP_MahlanyaSim, MAHLANYARPG_API);
DECLARE_CYCLE_STAT_EXTERN(TEXT("Economy Monthly"),     STAT_Economy_Monthly,         STATGROUP_MahlanyaSim, MAHLANYARPG_API);
DECLARE_CYCLE_STAT_EXTERN(TEXT("YearChange Orch"),     STAT_YearChange_Orchestrate,  STATGROUP_MahlanyaSim, MAHLANYARPG_API);

// ── FSimulationTracer ─────────────────────────────────────────────────────────
class MAHLANYARPG_API FSimulationTracer
{
public:
    // RAII scoped tracer — use via SCOPED_SIM_PHASE macro
    struct FScopedPhase
    {
        FScopedPhase(ESimPhase::Type InPhase, uint32 InYear);
        ~FScopedPhase();
    private:
        ESimPhase::Type Phase;
        uint32          Year;
        double          StartTime;
    };

    static void TracePhaseComplete(ESimPhase::Type Phase, uint32 Year,
                                   float DurationMs, uint32 ErrorCount = 0);

    // Emit a trust score change event (degraded or restored)
    static void TraceTrustEvent(bool bDegraded, float OldScore,
                                float NewScore, int32 PropertyCount);

    // Emit a throttle state transition event
    static void TraceThrottleEvent(int32 OldState, int32 NewState, float Headroom);
};

// ── Macro ─────────────────────────────────────────────────────────────────────
#define SCOPED_SIM_PHASE(Phase, Year) \
    FSimulationTracer::FScopedPhase PREPROCESSOR_JOIN(_SimPhase_, __LINE__)(Phase, Year)
