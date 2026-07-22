// Copyright Charles Bartaria / BRT Inc. All Rights Reserved.

#include "AProximityPark.h"

AProximityPark::AProximityPark()
    : LatitudeDeg(0.f)
    , LongitudeDeg(0.f)
    , SiteName(TEXT(""))
    , bFogNetDeployed(false)
{
    PrimaryActorTick.bCanEverTick = false;
}

void AProximityPark::BeginPlay()
{
    Super::BeginPlay();
    UE_LOG(LogTemp, Log, TEXT("AProximityPark '%s' spawned at %s"), *SiteName, *GetGPSString());
}

FString AProximityPark::GetGPSString() const
{
    return FString::Printf(TEXT("lat=%f lon=%f"), LatitudeDeg, LongitudeDeg);
}
