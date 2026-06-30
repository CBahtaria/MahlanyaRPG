// Copyright Charles Bartaria / BRT Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "MahlanyaGameClockSubsystem.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnGameDayAdvanced,  int32, NewDay);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnGameYearAdvanced, int32, NewYear);

/**
 * UMahlanyaGameClockSubsystem
 *
 * Converts real time to Swazi historical game-time and drives all simulation
 * subsystems that require a periodic tick:
 *
 *   Every game-day (default 1 real second):
 *     → UEconomySimulatorSubsystem::SimulateEconomyTick(1.0)
 *
 *   Every 7 game-days:
 *     → UEcologySimulatorSubsystem::SimulateEcologyTick(7.0)
 *
 *   Every 365 game-days (year wrap):
 *     → AMahlanyaGameState::ServerAdvanceYear(1)
 *       (which routes through UYearChangeOrchestrator to spread BFS events)
 *
 * Auto-starts on authority (server or standalone). Clients are listeners only;
 * the authoritative state replicates through AMahlanyaGameState.
 *
 * Time speed is configurable via SecondsPerGameDay for debug time-warp or
 * cutscene freeze. Call StopClock/StartClock to pause/resume.
 */
UCLASS()
class MAHLANYARPG_API UMahlanyaGameClockSubsystem : public UWorldSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    // ── Clock control ─────────────────────────────────────────────────────────

    /** Start the clock. Called automatically on authority during Initialize(). */
    UFUNCTION(BlueprintCallable, Category = "Mahlanya|Time")
    void StartClock();

    /** Pause the clock without resetting accumulators. */
    UFUNCTION(BlueprintCallable, Category = "Mahlanya|Time")
    void StopClock();

    // ── Configuration ─────────────────────────────────────────────────────────

    /**
     * Real seconds that elapse per in-game day.
     * Default 1.0 → 1 second/day → 365 seconds/year.
     * Set to 0.1 for rapid debug time-warp (10 days per second).
     * Changing this while the clock is running restarts the timer.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mahlanya|Time",
              meta = (ClampMin = "0.05", ClampMax = "60.0"))
    float SecondsPerGameDay = 1.0f;

    /**
     * Starting year for a new session. Matched to the Swazi historical window.
     * The plan places Mahlanya's arc from 1750 (Ngwane III founds the kingdom)
     * through 1906 (Partition of Eswatini).
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mahlanya|Time")
    int32 StartingYear = 1750;

    // ── State queries ─────────────────────────────────────────────────────────

    /** Total in-game days elapsed since the session began. */
    UFUNCTION(BlueprintPure, Category = "Mahlanya|Time")
    int32 GetCurrentGameDay() const { return CurrentGameDay; }

    /** Current in-game year (1750–1906). */
    UFUNCTION(BlueprintPure, Category = "Mahlanya|Time")
    int32 GetCurrentGameYear() const { return CurrentGameYear; }

    /** Day within the current year (0–364). */
    UFUNCTION(BlueprintPure, Category = "Mahlanya|Time")
    int32 GetDayOfYear() const { return DayOfYear; }

    UFUNCTION(BlueprintPure, Category = "Mahlanya|Time")
    bool IsClockRunning() const { return ClockTimerHandle.IsValid(); }

    // ── Delegates ─────────────────────────────────────────────────────────────

    /** Broadcast after every game-day tick on authority. */
    UPROPERTY(BlueprintAssignable, Category = "Mahlanya|Time")
    FOnGameDayAdvanced OnGameDayAdvanced;

    /** Broadcast when the year rolls over. */
    UPROPERTY(BlueprintAssignable, Category = "Mahlanya|Time")
    FOnGameYearAdvanced OnGameYearAdvanced;

private:
    void OnClockTick();

    FTimerHandle    ClockTimerHandle;
    FDelegateHandle DroughtWireHandle;

    int32 CurrentGameDay  = 0;
    int32 CurrentGameYear = 1750;
    int32 DayOfYear       = 0;   // 0 – 364

    static constexpr int32 DaysPerYear  = 365;
    static constexpr int32 DaysPerWeek  = 7;
};
