// Copyright Charles Bartaria / BRT Inc. All Rights Reserved.

#include "AMineralVeinActor.h"

#include "Components/SplineComponent.h"
#include "Math/RandomStream.h"

AMineralVeinActor::AMineralVeinActor()
    : MineralType(EMineralType::Granite)
    , VeinLengthM(DEFAULT_VEIN_LENGTH_M)
    , VeinThicknessM(DEFAULT_VEIN_THICKNESS_M)
    , VeinSpline(nullptr)
{
    PrimaryActorTick.bCanEverTick = false;
    VeinSpline = CreateDefaultSubobject<USplineComponent>(TEXT("VeinSpline"));
    RootComponent = VeinSpline;
}

void AMineralVeinActor::BeginPlay()
{
    Super::BeginPlay();
    GenerateVein(DEFAULT_SEED);
}

void AMineralVeinActor::GenerateVein(int32 Seed)
{
    if (VeinSpline == nullptr)
    {
        return;
    }

    const float ClampedLengthM = FMath::Clamp(VeinLengthM, MIN_VEIN_LENGTH_M, MAX_VEIN_LENGTH_M);
    FRandomStream Rng(Seed);

    const int32 PointCount = Rng.RandRange(MIN_SPLINE_POINTS, MAX_SPLINE_POINTS);
    const float SegmentLengthCm = (ClampedLengthM * METRES_TO_CENTIMETRES) / static_cast<float>(PointCount - 1);

    VeinSpline->ClearSplinePoints(false);
    for (int32 Index = 0; Index < PointCount; ++Index)
    {
        const float X = SegmentLengthCm * static_cast<float>(Index);
        const float Y = Rng.FRandRange(-SegmentLengthCm * 0.5f, SegmentLengthCm * 0.5f);
        const float Z = Rng.FRandRange(-SegmentLengthCm * 0.25f, SegmentLengthCm * 0.25f);
        VeinSpline->AddSplinePoint(FVector(X, Y, Z), ESplineCoordinateSpace::Local, false);
    }
    VeinSpline->UpdateSpline();
}

FString AMineralVeinActor::GetMineralLabel() const
{
    switch (MineralType)
    {
        case EMineralType::Granite:   return TEXT("Granite");
        case EMineralType::Dolerite:  return TEXT("Dolerite");
        case EMineralType::Magnetite: return TEXT("Magnetite");
    }
    return TEXT("Unknown");
}
