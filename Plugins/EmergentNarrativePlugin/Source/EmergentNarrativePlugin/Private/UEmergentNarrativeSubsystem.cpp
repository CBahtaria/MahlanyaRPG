#include "UEmergentNarrativeSubsystem.h"
#include "SimulationBusSubsystem.h"
#include "UHistoricalCalendarSubsystem.h"

void UEmergentNarrativeSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    ImbongiGen = NewObject<UImbongiGenerator>(this);

    RegisterTriggerRules();

    USimulationBusSubsystem* Bus = GetWorld()->GetSubsystem<USimulationBusSubsystem>();
    if (Bus)
    {
        Bus->OnQuestTriggerConditionMet.AddUObject(this, &UEmergentNarrativeSubsystem::OnQuestConditionMet);
    }
}

void UEmergentNarrativeSubsystem::Deinitialize()
{
    USimulationBusSubsystem* Bus = GetWorld() ? GetWorld()->GetSubsystem<USimulationBusSubsystem>() : nullptr;
    if (Bus)
    {
        Bus->OnQuestTriggerConditionMet.RemoveAll(this);
    }

    Super::Deinitialize();
}

void UEmergentNarrativeSubsystem::RegisterTriggerRules()
{
    TriggerRules.Empty();

    {
        FQuestTriggerRule Rule;
        Rule.QuestTemplateID = FName("QuestRaidOfNeed");
        Rule.ConditionID = FName("RaidOfNeed");
        Rule.CooldownDays = 90.f;
        Rule.bFired = false;
        Rule.LastFiredDay = -999.f;
        TriggerRules.Add(Rule);
    }

    {
        FQuestTriggerRule Rule;
        Rule.QuestTemplateID = FName("QuestConcessionBetrayal");
        Rule.ConditionID = FName("ConcessionBetrayal");
        Rule.CooldownDays = 60.f;
        Rule.bFired = false;
        Rule.LastFiredDay = -999.f;
        TriggerRules.Add(Rule);
    }

    {
        FQuestTriggerRule Rule;
        Rule.QuestTemplateID = FName("QuestMfecane");
        Rule.ConditionID = FName("EV_Mfecane");
        Rule.CooldownDays = 0.f;
        Rule.bFired = false;
        Rule.LastFiredDay = -999.f;
        TriggerRules.Add(Rule);
    }

    {
        FQuestTriggerRule Rule;
        Rule.QuestTemplateID = FName("QuestLubombo");
        Rule.ConditionID = FName("EV_LubomboBattle");
        Rule.CooldownDays = 0.f;
        Rule.bFired = false;
        Rule.LastFiredDay = -999.f;
        TriggerRules.Add(Rule);
    }

    {
        FQuestTriggerRule Rule;
        Rule.QuestTemplateID = FName("QuestPartition");
        Rule.ConditionID = FName("EV_Partition");
        Rule.CooldownDays = 0.f;
        Rule.bFired = false;
        Rule.LastFiredDay = -999.f;
        TriggerRules.Add(Rule);
    }

    {
        FQuestTriggerRule Rule;
        Rule.QuestTemplateID = FName("QuestBhunuTrial");
        Rule.ConditionID = FName("EV_BhunuTrial");
        Rule.CooldownDays = 0.f;
        Rule.bFired = false;
        Rule.LastFiredDay = -999.f;
        TriggerRules.Add(Rule);
    }
}

void UEmergentNarrativeSubsystem::OnQuestConditionMet(FName ConditionID, float ConditionValue)
{
    for (FQuestTriggerRule& Rule : TriggerRules)
    {
        if (Rule.ConditionID != ConditionID)
        {
            continue;
        }

        // Check cooldown: the rule is eligible if enough days have elapsed since
        // it last fired, or if it has never fired.
        const float DaysSinceLastFired = TotalDaysElapsed - Rule.LastFiredDay;
        if (Rule.CooldownDays > 0.f && DaysSinceLastFired < Rule.CooldownDays)
        {
            continue;
        }

        Rule.bFired = true;
        Rule.LastFiredDay = TotalDaysElapsed;
        ActiveQuests.Add(Rule.QuestTemplateID);

        USimulationBusSubsystem* Bus = GetWorld()->GetSubsystem<USimulationBusSubsystem>();
        if (Bus)
        {
            Bus->BroadcastQuestTriggerConditionMet(Rule.QuestTemplateID, ConditionValue);
        }

        // Each ConditionID maps to exactly one rule; stop searching.
        break;
    }
}

void UEmergentNarrativeSubsystem::AdvanceNarrativeTime(float GameDayDelta)
{
    TotalDaysElapsed += GameDayDelta;

    UHistoricalCalendarSubsystem* Calendar = GetWorld()->GetSubsystem<UHistoricalCalendarSubsystem>();
    if (Calendar)
    {
        Calendar->AdvanceTime(GameDayDelta);
    }
}

FString UEmergentNarrativeSubsystem::GenerateImbongiVerse(
    const FString& PlayerNameRoot,
    int32 CattleCount,
    int32 HuntsCompleted,
    const FString& EnemyName,
    const FString& LocationName) const
{
    if (!ImbongiGen)
    {
        return FString();
    }
    return ImbongiGen->GenerateVerse(PlayerNameRoot, CattleCount, HuntsCompleted, EnemyName, LocationName);
}

bool UEmergentNarrativeSubsystem::HasQuestFired(FName QuestTemplateID) const
{
    return ActiveQuests.Contains(QuestTemplateID);
}

TArray<FName> UEmergentNarrativeSubsystem::GetActiveQuests() const
{
    return ActiveQuests.Array();
}
