// Copyright Charles Bartaria. All Rights Reserved.

#include "MahlanyaGameState.h"
#include "Net/UnrealNetwork.h"

AMahlanyaGameState::AMahlanyaGameState()
{
    bReplicates = true;
}

void AMahlanyaGameState::GetLifetimeReplicatedProps(
    TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    DOREPLIFETIME(AMahlanyaGameState, WeatherState);
    DOREPLIFETIME(AMahlanyaGameState, ClanSnapshots);
    DOREPLIFETIME(AMahlanyaGameState, CurrentGameYear);
}

void AMahlanyaGameState::ServerUpdateWeather(const FReplicatedWeatherState& NewState)
{
    if (!HasAuthority())
    {
        return;
    }
    WeatherState = NewState;
}

void AMahlanyaGameState::ServerUpdateClanSnapshot(const FReplicatedClanSnapshot& Snapshot)
{
    if (!HasAuthority())
    {
        return;
    }

    for (FReplicatedClanSnapshot& Existing : ClanSnapshots)
    {
        if (Existing.ClanID == Snapshot.ClanID)
        {
            Existing = Snapshot;
            return;
        }
    }
    ClanSnapshots.Add(Snapshot);
}

void AMahlanyaGameState::ServerAdvanceYear(int32 Delta)
{
    if (!HasAuthority())
    {
        return;
    }
    CurrentGameYear += Delta;
}
