#include "UEcologySimulatorSubsystem.h"
#include "SimulationBusSubsystem.h"

void UEcologySimulatorSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    SeedDefaultSpecies();
}

void UEcologySimulatorSubsystem::AddPair(
    FName PredID, FString PredName, float PR, float PD, float PA, float PB, float PPop,
    FName PreyID, FString PreyName, float YR, float YA, float YK, float YPop)
{
    FSpeciesPopulation Pred;
    Pred.SpeciesID = PredID; Pred.DisplayName = PredName;
    Pred.BirthRate = PR; Pred.DeathRate = PD; Pred.LV_Alpha = PA; Pred.LV_Beta = PB;
    Pred.Population = PPop; Pred.InitialPopulation = PPop;
    Pred.bIsPredator = true; Pred.PreySpeciesID = PreyID;
    SpeciesMap.Add(PredID, Pred);

    FSpeciesPopulation Prey;
    Prey.SpeciesID = PreyID; Prey.DisplayName = PreyName;
    Prey.BirthRate = YR; Prey.LV_Alpha = YA; Prey.CarryingCapacity = YK;
    Prey.Population = YPop; Prey.InitialPopulation = YPop;
    Prey.bIsPredator = false;
    SpeciesMap.Add(PreyID, Prey);
}

void UEcologySimulatorSubsystem::SeedDefaultSpecies()
{
    AddPair(FName("lion"),              TEXT("Lion"),               0.15f,0.08f,0.04f,0.02f, 40.f,
            FName("impala"),            TEXT("Impala"),             0.6f, 0.04f,1200.f, 800.f);
    AddPair(FName("leopard"),           TEXT("Leopard"),            0.12f,0.06f,0.035f,0.018f,60.f,
            FName("warthog"),           TEXT("Common Warthog"),     0.5f, 0.035f,900.f, 600.f);
    AddPair(FName("nile_crocodile"),    TEXT("Nile Crocodile"),     0.08f,0.04f,0.06f,0.012f,120.f,
            FName("tilapia"),           TEXT("Mozambique Tilapia"), 1.2f, 0.06f,8000.f,5000.f);
    AddPair(FName("african_wild_dog"),  TEXT("African Wild Dog"),   0.18f,0.10f,0.05f,0.025f,25.f,
            FName("plains_zebra"),      TEXT("Plains Zebra"),       0.35f,0.05f,700.f,  400.f);
    AddPair(FName("african_rock_python"),TEXT("African Rock Python"),0.10f,0.05f,0.08f,0.015f,200.f,
            FName("small_mammals"),     TEXT("Small Mammals"),      2.0f, 0.08f,15000.f,10000.f);
    AddPair(FName("black_mamba"),       TEXT("Black Mamba"),        0.14f,0.06f,0.10f,0.010f,500.f,
            FName("rodents"),           TEXT("Rodents"),            3.0f, 0.10f,30000.f,20000.f);
}

void UEcologySimulatorSubsystem::SimulateEcologyTick(float GameDayDelta)
{
    if (GameDayDelta <= 0.f) return;
    AccumulatedDays += GameDayDelta;
    while (AccumulatedDays >= WEEK_DAYS)
    {
        AccumulatedDays -= WEEK_DAYS;
        ProcessWeeklyEcology();
    }
}

void UEcologySimulatorSubsystem::ProcessWeeklyEcology()
{
    const float DeltaYears = WEEK_DAYS / 365.f;

    for (auto& Pair : SpeciesMap)
    {
        FSpeciesPopulation& Predator = Pair.Value;
        if (!Predator.bIsPredator) continue;

        FSpeciesPopulation* Prey = SpeciesMap.Find(Predator.PreySpeciesID);
        if (!Prey) continue;

        StepLotkaVolterra(*Prey, Predator, DeltaYears);
    }

    CheckPopulationTriggers();
}

void UEcologySimulatorSubsystem::StepLotkaVolterra(
    FSpeciesPopulation& Prey, FSpeciesPopulation& Predator, float DeltaYears)
{
    const float N = Prey.Population;
    const float P = Predator.Population;
    const float K = FMath::Max(Prey.CarryingCapacity, 1.f);

    const float dN = Prey.BirthRate * N * (1.f - N / K) - Prey.LV_Alpha * N * P;
    const float dP = Predator.LV_Beta * Predator.LV_Alpha * N * P - Predator.DeathRate * P;

    Prey.Population     = FMath::Max(N + dN * DeltaYears, 0.f);
    Predator.Population = FMath::Max(P + dP * DeltaYears, 0.f);
}

void UEcologySimulatorSubsystem::CheckPopulationTriggers()
{
    USimulationBusSubsystem* Bus = GetWorld()
        ? GetWorld()->GetSubsystem<USimulationBusSubsystem>()
        : nullptr;

    for (const auto& Pair : SpeciesMap)
    {
        const float Fraction = GetPopulationFraction(Pair.Key);
        if (Fraction < CRITICAL_FRACTION && Bus)
        {
            Bus->BroadcastSpeciesPopulationCritical(Pair.Key, Fraction);
        }
    }
}

void UEcologySimulatorSubsystem::RegisterSpecies(const FSpeciesPopulation& Species)
{
    SpeciesMap.Add(Species.SpeciesID, Species);
}

FSpeciesPopulation UEcologySimulatorSubsystem::GetSpeciesState(FName SpeciesID) const
{
    return SpeciesMap.FindRef(SpeciesID);
}

TArray<FName> UEcologySimulatorSubsystem::GetAllSpeciesIDs() const
{
    TArray<FName> Keys;
    SpeciesMap.GetKeys(Keys);
    return Keys;
}

float UEcologySimulatorSubsystem::GetPopulationFraction(FName SpeciesID) const
{
    const FSpeciesPopulation* S = SpeciesMap.Find(SpeciesID);
    if (!S) return 0.f;
    return S->Population / FMath::Max(S->InitialPopulation, 1.f);
}

void UEcologySimulatorSubsystem::ApplyDroughtStress(float DroughtSeverity)
{
    const float Factor = FMath::Clamp(1.f - DroughtSeverity * 0.5f, 0.01f, 1.f);
    for (auto& Pair : SpeciesMap)
    {
        if (!Pair.Value.bIsPredator)
            Pair.Value.BirthRate *= Factor;
    }
}
