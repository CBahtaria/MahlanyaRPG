#include "UHistoricalCalendarSubsystem.h"
#include "SimulationBusSubsystem.h"

void UHistoricalCalendarSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    SetStartYear(1750);
    RegisterHistoricalEvents();
}

void UHistoricalCalendarSubsystem::SetStartYear(int32 Year)
{
    CurrentYear = Year;
    DayOfYear = 0.f;
}

void UHistoricalCalendarSubsystem::RegisterHistoricalEvents()
{
    EventChain.Empty();

    // EV_NgwaneKingdom: The founding of the Ngwane Kingdom
    {
        FHistoricalEventNode Node;
        Node.EventID = FName("EV_NgwaneKingdom");
        Node.Year = 1750;
        Node.Prerequisites = {};
        Node.NarrativeWeight = 2.0f;
        Node.bFired = false;
        EventChain.Add(Node);
    }

    // EV_Mfecane: The Great Crushing/Scattering
    {
        FHistoricalEventNode Node;
        Node.EventID = FName("EV_Mfecane");
        Node.Year = 1815;
        Node.Prerequisites = { FName("EV_NgwaneKingdom") };
        Node.NarrativeWeight = 3.0f;
        Node.bFired = false;
        EventChain.Add(Node);
    }

    // EV_LubomboBattle: Battle of the Lubombo
    {
        FHistoricalEventNode Node;
        Node.EventID = FName("EV_LubomboBattle");
        Node.Year = 1846;
        Node.Prerequisites = { FName("EV_Mfecane") };
        Node.NarrativeWeight = 2.5f;
        Node.bFired = false;
        EventChain.Add(Node);
    }

    // EV_ConcessionRush: The Concession Rush era
    {
        FHistoricalEventNode Node;
        Node.EventID = FName("EV_ConcessionRush");
        Node.Year = 1880;
        Node.Prerequisites = { FName("EV_LubomboBattle") };
        Node.NarrativeWeight = 3.0f;
        Node.bFired = false;
        EventChain.Add(Node);
    }

    // EV_BhunuTrial: The trial of King Bhunu
    {
        FHistoricalEventNode Node;
        Node.EventID = FName("EV_BhunuTrial");
        Node.Year = 1898;
        Node.Prerequisites = { FName("EV_ConcessionRush") };
        Node.NarrativeWeight = 2.0f;
        Node.bFired = false;
        EventChain.Add(Node);
    }

    // EV_Partition: The Partition of Swaziland
    {
        FHistoricalEventNode Node;
        Node.EventID = FName("EV_Partition");
        Node.Year = 1906;
        Node.Prerequisites = { FName("EV_BhunuTrial") };
        Node.NarrativeWeight = 4.0f;
        Node.bFired = false;
        EventChain.Add(Node);
    }
}

void UHistoricalCalendarSubsystem::AdvanceTime(float GameDayDelta)
{
    DayOfYear += GameDayDelta;

    while (DayOfYear >= 365.f)
    {
        DayOfYear -= 365.f;
        CurrentYear++;
    }

    TryFireEvents();
}

void UHistoricalCalendarSubsystem::TryFireEvents()
{
    USimulationBusSubsystem* Bus = GetWorld()->GetSubsystem<USimulationBusSubsystem>();

    for (FHistoricalEventNode& Node : EventChain)
    {
        if (Node.bFired)
        {
            continue;
        }

        if (Node.Year > CurrentYear)
        {
            continue;
        }

        if (!PrerequisitesMet(Node))
        {
            continue;
        }

        Node.bFired = true;
        FiredEvents.Add(Node.EventID);

        if (Bus)
        {
            // Broadcast using the ConditionTag/ConditionValue signature.
            // NarrativeWeight is used as the condition value so subscribers
            // can weight the narrative impact of the event.
            Bus->BroadcastQuestTriggerConditionMet(Node.EventID, Node.NarrativeWeight);
        }
    }
}

bool UHistoricalCalendarSubsystem::PrerequisitesMet(const FHistoricalEventNode& Node) const
{
    for (const FName& PrereqID : Node.Prerequisites)
    {
        if (!FiredEvents.Contains(PrereqID))
        {
            return false;
        }
    }
    return true;
}

bool UHistoricalCalendarSubsystem::HasEventFired(FName EventID) const
{
    return FiredEvents.Contains(EventID);
}

TArray<FHistoricalEventNode> UHistoricalCalendarSubsystem::GetPendingEvents() const
{
    TArray<FHistoricalEventNode> Pending;
    for (const FHistoricalEventNode& Node : EventChain)
    {
        if (!Node.bFired && Node.Year <= CurrentYear && PrerequisitesMet(Node))
        {
            Pending.Add(Node);
        }
    }
    return Pending;
}
