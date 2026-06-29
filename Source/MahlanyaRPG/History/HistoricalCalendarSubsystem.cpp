#include "History/HistoricalCalendarSubsystem.h"

DEFINE_LOG_CATEGORY_STATIC(LogMahlanya, Log, All);

// ---------------------------------------------------------------------------
// UHistoricalCalendarSubsystem — UWorldSubsystem interface
// ---------------------------------------------------------------------------

void UHistoricalCalendarSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    LoadHistoricalEvents();

    // Initial BFS pass: unlock root events (those with no prerequisites)
    // so they are ready to fire the moment AdvanceYear() is first called.
    BFSUnlock();

    UE_LOG(LogMahlanya, Log,
        TEXT("UHistoricalCalendarSubsystem: Initialised with %d historical events. CurrentYear=%d"),
        EventGraph.Num(), CurrentYear);
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void UHistoricalCalendarSubsystem::AdvanceYear(int32 NewYear)
{
    if (NewYear < CurrentYear)
    {
        UE_LOG(LogMahlanya, Warning,
            TEXT("UHistoricalCalendarSubsystem::AdvanceYear: NewYear (%d) is before CurrentYear (%d). Ignored."),
            NewYear, CurrentYear);
        return;
    }

    // 1. Update the in-game year.
    CurrentYear = NewYear;

    // 2. BFS unlock: propagate unlock status through the dependency graph.
    TArray<FName> NewlyUnlocked = BFSUnlock();
    if (NewlyUnlocked.Num() > 0)
    {
        for (const FName& ID : NewlyUnlocked)
        {
            UE_LOG(LogMahlanya, Log,
                TEXT("UHistoricalCalendarSubsystem: Event '%s' newly unlocked (Year=%d)."),
                *ID.ToString(), CurrentYear);
        }
    }

    // 3. Fire all unlocked, unfired events whose Year <= NewYear.
    FiredThisYear.Reset();

    for (FHistoricalEventNode& Node : EventGraph)
    {
        if (Node.bUnlocked && !Node.bFired && Node.Year <= CurrentYear)
        {
            Node.bFired = true;
            FiredThisYear.Add(Node.EventID);

            UE_LOG(LogMahlanya, Log,
                TEXT("UHistoricalCalendarSubsystem: FIRED event '%s' (Year=%d, NarrativeWeight=%.2f)."),
                *Node.EventID.ToString(), Node.Year, Node.NarrativeWeight);

            OnHistoricalEventFired.Broadcast(Node.EventID, Node.Year);
        }
    }

    if (FiredThisYear.Num() > 0)
    {
        UE_LOG(LogMahlanya, Log,
            TEXT("UHistoricalCalendarSubsystem: %d event(s) fired advancing to Year %d."),
            FiredThisYear.Num(), CurrentYear);
    }
}

bool UHistoricalCalendarSubsystem::HasEventFired(FName EventID) const
{
    for (const FHistoricalEventNode& Node : EventGraph)
    {
        if (Node.EventID == EventID)
        {
            return Node.bFired;
        }
    }
    return false;
}

TArray<FName> UHistoricalCalendarSubsystem::GetEventsFiredThisYear() const
{
    return FiredThisYear;
}

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

TArray<FName> UHistoricalCalendarSubsystem::BFSUnlock()
{
    TArray<FName> NewlyUnlocked;

    // Repeat passes until a complete pass produces no new unlocks.
    // This handles dependency chains of arbitrary depth (e.g. A→B→C→D).
    bool bFoundNew = true;
    while (bFoundNew)
    {
        bFoundNew = false;

        for (FHistoricalEventNode& Node : EventGraph)
        {
            // Skip nodes already fired or already unlocked.
            if (Node.bFired || Node.bUnlocked)
            {
                continue;
            }

            // A node with no prerequisites is unconditionally unlocked.
            if (Node.Prerequisites.Num() == 0)
            {
                Node.bUnlocked = true;
                NewlyUnlocked.Add(Node.EventID);
                bFoundNew = true;
                continue;
            }

            // Check every prerequisite event.
            bool bAllPrereqsFired = true;
            for (const FName& PrereqID : Node.Prerequisites)
            {
                bool bPrereqFired = false;
                for (const FHistoricalEventNode& Other : EventGraph)
                {
                    if (Other.EventID == PrereqID && Other.bFired)
                    {
                        bPrereqFired = true;
                        break;
                    }
                }
                if (!bPrereqFired)
                {
                    bAllPrereqsFired = false;
                    break;
                }
            }

            if (bAllPrereqsFired)
            {
                Node.bUnlocked = true;
                NewlyUnlocked.Add(Node.EventID);
                bFoundNew = true;
            }
        }
    }

    return NewlyUnlocked;
}

void UHistoricalCalendarSubsystem::LoadHistoricalEvents()
{
    EventGraph.Reset();

    // -----------------------------------------------------------------------
    // Canonical Swazi historical sequence (1750–1906)
    // Each entry builds causally on the previous, modelling the real
    // dependency chain: kingdom foundation → difaqane disruption →
    // boundary conflict → colonial concession race → constitutional crisis →
    // formal partition of sovereignty.
    // -----------------------------------------------------------------------

    // EV_NgwaneKingdom — Founding of the Ngwane kingdom under Ngwane III.
    // No prerequisites; this is the root of the entire dependency chain.
    {
        FHistoricalEventNode Node;
        Node.EventID         = FName("EV_NgwaneKingdom");
        Node.Year            = 1750;
        Node.NarrativeWeight = 2.0f;
        // Prerequisites intentionally empty — root node.
        EventGraph.Add(Node);
    }

    // EV_Mfecane — The difaqane (forced migration/wars) destabilises southern
    // Africa; the Swazi kingdom consolidates its identity under pressure.
    // Requires: EV_NgwaneKingdom (a kingdom must exist to be tested).
    {
        FHistoricalEventNode Node;
        Node.EventID         = FName("EV_Mfecane");
        Node.Year            = 1815;
        Node.NarrativeWeight = 3.0f;
        Node.Prerequisites.Add(FName("EV_NgwaneKingdom"));
        EventGraph.Add(Node);
    }

    // EV_LubomboBattle — Military confrontation in the Lubombo region tests
    // Swazi sovereignty against expanding neighbours.
    // Requires: EV_Mfecane (the difaqane created the geopolitical pressure).
    {
        FHistoricalEventNode Node;
        Node.EventID         = FName("EV_LubomboBattle");
        Node.Year            = 1846;
        Node.NarrativeWeight = 2.5f;
        Node.Prerequisites.Add(FName("EV_Mfecane"));
        EventGraph.Add(Node);
    }

    // EV_ConcessionRush — European concessionaires flood Swaziland seeking
    // mining, land, and trading rights from King Mbandzeni.
    // Requires: EV_LubomboBattle (colonial interest intensified after the
    // demonstration of military vulnerability).
    {
        FHistoricalEventNode Node;
        Node.EventID         = FName("EV_ConcessionRush");
        Node.Year            = 1880;
        Node.NarrativeWeight = 3.0f;
        Node.Prerequisites.Add(FName("EV_LubomboBattle"));
        EventGraph.Add(Node);
    }

    // EV_BhunuTrial — The trial of Prince Bhunu (Ngwane V) by the Transvaal
    // courts for murder; a direct assault on Swazi sovereignty.
    // Requires: EV_ConcessionRush (the concession era eroded Swazi legal
    // autonomy to the point where external jurisdiction was asserted).
    {
        FHistoricalEventNode Node;
        Node.EventID         = FName("EV_BhunuTrial");
        Node.Year            = 1898;
        Node.NarrativeWeight = 2.0f;
        Node.Prerequisites.Add(FName("EV_ConcessionRush"));
        EventGraph.Add(Node);
    }

    // EV_Partition — The Order-in-Council partitions Swaziland between Britain
    // and the Transvaal, effectively ending Swazi self-governance.
    // Requires: EV_BhunuTrial (the constitutional crisis made partition
    // politically feasible for the colonial powers).
    {
        FHistoricalEventNode Node;
        Node.EventID         = FName("EV_Partition");
        Node.Year            = 1906;
        Node.NarrativeWeight = 3.5f;
        Node.Prerequisites.Add(FName("EV_BhunuTrial"));
        EventGraph.Add(Node);
    }

    UE_LOG(LogMahlanya, Log,
        TEXT("UHistoricalCalendarSubsystem::LoadHistoricalEvents: Loaded %d events."),
        EventGraph.Num());
}
