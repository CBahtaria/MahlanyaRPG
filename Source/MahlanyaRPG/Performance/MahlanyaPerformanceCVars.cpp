// Copyright Charles Bartaria. All Rights Reserved.

#include "MahlanyaPerformanceCVars.h"

namespace MahlanyaPerformanceCVars
{
    TAutoConsoleVariable<int32> ForceHardwareTier(
        TEXT("mahlanya.ForceHardwareTier"),
        -1,
        TEXT("Override hardware tier detection: -1=auto, 0=LowEnd, 1=MidRange, 2=HighEnd, 3=Ultra"),
        ECVF_Default);

    TAutoConsoleVariable<float> ThrottleEvaluationInterval(
        TEXT("mahlanya.ThrottleEvaluationInterval"),
        0.5f,
        TEXT("Seconds between dynamic throttle evaluations"),
        ECVF_Default);

    TAutoConsoleVariable<int32> MaxConsecutiveThrottleFrames(
        TEXT("mahlanya.MaxConsecutiveThrottleFrames"),
        300,
        TEXT("Maximum frames the throttle system tracks in history"),
        ECVF_Default);

    TAutoConsoleVariable<float> TrustDecayRate(
        TEXT("mahlanya.TrustDecayRate"),
        0.08f,
        TEXT("Per-tick decay rate applied to simulation property trust scores"),
        ECVF_Default);

    TAutoConsoleVariable<float> MinTrustScore(
        TEXT("mahlanya.MinTrustScore"),
        0.4f,
        TEXT("Trust score below which the replication manager fires OnTrustScoreChanged"),
        ECVF_Default);

    TAutoConsoleVariable<int32> MaxExtrapolationMs(
        TEXT("mahlanya.MaxExtrapolationMs"),
        500,
        TEXT("Maximum milliseconds a simulation property may be extrapolated before going Invalid"),
        ECVF_Default);

    TAutoConsoleVariable<float> YearChangeBudgetMs(
        TEXT("mahlanya.YearChangeBudgetMs"),
        2.0f,
        TEXT("Per-tick time budget in milliseconds for UYearChangeOrchestrator event processing"),
        ECVF_Default);

    TAutoConsoleVariable<int32> MaxEventsPerTick(
        TEXT("mahlanya.MaxEventsPerTick"),
        5,
        TEXT("Maximum historical event nodes processed per orchestrator tick"),
        ECVF_Default);

    TAutoConsoleVariable<int32> EnableSimulationTracing(
        TEXT("mahlanya.EnableSimulationTracing"),
        1,
        TEXT("1=emit Unreal Insights simulation trace events; 0=disabled"),
        ECVF_Default);

    TAutoConsoleVariable<float> TraceSampleRate(
        TEXT("mahlanya.TraceSampleRate"),
        1.0f,
        TEXT("Fraction of simulation frames that emit trace events (1.0=every frame)"),
        ECVF_Default);

    TAutoConsoleVariable<int32> MaxPropertiesPerBatch(
        TEXT("mahlanya.MaxPropertiesPerBatch"),
        50,
        TEXT("Maximum simulation properties sent per Client_ReceivePropertyBatch RPC"),
        ECVF_Default);

    TAutoConsoleVariable<float> BatchIntervalSeconds(
        TEXT("mahlanya.BatchIntervalSeconds"),
        0.1f,
        TEXT("Minimum seconds between Client_ReceivePropertyBatch RPCs from the server"),
        ECVF_Default);

    TAutoConsoleVariable<float> SimulationBandwidthShare(
        TEXT("mahlanya.SimulationBandwidthShare"),
        0.3f,
        TEXT("Fraction of TotalNetBandwidth reserved for simulation sync RPCs"),
        ECVF_Default);

    TAutoConsoleVariable<int32> ShowTrustMatrixOverlay(
        TEXT("mahlanya.ShowTrustMatrixOverlay"),
        0,
        TEXT("[Cheat] 1=show in-HUD trust matrix debug overlay"),
        ECVF_Cheat);

    TAutoConsoleVariable<int32> ShowThrottleState(
        TEXT("mahlanya.ShowThrottleState"),
        0,
        TEXT("[Cheat] 1=show throttle state overlay on screen"),
        ECVF_Cheat);

    TAutoConsoleVariable<int32> ProximityLODEnabled(
        TEXT("mahlanya.ProximityLOD.Enabled"), 1,
        TEXT("Enable view-based LOD tier system (0=off, 1=on)."),
        ECVF_Default);

    TAutoConsoleVariable<float> ProximityLODInterval(
        TEXT("mahlanya.ProximityLOD.EvalIntervalSeconds"), 0.1f,
        TEXT("How often (seconds) to re-evaluate LOD tiers."),
        ECVF_Default);

    TAutoConsoleVariable<float> ProximityLODFocusConeAngle(
        TEXT("mahlanya.ProximityLOD.FocusConeAngle"), 25.f,
        TEXT("Half-angle (degrees) of the Focus tier cone around the camera forward vector."),
        ECVF_Default);

    TAutoConsoleVariable<float> ProximityLODBackgroundDotThreshold(
        TEXT("mahlanya.ProximityLOD.BackgroundDotThreshold"), -0.2f,
        TEXT("Dot product threshold below which objects enter Background tier (force lowest LOD)."),
        ECVF_Default);
}
