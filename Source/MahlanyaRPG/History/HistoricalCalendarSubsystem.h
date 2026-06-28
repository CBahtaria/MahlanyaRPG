#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "HistoricalCalendarSubsystem.generated.h"

// ---------------------------------------------------------------------------
// FHistoricalEventNode
//
// Represents a single node in the Swazi historical event dependency graph.
// Prerequisites must all have fired before this event becomes unlocked.
// BFS traversal in AdvanceYear() propagates unlock status through the graph.
// ---------------------------------------------------------------------------
USTRUCT(BlueprintType)
struct FHistoricalEventNode
{
    GENERATED_BODY()

    /** Unique identifier for this historical event, e.g. "EV_Mfecane". */
    UPROPERTY(EditAnywhere, Category = "History")
    FName EventID;

    /** In-game year this event fires once unlocked, e.g. 1815. */
    UPROPERTY(EditAnywhere, Category = "History")
    int32 Year = 0;

    /**
     * IDs of events that must have already fired before this node unlocks.
     * An empty array means the event is immediately unlocked at subsystem init.
     */
    UPROPERTY(EditAnywhere, Category = "History")
    TArray<FName> Prerequisites;

    /**
     * Importance multiplier used by narrative and quest systems to weight
     * this event relative to others. Higher values indicate greater historical
     * significance in the Swazi context.
     */
    UPROPERTY(EditAnywhere, Category = "History")
    float NarrativeWeight = 1.0f;

    /** Set to true once this event has been broadcast. Never resets. */
    UPROPERTY(BlueprintReadOnly, Category = "History")
    bool bFired = false;

    /**
     * Set to true once all prerequisites have fired (or the prerequisite list
     * is empty). An unlocked event will fire the next time AdvanceYear() is
     * called with Year >= this node's Year.
     */
    UPROPERTY(BlueprintReadOnly, Category = "History")
    bool bUnlocked = false;
};

// ---------------------------------------------------------------------------
// UHistoricalCalendarSubsystem
//
// World subsystem that manages the Swazi historical event dependency graph
// (1750–1906). Uses BFS to propagate unlock status whenever the in-game year
// advances, then fires all events whose Year has been reached.
//
// Pattern: mdbesten/emergence — BFS dependency tree where prerequisite events
// unlock subsequent events, modelling the causal chain of Swazi history.
// ---------------------------------------------------------------------------
UCLASS()
class MAHLANYARPG_API UHistoricalCalendarSubsystem : public UWorldSubsystem
{
    GENERATED_BODY()

public:
    // -----------------------------------------------------------------------
    // UWorldSubsystem interface
    // -----------------------------------------------------------------------

    /**
     * Populates the event graph with the hardcoded Swazi historical sequence
     * and runs an initial BFS pass so root events (no prerequisites) are
     * immediately unlocked.
     */
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;

    // -----------------------------------------------------------------------
    // Public API
    // -----------------------------------------------------------------------

    /**
     * Advance the in-game calendar to NewYear.
     * Order of operations:
     *   1. Update CurrentYear.
     *   2. BFSUnlock() — mark any events whose prerequisites have all fired.
     *   3. For each unlocked, unfired event with Year <= NewYear: fire it.
     */
    UFUNCTION(BlueprintCallable, Category = "Mahlanya|History")
    void AdvanceYear(int32 NewYear);

    /**
     * Returns true if the event identified by EventID has already fired.
     * Safe to call from any context; does not mutate state.
     */
    UFUNCTION(BlueprintPure, Category = "Mahlanya|History")
    bool HasEventFired(FName EventID) const;

    /**
     * Returns the list of event IDs that fired during the most recent call to
     * AdvanceYear(). Cleared at the start of each AdvanceYear() call.
     */
    UFUNCTION(BlueprintPure, Category = "Mahlanya|History")
    TArray<FName> GetEventsFiredThisYear() const;

    // -----------------------------------------------------------------------
    // Delegates
    // -----------------------------------------------------------------------

    /**
     * Broadcast immediately after an event fires.
     * @param EventID   The FName identifier of the historical event.
     * @param Year      The in-game year in which the event fired.
     */
    DECLARE_MULTICAST_DELEGATE_TwoParams(FOnHistoricalEventFired, FName /*EventID*/, int32 /*Year*/);
    FOnHistoricalEventFired OnHistoricalEventFired;

protected:
    // -----------------------------------------------------------------------
    // Internal state
    // -----------------------------------------------------------------------

    /** Full event graph. Populated once by LoadHistoricalEvents(). */
    UPROPERTY()
    TArray<FHistoricalEventNode> EventGraph;

    /** Current in-game year; initialised to 1750 (founding of the Ngwane kingdom). */
    int32 CurrentYear = 1750;

    /** Events that fired during the most recent AdvanceYear() call. */
    TArray<FName> FiredThisYear;

    // -----------------------------------------------------------------------
    // Internal helpers
    // -----------------------------------------------------------------------

    /**
     * BFS unlock pass.
     * Iterates the event graph; for each unfired, non-unlocked event, checks
     * whether all entries in its Prerequisites array have bFired == true. If
     * so, marks bUnlocked = true and records the ID for the return value.
     *
     * The BFS loop repeats until no new nodes are unlocked in a full pass,
     * correctly handling chains of arbitrary depth.
     *
     * @return IDs of all events newly unlocked during this call.
     */
    TArray<FName> BFSUnlock();

    /**
     * Populates EventGraph with the canonical Swazi historical sequence
     * (1750–1906). Called once from Initialize().
     */
    void LoadHistoricalEvents();
};
