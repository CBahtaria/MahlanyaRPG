// Copyright Charles Bartaria / BRT Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "UGroundwaterSubsystem.generated.h"

/**
 * World subsystem simulating groundwater flow on a regular grid using
 * Darcy's law: Q = -K * A * (dh/dl).
 */
UCLASS(BlueprintType)
class ESWATINIGEOPHYSICS_API UGroundwaterSubsystem : public UWorldSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mahlanya|Geophysics")
    int32 GridResolution;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mahlanya|Geophysics")
    float CellSizeM;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mahlanya|Geophysics")
    float HydraulicConductivityMPerDay;

    /** Allocates head/flow grids to GridResolution^2 and zeros them. */
    UFUNCTION(BlueprintCallable, Category = "Mahlanya|Geophysics")
    void InitialiseGrid();

    /** Advances one Darcy step. GameDayDelta is fractional game days elapsed. */
    UFUNCTION(BlueprintCallable, Category = "Mahlanya|Geophysics")
    void StepFlow(float GameDayDelta);

    /** Hydraulic head at (Col, Row). Returns 0 if out of bounds. */
    UFUNCTION(BlueprintCallable, Category = "Mahlanya|Geophysics")
    float GetHead(int32 Col, int32 Row) const;

    /** Local Darcy flow rate in m^3 / day at (Col, Row). */
    UFUNCTION(BlueprintCallable, Category = "Mahlanya|Geophysics")
    float GetFlowRateM3PerDay(int32 Col, int32 Row) const;

private:
    TArray<float> HeadGrid;
    TArray<float> FlowGrid;

    static constexpr float MIN_CONDUCTIVITY = 1e-6f;
    static constexpr float MAX_CONDUCTIVITY = 100.f;
    static constexpr int32 DEFAULT_GRID_RESOLUTION = 50;
    static constexpr float DEFAULT_CELL_SIZE_M = 50.f;
    static constexpr float DEFAULT_CONDUCTIVITY_M_PER_DAY = 1.0f;
    static constexpr float INITIAL_HEAD_M = 0.f;

    int32 IndexOf(int32 Col, int32 Row) const;
    bool InBounds(int32 Col, int32 Row) const;
};
