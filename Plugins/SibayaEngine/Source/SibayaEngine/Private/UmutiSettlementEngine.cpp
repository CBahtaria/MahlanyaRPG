#include "UmutiSettlementEngine.h"

AUmutiSettlementEngine::AUmutiSettlementEngine()
{
    PrimaryActorTick.bCanEverTick = false;
}

bool AUmutiSettlementEngine::IsInTabooArc(float BearingDeg)
{
    return BearingDeg >= TABOO_ARC_START_DEG && BearingDeg <= TABOO_ARC_END_DEG;
}

TArray<FSwaziHutNode> AUmutiSettlementEngine::GenerateHomesteadMatrix(
    FVector SettlementOrigin,
    float   Radius,
    int32   TotalWives)
{
    CachedHutNodes.Empty();

    // 1. Sibaya — cattle kraal at the geometric origin
    {
        FSwaziHutNode Sibaya;
        Sibaya.HutType             = ESwaziHutType::Sibaya;
        Sibaya.WorldLocation       = SettlementOrigin;
        Sibaya.WorldRotation       = FRotator::ZeroRotator;
        Sibaya.HierarchicalWeight  = 1.0f;
        Sibaya.SeniorityRank       = 0;
        CachedHutNodes.Add(Sibaya);
    }

    // 2. IndluLeyinkhulu (queen mother / chief's apex house)
    //    Historically placed due West of Sibaya, facing East toward the sunrise gate.
    {
        FVector ApexOffset = FVector(-Radius, 0.0f, 0.0f);  // UE5: X = North, -X = South...
        // Note: adjust sign for actual UE5 coordinate convention in world setup.
        // West in a North-up world = -Y in UE5 landscape coords; kept as -X for
        // a settlement that aligns its "East" with +X world axis.
        FSwaziHutNode Apex;
        Apex.HutType            = ESwaziHutType::IndluLeyinkhulu;
        Apex.WorldLocation      = SettlementOrigin + ApexOffset;
        Apex.WorldRotation      = FRotator(0.0f, 0.0f, 0.0f);  // Faces +X (East / Sibaya gate)
        Apex.HierarchicalWeight = 0.95f;
        Apex.SeniorityRank      = 1;
        CachedHutNodes.Add(Apex);
    }

    // 3. Wives' huts — alternating arc tessellation with seniority distance decay
    //    d_i = Radius * 0.8 / rank^0.6 (senior wives closer to Sibaya)
    //    Arc spread: 160° arc centred on South, split left/right alternately.
    //    Taboo arc (240°–300°) is excluded.
    float ArcSpacingAngle = TotalWives > 0 ? 160.0f / FMath::Max(1, TotalWives) : 0.0f;

    for (int32 i = 0; i < TotalWives; ++i)
    {
        int32 Rank      = i + 1;
        float DistScale = FMath::Pow(static_cast<float>(Rank), -0.6f);
        float HutRadius = Radius * 0.8f * DistScale;

        // Alternate left/right of the South axis (180°)
        float BearingDeg = 180.0f + ((i % 2 == 0 ? 1.0f : -1.0f) * ((i / 2) + 1) * ArcSpacingAngle);
        BearingDeg = FMath::Fmod(BearingDeg + 360.0f, 360.0f);

        // Skip if position falls in the luhlanga taboo arc
        if (IsInTabooArc(BearingDeg))
        {
            BearingDeg = TABOO_ARC_END_DEG + 5.0f;  // Bump just outside the arc
        }

        float Rad = FMath::DegreesToRadians(BearingDeg);
        FVector HutPos = SettlementOrigin + FVector(
            FMath::Cos(Rad) * HutRadius,
            FMath::Sin(Rad) * HutRadius,
            0.0f);

        FSwaziHutNode WifeHut;
        WifeHut.HutType            = ESwaziHutType::Guma;
        WifeHut.WorldLocation      = HutPos;
        WifeHut.WorldRotation      = (SettlementOrigin - HutPos).Rotation();  // Face inward
        WifeHut.HierarchicalWeight = FMath::Max(0.1f, 0.75f - (i * 0.02f));
        WifeHut.SeniorityRank      = Rank + 1;
        CachedHutNodes.Add(WifeHut);
    }

    // 4. Lilawu — bachelor quarters at the East gate for village defence
    {
        FVector GateOffset = FVector(Radius * 0.9f, 0.0f, 0.0f);  // Due East
        FSwaziHutNode Lilawu;
        Lilawu.HutType            = ESwaziHutType::Lilawu;
        Lilawu.WorldLocation      = SettlementOrigin + GateOffset;
        Lilawu.WorldRotation      = (SettlementOrigin - Lilawu.WorldLocation).Rotation();
        Lilawu.HierarchicalWeight = 0.40f;
        Lilawu.SeniorityRank      = TotalWives + 2;
        CachedHutNodes.Add(Lilawu);
    }

    // Sort descending by HierarchicalWeight
    CachedHutNodes.Sort([](const FSwaziHutNode& A, const FSwaziHutNode& B)
    {
        return A.HierarchicalWeight > B.HierarchicalWeight;
    });

    return CachedHutNodes;
}

void AUmutiSettlementEngine::RunLloydRelaxation(int32 Iterations)
{
    // Lloyd's algorithm: iteratively move each hut to the centroid of its
    // Voronoi cell neighbours, clamped so it cannot enter the taboo arc.
    // Full implementation in Plan 4 — SibayaEngine VoronoiSolver.cpp.
    // This stub ensures the Blueprint interface is compile-valid.
    UE_LOG(LogTemp, Log, TEXT("UmutiSettlementEngine: Lloyd relaxation (%d iterations) — Plan 4"), Iterations);
}
