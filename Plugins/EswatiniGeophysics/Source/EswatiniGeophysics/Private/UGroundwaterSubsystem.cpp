// Copyright Charles Bartaria / BRT Inc. All Rights Reserved.

#include "UGroundwaterSubsystem.h"

void UGroundwaterSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    GridResolution = DEFAULT_GRID_RESOLUTION;
    CellSizeM = DEFAULT_CELL_SIZE_M;
    HydraulicConductivityMPerDay = DEFAULT_CONDUCTIVITY_M_PER_DAY;
    InitialiseGrid();
}

void UGroundwaterSubsystem::Deinitialize()
{
    HeadGrid.Reset();
    FlowGrid.Reset();
    Super::Deinitialize();
}

void UGroundwaterSubsystem::InitialiseGrid()
{
    const int32 Cells = FMath::Max(1, GridResolution) * FMath::Max(1, GridResolution);
    HeadGrid.Init(INITIAL_HEAD_M, Cells);
    FlowGrid.Init(0.f, Cells);
}

int32 UGroundwaterSubsystem::IndexOf(int32 Col, int32 Row) const
{
    return Row * GridResolution + Col;
}

bool UGroundwaterSubsystem::InBounds(int32 Col, int32 Row) const
{
    return Col >= 0 && Col < GridResolution && Row >= 0 && Row < GridResolution;
}

void UGroundwaterSubsystem::StepFlow(float GameDayDelta)
{
    if (HeadGrid.Num() != GridResolution * GridResolution)
    {
        InitialiseGrid();
    }

    const float K = FMath::Clamp(HydraulicConductivityMPerDay, MIN_CONDUCTIVITY, MAX_CONDUCTIVITY);
    const float Dl = FMath::Max(CellSizeM, KINDA_SMALL_NUMBER);
    const float CellArea = CellSizeM * CellSizeM;

    TArray<float> NextHead;
    NextHead.SetNumUninitialized(HeadGrid.Num());

    for (int32 Row = 0; Row < GridResolution; ++Row)
    {
        for (int32 Col = 0; Col < GridResolution; ++Col)
        {
            const int32 Idx = IndexOf(Col, Row);
            const float H = HeadGrid[Idx];

            // Simple 4-neighbour finite-difference smoothing driven by Darcy Q = -K * A * dh/dl.
            float NeighbourSum = 0.f;
            int32 NeighbourCount = 0;
            const int32 Offsets[4][2] = { {1,0}, {-1,0}, {0,1}, {0,-1} };
            for (int32 N = 0; N < 4; ++N)
            {
                const int32 NCol = Col + Offsets[N][0];
                const int32 NRow = Row + Offsets[N][1];
                if (InBounds(NCol, NRow))
                {
                    NeighbourSum += HeadGrid[IndexOf(NCol, NRow)];
                    ++NeighbourCount;
                }
            }
            const float NeighbourMean = (NeighbourCount > 0) ? NeighbourSum / static_cast<float>(NeighbourCount) : H;

            const float Gradient = (NeighbourMean - H) / Dl;
            NextHead[Idx] = H + K * Gradient * GameDayDelta;
            FlowGrid[Idx] = -K * CellArea * Gradient;
        }
    }

    HeadGrid = MoveTemp(NextHead);
}

float UGroundwaterSubsystem::GetHead(int32 Col, int32 Row) const
{
    if (!InBounds(Col, Row))
    {
        return 0.f;
    }
    return HeadGrid[IndexOf(Col, Row)];
}

float UGroundwaterSubsystem::GetFlowRateM3PerDay(int32 Col, int32 Row) const
{
    if (!InBounds(Col, Row))
    {
        return 0.f;
    }
    return FlowGrid[IndexOf(Col, Row)];
}
