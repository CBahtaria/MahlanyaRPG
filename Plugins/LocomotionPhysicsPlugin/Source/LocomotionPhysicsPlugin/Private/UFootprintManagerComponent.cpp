#include "UFootprintManagerComponent.h"

// ── FTerrainHardnessTable ─────────────────────────────────────────────────────

float FTerrainHardnessTable::Lookup(FName MaterialName)
{
    if (MaterialName == FName("PM_GraniteDry") || MaterialName == FName("PM_GraniteWet"))
    {
        return 5000000.f;
    }
    if (MaterialName == FName("PM_ClayDry"))
    {
        return 500000.f;
    }
    if (MaterialName == FName("PM_ClayWet"))
    {
        return 50000.f;
    }
    if (MaterialName == FName("PM_GrasslandDry"))
    {
        return 200000.f;
    }
    if (MaterialName == FName("PM_BurnedGrass"))
    {
        return 150000.f;
    }
    if (MaterialName == FName("PM_RiverSand"))
    {
        return 100000.f;
    }
    if (MaterialName == FName("PM_ShaleDry") || MaterialName == FName("PM_ShaleWet"))
    {
        return 1500000.f;
    }

    // Default hardness for unknown materials
    return 200000.f;
}

// ── UFootprintManagerComponent ────────────────────────────────────────────────

UFootprintManagerComponent::UFootprintManagerComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

float UFootprintManagerComponent::ComputeDepth(float Mass_kg, float Area_m2, float Hardness_Pa)
{
    return Mass_kg / (Area_m2 * Hardness_Pa);
}

void UFootprintManagerComponent::StampFootprint(FVector WorldLocation, FName MaterialName,
                                                 float CharacterMass_kg,
                                                 float FootContactArea_m2)
{
    const float Hardness = FTerrainHardnessTable::Lookup(MaterialName);
    float Depth = ComputeDepth(CharacterMass_kg, FootContactArea_m2, Hardness);

    // Clamp to [0, 0.15] metres — no print deeper than 15 cm
    Depth = FMath::Clamp(Depth, 0.f, 0.15f);

    FFootprint NewPrint;
    NewPrint.WorldLocation = WorldLocation;
    NewPrint.DepthMetres   = Depth;
    NewPrint.AgeSeconds    = 0.f;
    NewPrint.bBaked        = false;
    NewPrint.HardnessPa    = Hardness;

    ActiveFootprints.Add(NewPrint);

    // Discard oldest if over limit
    if (ActiveFootprints.Num() > MaxFootprints)
    {
        ActiveFootprints.RemoveAt(0);
    }

    UE_LOG(LogTemp, Log, TEXT("UFootprintManagerComponent: Stamped footprint — material=%s depth=%.2f cm"),
           *MaterialName.ToString(), Depth * 100.f);
}

void UFootprintManagerComponent::AgePrints(float DeltaTime, float RainIntensity_mmhr, float SunIntensity)
{
    // Iterate in reverse so we can safely remove elements
    for (int32 i = ActiveFootprints.Num() - 1; i >= 0; --i)
    {
        FFootprint& Print = ActiveFootprints[i];

        Print.AgeSeconds += DeltaTime;

        if (!Print.bBaked && RainIntensity_mmhr > 0.f)
        {
            // Rain fills the print upward
            Print.DepthMetres -= RainFillRate * RainIntensity_mmhr * DeltaTime;
            Print.DepthMetres  = FMath::Max(0.f, Print.DepthMetres);
        }

        if (RainIntensity_mmhr == 0.f && SunIntensity > 0.3f && Print.AgeSeconds > SunBakeThreshold_s)
        {
            // Sun has hardened the clay — depth is now locked
            Print.bBaked = true;
        }

        // Erase prints that have been completely filled by rain (not baked)
        if (Print.DepthMetres <= 0.001f && !Print.bBaked)
        {
            ActiveFootprints.RemoveAt(i);
        }
    }
}

float UFootprintManagerComponent::GetMostRecentPrintFreshness() const
{
    if (ActiveFootprints.Num() == 0)
    {
        return 0.f;
    }

    const FFootprint& LastPrint = ActiveFootprints.Last();

    // Approximate freshness using 0.06m as the average wet-clay print depth for a 75 kg character:
    // 75 / (0.025 * 50000) = 0.06 m
    const float ReferencDepth = 0.06f;
    return FMath::Clamp(LastPrint.DepthMetres / ReferencDepth, 0.f, 1.f);
}
