#include "UStealthManagerComponent.h"
#include "UMicroclimateSubsystem.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"

UStealthManagerComponent::UStealthManagerComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
}

void UStealthManagerComponent::TickComponent(float DeltaTime, ELevelTick TickType,
                                              FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    if (!CachedSubsystem && GetWorld())
    {
        CachedSubsystem = GetWorld()->GetSubsystem<UMicroclimateSubsystem>();
    }

    // Lazy update: only recalculate when player moves > 50cm
    if (!GetWorld()) return;
    APlayerController* PC = GetWorld()->GetFirstPlayerController();
    if (!PC || !PC->GetPawn()) return;

    const FVector PlayerLoc = PC->GetPawn()->GetActorLocation();
    if (FVector::Dist(PlayerLoc, LastUpdateLocation) < UPDATE_DISTANCE_CM) return;

    LastUpdateLocation = PlayerLoc;
    CachedDetectionRange = ComputeDetectionRange();
}

float UStealthManagerComponent::ComputeDetectionRange() const
{
    FVector PlayerLoc = FVector::ZeroVector;
    if (GetWorld())
    {
        if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
        {
            if (APawn* Pawn = PC->GetPawn())
                PlayerLoc = Pawn->GetActorLocation();
        }
    }

    float FogDensity = 0.f;
    float SmokeDensity = 0.f;
    float RainReduction = 0.f;

    if (CachedSubsystem)
    {
        FogDensity = CachedSubsystem->GetVolumetricDensityAt(PlayerLoc);
        SmokeDensity = CachedSubsystem->GetSmokeDensityAt(PlayerLoc);
        RainReduction = FMath::Clamp(
            CachedSubsystem->GetCurrentState().PrecipitationIntensity / 50.f,
            0.f, 0.3f);
    }

    const float NightMult = IsNightTime() ? NightDetectionMultiplier : 1.0f;
    const float VisibilityMultiplier = FMath::Clamp(
        1.0f - FogDensity * 0.8f - SmokeDensity * 0.6f - RainReduction,
        0.05f, 1.0f);

    return BaseDetectionRange * VisibilityMultiplier * NightMult;
}

bool UStealthManagerComponent::IsNightTime() const
{
    if (!GetWorld()) return false;
    const float DaySeconds = FMath::Fmod(GetWorld()->GetTimeSeconds(), 86400.f);
    const float Hour = DaySeconds / 3600.f;
    return Hour >= 20.f || Hour < 6.f;
}
