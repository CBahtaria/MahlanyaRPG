#include "FTerrainFrictionTensor.h"

const TMap<FName, FTerrainFrictionTensor>& FTerrainFrictionRegistry::Get()
{
    static TMap<FName, FTerrainFrictionTensor> Registry = []()
    {
        TMap<FName, FTerrainFrictionTensor> Map;

        // StaticDry  StaticWet  DynamicDry  DynamicWet  Axis   Ratio
        {
            FTerrainFrictionTensor T;
            T.StaticDry = 0.65f; T.StaticWet = 0.38f;
            T.DynamicDry = 0.55f; T.DynamicWet = 0.28f;
            T.AnisotropyAxis = 0.f; T.AnisotropyRatio = 1.0f;
            Map.Add(TEXT("PM_GraniteWet"), T);
        }
        {
            FTerrainFrictionTensor T;
            T.StaticDry = 0.82f; T.StaticWet = 0.82f;
            T.DynamicDry = 0.72f; T.DynamicWet = 0.72f;
            T.AnisotropyAxis = 0.f; T.AnisotropyRatio = 1.0f;
            Map.Add(TEXT("PM_GraniteDry"), T);
        }
        {
            FTerrainFrictionTensor T;
            T.StaticDry = 0.18f; T.StaticWet = 0.06f;
            T.DynamicDry = 0.12f; T.DynamicWet = 0.04f;
            T.AnisotropyAxis = 0.f; T.AnisotropyRatio = 1.0f;
            Map.Add(TEXT("PM_ClayWet"), T);
        }
        {
            FTerrainFrictionTensor T;
            T.StaticDry = 0.55f; T.StaticWet = 0.55f;
            T.DynamicDry = 0.42f; T.DynamicWet = 0.42f;
            T.AnisotropyAxis = 0.f; T.AnisotropyRatio = 1.0f;
            Map.Add(TEXT("PM_ClayDry"), T);
        }
        {
            FTerrainFrictionTensor T;
            T.StaticDry = 0.45f; T.StaticWet = 0.22f;
            T.DynamicDry = 0.35f; T.DynamicWet = 0.14f;
            T.AnisotropyAxis = 235.f; T.AnisotropyRatio = 3.2f;
            Map.Add(TEXT("PM_ShaleWet"), T);
        }
        {
            FTerrainFrictionTensor T;
            T.StaticDry = 0.60f; T.StaticWet = 0.60f;
            T.DynamicDry = 0.48f; T.DynamicWet = 0.48f;
            T.AnisotropyAxis = 0.f; T.AnisotropyRatio = 1.0f;
            Map.Add(TEXT("PM_GrasslandDry"), T);
        }
        {
            FTerrainFrictionTensor T;
            T.StaticDry = 0.12f; T.StaticWet = 0.08f;
            T.DynamicDry = 0.09f; T.DynamicWet = 0.06f;
            T.AnisotropyAxis = 0.f; T.AnisotropyRatio = 1.0f;
            Map.Add(TEXT("PM_BurnedGrass"), T);
        }
        {
            FTerrainFrictionTensor T;
            T.StaticDry = 0.38f; T.StaticWet = 0.28f;
            T.DynamicDry = 0.30f; T.DynamicWet = 0.20f;
            T.AnisotropyAxis = 0.f; T.AnisotropyRatio = 1.0f;
            Map.Add(TEXT("PM_RiverSand"), T);
        }

        return Map;
    }();

    return Registry;
}

FTerrainFrictionTensor FTerrainFrictionRegistry::Lookup(FName MaterialName)
{
    const TMap<FName, FTerrainFrictionTensor>& Registry = Get();
    if (const FTerrainFrictionTensor* Found = Registry.Find(MaterialName))
    {
        return *Found;
    }

    // Default fallback for unknown materials
    FTerrainFrictionTensor Default;
    Default.StaticDry    = 0.60f;
    Default.StaticWet    = 0.40f;
    Default.DynamicDry   = 0.50f;
    Default.DynamicWet   = 0.30f;
    Default.AnisotropyAxis  = 0.f;
    Default.AnisotropyRatio = 1.0f;
    return Default;
}
