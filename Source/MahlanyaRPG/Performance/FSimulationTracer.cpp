// Copyright Charles Bartaria. All Rights Reserved.

#include "FSimulationTracer.h"
#include "Core/MahlanyaLogChannels.h"
#include "HAL/PlatformTime.h"

// ── Stats definitions ─────────────────────────────────────────────────────────
DEFINE_STAT(STAT_Ecology_LV);
DEFINE_STAT(STAT_History_Events);
DEFINE_STAT(STAT_Cultural_Protocol);
DEFINE_STAT(STAT_Economy_Monthly);
DEFINE_STAT(STAT_YearChange_Orchestrate);

#if MAHLANYA_ENABLE_INSIGHTS
#include "Trace/Trace.inl"

UE_TRACE_CHANNEL_DEFINE(SimulationChannel);

UE_TRACE_EVENT_BEGIN(SimulationChannel, SimPhaseEvent)
    UE_TRACE_EVENT_FIELD(uint32, PhaseId)
    UE_TRACE_EVENT_FIELD(uint32, Year)
    UE_TRACE_EVENT_FIELD(float,  DurationMs)
    UE_TRACE_EVENT_FIELD(uint32, ErrorCount)
UE_TRACE_EVENT_END()

UE_TRACE_EVENT_BEGIN(SimulationChannel, TrustEvent)
    UE_TRACE_EVENT_FIELD(uint32, bDegraded)
    UE_TRACE_EVENT_FIELD(float,  OldScore)
    UE_TRACE_EVENT_FIELD(float,  NewScore)
    UE_TRACE_EVENT_FIELD(uint32, PropertyCount)
UE_TRACE_EVENT_END()

UE_TRACE_EVENT_BEGIN(SimulationChannel, ThrottleEvent)
    UE_TRACE_EVENT_FIELD(uint32, OldState)
    UE_TRACE_EVENT_FIELD(uint32, NewState)
    UE_TRACE_EVENT_FIELD(float,  Headroom)
UE_TRACE_EVENT_END()
#endif // MAHLANYA_ENABLE_INSIGHTS

// ── FScopedPhase ──────────────────────────────────────────────────────────────

FSimulationTracer::FScopedPhase::FScopedPhase(ESimPhase::Type InPhase, uint32 InYear)
    : Phase(InPhase), Year(InYear), StartTime(FPlatformTime::Seconds())
{
}

FSimulationTracer::FScopedPhase::~FScopedPhase()
{
    const float Ms = static_cast<float>((FPlatformTime::Seconds() - StartTime) * 1000.0);
    FSimulationTracer::TracePhaseComplete(Phase, Year, Ms);
}

// ── TracePhaseComplete ────────────────────────────────────────────────────────

void FSimulationTracer::TracePhaseComplete(
    ESimPhase::Type Phase, uint32 Year, float DurationMs, uint32 ErrorCount)
{
#if MAHLANYA_ENABLE_INSIGHTS
    UE_TRACE_LOG(SimulationChannel, SimPhaseEvent, SimulationChannel)
        << SimPhaseEvent.PhaseId(static_cast<uint32>(Phase))
        << SimPhaseEvent.Year(Year)
        << SimPhaseEvent.DurationMs(DurationMs)
        << SimPhaseEvent.ErrorCount(ErrorCount);
#endif

    UE_LOG(LogMahlanyaTracing, Verbose,
           TEXT("SimPhase[%u] year=%u %.2fms errors=%u"),
           (uint32)Phase, Year, DurationMs, ErrorCount);
}

// ── TraceTrustEvent ───────────────────────────────────────────────────────────

void FSimulationTracer::TraceTrustEvent(
    bool bDegraded, float OldScore, float NewScore, int32 PropertyCount)
{
#if MAHLANYA_ENABLE_INSIGHTS
    UE_TRACE_LOG(SimulationChannel, TrustEvent, SimulationChannel)
        << TrustEvent.bDegraded(bDegraded ? 1u : 0u)
        << TrustEvent.OldScore(OldScore)
        << TrustEvent.NewScore(NewScore)
        << TrustEvent.PropertyCount(static_cast<uint32>(PropertyCount));
#endif

    UE_LOG(LogMahlanyaTrust, Verbose,
           TEXT("TrustEvent: %s %.2f→%.2f (%d props)"),
           bDegraded ? TEXT("DEGRADED") : TEXT("RESTORED"),
           OldScore, NewScore, PropertyCount);
}

// ── TraceThrottleEvent ────────────────────────────────────────────────────────

void FSimulationTracer::TraceThrottleEvent(
    int32 OldState, int32 NewState, float Headroom)
{
#if MAHLANYA_ENABLE_INSIGHTS
    UE_TRACE_LOG(SimulationChannel, ThrottleEvent, SimulationChannel)
        << ThrottleEvent.OldState(static_cast<uint32>(OldState))
        << ThrottleEvent.NewState(static_cast<uint32>(NewState))
        << ThrottleEvent.Headroom(Headroom);
#endif

    UE_LOG(LogMahlanyaThrottle, Log,
           TEXT("ThrottleEvent: state %d→%d headroom=%.0f%%"),
           OldState, NewState, Headroom * 100.f);
}
