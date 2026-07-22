// Copyright Charles Bartaria / BRT Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AMineralVeinActor.generated.h"

class USplineComponent;

/** Mineral types encountered in Eswatini geology. */
UENUM(BlueprintType)
enum class EMineralType : uint8
{
    Granite     UMETA(DisplayName = "Granite"),
    Dolerite    UMETA(DisplayName = "Dolerite"),
    Magnetite   UMETA(DisplayName = "Magnetite")
};

/**
 * Procedural mineral vein represented as a spline. Geometry is generated
 * from a seed so that world-generation is deterministic.
 */
UCLASS(BlueprintType)
class ESWATINIGEOPHYSICS_API AMineralVeinActor : public AActor
{
    GENERATED_BODY()

public:
    AMineralVeinActor();

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mahlanya|Geophysics")
    EMineralType MineralType;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mahlanya|Geophysics")
    float VeinLengthM;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mahlanya|Geophysics")
    float VeinThicknessM;

    /** Generates the spline geometry deterministically from Seed. */
    UFUNCTION(BlueprintCallable, Category = "Mahlanya|Geophysics")
    void GenerateVein(int32 Seed);

    /** Human-readable label matching MineralType. */
    UFUNCTION(BlueprintCallable, Category = "Mahlanya|Geophysics")
    FString GetMineralLabel() const;

protected:
    virtual void BeginPlay() override;

private:
    UPROPERTY()
    USplineComponent* VeinSpline;

    static constexpr float MIN_VEIN_LENGTH_M = 10.f;
    static constexpr float MAX_VEIN_LENGTH_M = 5000.f;
    static constexpr float DEFAULT_VEIN_LENGTH_M = 100.f;
    static constexpr float DEFAULT_VEIN_THICKNESS_M = 2.f;
    static constexpr int32 MIN_SPLINE_POINTS = 3;
    static constexpr int32 MAX_SPLINE_POINTS = 7;
    static constexpr int32 DEFAULT_SEED = 0;
    static constexpr float METRES_TO_CENTIMETRES = 100.f;
};
