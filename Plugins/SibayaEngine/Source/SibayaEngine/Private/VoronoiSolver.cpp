#include "VoronoiSolver.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformProcess.h"
#include "Misc/Paths.h"
#include "Math/UnrealMathUtility.h"

// ---------------------------------------------------------------------------
// Private helpers
// ---------------------------------------------------------------------------

void* FVoronoiSolver::TryLoadZigLib()
{
    // 1. Check environment variable override first
    // FPlatformMisc::GetEnvironmentVariable returns an FString in UE5
    FString EnvPath = FPlatformMisc::GetEnvironmentVariable(TEXT("MAHLANYA_ZIG_LIB_PATH"));
    if (!EnvPath.IsEmpty())
    {
        void* Handle = FPlatformProcess::GetDllHandle(*EnvPath);
        if (Handle)
        {
            return Handle;
        }
    }

    // 2. Fall back to conventional plugin lib directory
    const FString DefaultPath = FPaths::ProjectPluginsDir()
        / TEXT("SibayaEngine") / TEXT("lib") / TEXT("libmahlanya_compute.so");

    return FPlatformProcess::GetDllHandle(*DefaultPath);
}

bool FVoronoiSolver::IsInTabooArc(const FVector2D& Point)
{
    const float dx = Point.X - 0.5f;
    const float dy = Point.Y - 0.5f;

    // Atan2 returns [-180, 180]; convert to [0, 360)
    float Degrees = FMath::RadiansToDegrees(FMath::Atan2(dy, dx));
    if (Degrees < 0.f)
    {
        Degrees += 360.f;
    }

    return Degrees >= 240.f && Degrees <= 300.f;
}

FVector2D FVoronoiSolver::ReflectFromTabooArc(const FVector2D& Point)
{
    const float dx = Point.X - 0.5f;
    const float dy = Point.Y - 0.5f;
    const float Dist = FMath::Sqrt(dx * dx + dy * dy);

    float Bearing = FMath::RadiansToDegrees(FMath::Atan2(dy, dx));
    if (Bearing < 0.f)
    {
        Bearing += 360.f;
    }

    // Midpoint of taboo arc is 270° (West). Reflect around 270°:
    // newBearing = 540° - bearing
    // This maps 240→300, 300→240, 270→270.
    float NewBearing = 540.f - Bearing;
    // Normalize back to [0, 360)
    NewBearing = FMath::Fmod(NewBearing + 360.f, 360.f);

    const float RadBearing = FMath::DegreesToRadians(NewBearing);
    return FVector2D(0.5f + FMath::Cos(RadBearing) * Dist,
                     0.5f + FMath::Sin(RadBearing) * Dist);
}

// ---------------------------------------------------------------------------
// Pure C++ fallback Lloyd relaxation
// ---------------------------------------------------------------------------

void FVoronoiSolver::LloydRelaxCPP(TArray<FVector2D>& Points, int32 Iterations)
{
    constexpr int32 N = 32;
    const int32 NumPoints = Points.Num();
    if (NumPoints == 0)
    {
        return;
    }

    for (int32 Iter = 0; Iter < Iterations; ++Iter)
    {
        // Accumulate centroid sums and counts per generator
        TArray<FVector2D> Centroids;
        TArray<int32>     Counts;
        Centroids.SetNumZeroed(NumPoints);
        Counts.SetNumZeroed(NumPoints);

        // Sample N×N grid in [0,1]²
        for (int32 sy = 0; sy < N; ++sy)
        {
            for (int32 sx = 0; sx < N; ++sx)
            {
                const FVector2D Sample(
                    (sx + 0.5f) / static_cast<float>(N),
                    (sy + 0.5f) / static_cast<float>(N));

                // Skip samples that lie inside the taboo arc
                if (IsInTabooArc(Sample))
                {
                    continue;
                }

                // Find nearest generator
                int32 NearestIdx = 0;
                float MinDistSq  = FLT_MAX;
                for (int32 pi = 0; pi < NumPoints; ++pi)
                {
                    const float DistSq = FVector2D::DistSquared(Sample, Points[pi]);
                    if (DistSq < MinDistSq)
                    {
                        MinDistSq  = DistSq;
                        NearestIdx = pi;
                    }
                }

                Centroids[NearestIdx] += Sample;
                Counts[NearestIdx]    += 1;
            }
        }

        // Move each generator to the centroid of its cell
        for (int32 pi = 0; pi < NumPoints; ++pi)
        {
            if (Counts[pi] > 0)
            {
                Points[pi] = Centroids[pi] / static_cast<float>(Counts[pi]);
            }

            // Reflect out of taboo arc if needed
            if (IsInTabooArc(Points[pi]))
            {
                Points[pi] = ReflectFromTabooArc(Points[pi]);
            }

            // Clamp to [0.05, 0.95]² to avoid degenerate boundary cells
            Points[pi].X = FMath::Clamp(Points[pi].X, 0.05f, 0.95f);
            Points[pi].Y = FMath::Clamp(Points[pi].Y, 0.05f, 0.95f);
        }
    }
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void FVoronoiSolver::LloydRelax(TArray<FVector2D>& InOutPoints, int32 Iterations)
{
    void* LibHandle = TryLoadZigLib();

    if (LibHandle)
    {
        // Signature: void lloyd_relax(double* points, int32 num_points,
        //                             int32 iterations,
        //                             double taboo_start, double taboo_end)
        typedef void (*FLloydRelaxFn)(double*, int32, int32, double, double);
        FLloydRelaxFn LloydRelaxFn = reinterpret_cast<FLloydRelaxFn>(
            FPlatformProcess::GetDllExport(LibHandle, TEXT("lloyd_relax")));

        if (LloydRelaxFn)
        {
            const int32 NumPoints = InOutPoints.Num();

            // Pack points into flat double array: x0, y0, x1, y1, ...
            TArray<double> PackedPoints;
            PackedPoints.SetNumUninitialized(NumPoints * 2);
            for (int32 i = 0; i < NumPoints; ++i)
            {
                PackedPoints[i * 2]     = static_cast<double>(InOutPoints[i].X);
                PackedPoints[i * 2 + 1] = static_cast<double>(InOutPoints[i].Y);
            }

            LloydRelaxFn(PackedPoints.GetData(), NumPoints, Iterations, 240.0, 300.0);

            // Unpack back
            for (int32 i = 0; i < NumPoints; ++i)
            {
                InOutPoints[i].X = static_cast<float>(PackedPoints[i * 2]);
                InOutPoints[i].Y = static_cast<float>(PackedPoints[i * 2 + 1]);
            }

            FPlatformProcess::FreeDllHandle(LibHandle);
            return;
        }

        FPlatformProcess::FreeDllHandle(LibHandle);
    }

    // Fall through to pure C++ implementation
    LloydRelaxCPP(InOutPoints, Iterations);
}

FVoronoiLayout FVoronoiSolver::ComputeLayout(int32 NumWives, int32 NumSons,
                                               int32 NumDependents, int32 CattleCount,
                                               float TerrainGradient)
{
    TArray<FVector2D> AllPoints;
    TArray<float>     AllWeights;

    // Index 0: Sibaya — cattle kraal at centre (fixed, never moved by Lloyd)
    AllPoints.Add(FVector2D(0.5f, 0.5f));
    AllWeights.Add(1.0f);

    // Index 1: IndluLeyinkhulu — West of Sibaya
    AllPoints.Add(FVector2D(0.35f, 0.5f));
    AllWeights.Add(0.95f);

    // Index 2: Chief's hut — East apex
    AllPoints.Add(FVector2D(0.8f, 0.5f));
    AllWeights.Add(0.90f);

    // Wife huts: radial placement with seniority decay
    // Angle alternates North/South of the East axis (0°)
    for (int32 i = 0; i < NumWives; ++i)
    {
        const int32 Rank = i + 1;

        // Radial distance: 0.15 / rank^0.6 from centre
        const float Dist = 0.15f / FMath::Pow(static_cast<float>(Rank), 0.6f);

        // Alternate North (+Y) / South (-Y) of the East axis
        // Even indices: North side (positive Y), Odd: South (negative Y)
        const float Sign      = (i % 2 == 0) ? 1.f : -1.f;
        // Offset along East axis so wives are to the East of centre
        const float OffsetX   = 0.1f + Dist * 0.5f;
        const float OffsetY   = Sign * Dist;

        const FVector2D WifePos(
            FMath::Clamp(0.5f + OffsetX, 0.05f, 0.95f),
            FMath::Clamp(0.5f + OffsetY, 0.05f, 0.95f));

        AllPoints.Add(WifePos);
        AllWeights.Add(FMath::Max(0.1f, 0.75f - 0.02f * static_cast<float>(Rank - 1)));
    }

    // Son huts (Lilawu / bachelor quarters): near East arc
    for (int32 i = 0; i < NumSons; ++i)
    {
        // Spread along East arc [0.7..0.85, ±0.2]
        const float Sign = (i % 2 == 0) ? 1.f : -1.f;
        const float XPos = FMath::Lerp(0.70f, 0.85f, static_cast<float>(i) /
                                        FMath::Max(1, NumSons - 1 + (NumSons == 1 ? 1 : 0)));
        const float YPos = 0.5f + Sign * 0.2f * FMath::Min(1.f,
                           static_cast<float>((i / 2) + 1) / FMath::Max(1.f, static_cast<float>(NumSons) * 0.5f));

        AllPoints.Add(FVector2D(FMath::Clamp(XPos, 0.05f, 0.95f),
                                FMath::Clamp(YPos, 0.05f, 0.95f)));
        AllWeights.Add(0.55f);
    }

    // Dependent huts (Guma enclosures): fill remaining arc
    for (int32 i = 0; i < NumDependents; ++i)
    {
        // Distribute around the periphery, avoiding the taboo arc (West)
        // Use angles from 310° to 230° going clockwise (i.e. 310..360+0..230)
        // Map to a safe arc that avoids [240°, 300°]
        // Safe range: 300° to 240° going the long way (via East/North/South)
        // Total safe arc = 360 - 60 = 300°, starting from 300° going through 0° to 240°
        const float SafeArcDeg = 300.f;
        const float StartDeg   = 300.f;  // Just past taboo end
        const float T          = static_cast<float>(i) /
                                 FMath::Max(1.f, static_cast<float>(NumDependents));
        float Angle = FMath::Fmod(StartDeg + T * SafeArcDeg, 360.f);

        const float Rad  = FMath::DegreesToRadians(Angle);
        const float Dist = 0.35f;  // Outer ring
        FVector2D DepPos(0.5f + FMath::Cos(Rad) * Dist,
                         0.5f + FMath::Sin(Rad) * Dist);

        // Safety: reflect if somehow in taboo arc
        if (IsInTabooArc(DepPos))
        {
            DepPos = ReflectFromTabooArc(DepPos);
        }

        DepPos.X = FMath::Clamp(DepPos.X, 0.05f, 0.95f);
        DepPos.Y = FMath::Clamp(DepPos.Y, 0.05f, 0.95f);

        AllPoints.Add(DepPos);
        AllWeights.Add(0.40f);
    }

    // Run Lloyd relaxation on all moveable points (indices >= 1; Sibaya at 0 stays fixed)
    if (AllPoints.Num() > 1)
    {
        // Extract moveable points (all except index 0)
        TArray<FVector2D> MoveablePoints;
        MoveablePoints.Append(AllPoints.GetData() + 1, AllPoints.Num() - 1);

        LloydRelax(MoveablePoints, 50);

        // Write relaxed positions back (Sibaya at index 0 unchanged)
        for (int32 i = 0; i < MoveablePoints.Num(); ++i)
        {
            AllPoints[i + 1] = MoveablePoints[i];
        }
    }

    // Pack into FVoronoiLayout
    FVoronoiLayout Layout;
    Layout.HutPositions        = AllPoints;
    Layout.HierarchicalWeights = AllWeights;
    return Layout;
}
