// Copyright Charles Bartaria. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "HAL/IConsoleManager.h"

// Phase 10 performance CVars.
// NOTE: The five simulation CVars (RuntimeErosion.Enabled, RuntimeVoronoi.Enabled,
// GeometricAudio.RayCount, GeometricAudio.MaxBounces, DrawDistance.Km) are already
// registered in UMahlanyaScalabilitySubsystem::Initialize(). They are NOT declared here.
namespace MahlanyaPerformanceCVars
{
    extern TAutoConsoleVariable<int32>  ForceHardwareTier;
    extern TAutoConsoleVariable<float>  ThrottleEvaluationInterval;
    extern TAutoConsoleVariable<int32>  MaxConsecutiveThrottleFrames;
    extern TAutoConsoleVariable<float>  TrustDecayRate;
    extern TAutoConsoleVariable<float>  MinTrustScore;
    extern TAutoConsoleVariable<int32>  MaxExtrapolationMs;
    extern TAutoConsoleVariable<float>  YearChangeBudgetMs;
    extern TAutoConsoleVariable<int32>  MaxEventsPerTick;
    extern TAutoConsoleVariable<int32>  EnableSimulationTracing;
    extern TAutoConsoleVariable<float>  TraceSampleRate;
    extern TAutoConsoleVariable<int32>  MaxPropertiesPerBatch;
    extern TAutoConsoleVariable<float>  BatchIntervalSeconds;
    extern TAutoConsoleVariable<float>  SimulationBandwidthShare;
    extern TAutoConsoleVariable<int32>  ShowTrustMatrixOverlay;
    extern TAutoConsoleVariable<int32>  ShowThrottleState;
}
