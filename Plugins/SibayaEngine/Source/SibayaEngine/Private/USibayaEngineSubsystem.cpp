#include "USibayaEngineSubsystem.h"
#include "VoronoiSolver.h"

// ---------------------------------------------------------------------------
// UWorldSubsystem overrides
// ---------------------------------------------------------------------------

void USibayaEngineSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    UE_LOG(LogTemp, Log, TEXT("USibayaEngineSubsystem initialized"));
}

void USibayaEngineSubsystem::Deinitialize()
{
    Super::Deinitialize();
}

// ---------------------------------------------------------------------------
// Settlement management
// ---------------------------------------------------------------------------

void USibayaEngineSubsystem::RegisterSettlement(FSettlementID SettlementID,
                                                  int32 NumWives, int32 NumSons,
                                                  int32 NumDependents, int32 CattleCount,
                                                  float TerrainGradient)
{
    FSettlementData Data;
    Data.NumWives        = NumWives;
    Data.NumSons         = NumSons;
    Data.NumDependents   = NumDependents;
    Data.CattleCount     = CattleCount;
    Data.TerrainGradient = TerrainGradient;

    Settlements.Add(SettlementID, Data);
    RecomputeLayout(SettlementID);
}

// ---------------------------------------------------------------------------
// Demographic event handlers
// ---------------------------------------------------------------------------

void USibayaEngineSubsystem::OnWifeMarried(FSettlementID SettlementID, FWifeData NewWife)
{
    FSettlementData* Data = Settlements.Find(SettlementID);
    if (!Data)
    {
        UE_LOG(LogTemp, Warning, TEXT("USibayaEngineSubsystem::OnWifeMarried — settlement %s not found"),
               *SettlementID.ID.ToString());
        return;
    }

    Data->NumWives += 1;
    RecomputeLayout(SettlementID);
    PublishDemographicChange(SettlementID);
}

void USibayaEngineSubsystem::OnHutDestroyed(FSettlementID SettlementID, FHutID HutID)
{
    FSettlementData* Data = Settlements.Find(SettlementID);
    if (!Data)
    {
        UE_LOG(LogTemp, Warning, TEXT("USibayaEngineSubsystem::OnHutDestroyed — settlement %s not found"),
               *SettlementID.ID.ToString());
        return;
    }

    Data->DestroyedHuts.Add(HutID);
    RecomputeLayout(SettlementID);
    PublishDemographicChange(SettlementID);
}

void USibayaEngineSubsystem::OnCattleRaid(FSettlementID SettlementID, int32 CattleLost)
{
    FSettlementData* Data = Settlements.Find(SettlementID);
    if (!Data)
    {
        UE_LOG(LogTemp, Warning, TEXT("USibayaEngineSubsystem::OnCattleRaid — settlement %s not found"),
               *SettlementID.ID.ToString());
        return;
    }

    Data->CattleCount = FMath::Max(0, Data->CattleCount - CattleLost);
    RecomputeLayout(SettlementID);
    PublishDemographicChange(SettlementID);
}

void USibayaEngineSubsystem::OnFamilyMerge(FSettlementID SettlementA, FSettlementID SettlementB)
{
    FSettlementData* DataA = Settlements.Find(SettlementA);
    FSettlementData* DataB = Settlements.Find(SettlementB);

    if (!DataA)
    {
        UE_LOG(LogTemp, Warning, TEXT("USibayaEngineSubsystem::OnFamilyMerge — settlement %s not found"),
               *SettlementA.ID.ToString());
        return;
    }
    if (!DataB)
    {
        UE_LOG(LogTemp, Warning, TEXT("USibayaEngineSubsystem::OnFamilyMerge — settlement %s not found"),
               *SettlementB.ID.ToString());
        return;
    }

    // Merge B into A
    DataA->NumWives      += DataB->NumWives;
    DataA->NumSons       += DataB->NumSons;
    DataA->NumDependents += DataB->NumDependents;
    DataA->CattleCount   += DataB->CattleCount;
    DataA->DestroyedHuts.Append(DataB->DestroyedHuts);

    // Remove settlement B
    Settlements.Remove(SettlementB);

    RecomputeLayout(SettlementA);
    PublishDemographicChange(SettlementA);
}

// ---------------------------------------------------------------------------
// Query
// ---------------------------------------------------------------------------

FVoronoiLayout USibayaEngineSubsystem::GetLayout(FSettlementID SettlementID) const
{
    const FSettlementData* Data = Settlements.Find(SettlementID);
    if (Data)
    {
        return Data->Layout;
    }

    // Return empty layout if settlement not found
    return FVoronoiLayout{};
}

// ---------------------------------------------------------------------------
// Private helpers
// ---------------------------------------------------------------------------

void USibayaEngineSubsystem::RecomputeLayout(FSettlementID SettlementID)
{
    FSettlementData* Data = Settlements.Find(SettlementID);
    if (!Data)
    {
        return;
    }

    Data->Layout = FVoronoiSolver::ComputeLayout(
        Data->NumWives,
        Data->NumSons,
        Data->NumDependents,
        Data->CattleCount,
        Data->TerrainGradient);
}

void USibayaEngineSubsystem::PublishDemographicChange(FSettlementID SettlementID)
{
    // Full SimulationBus integration requires the bus header which may not be
    // available at this stage; use a simple log for now.
    UE_LOG(LogTemp, Verbose, TEXT("Settlement %s demographic changed"),
           *SettlementID.ID.ToString());
}
