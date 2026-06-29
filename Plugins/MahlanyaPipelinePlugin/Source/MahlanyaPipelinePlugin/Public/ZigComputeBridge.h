#pragma once

#include "CoreMinimal.h"

/**
 * FZigComputeBridge
 *
 * dlopen / FPlatformProcess wrapper for libmahlanya_compute.so.
 * Call Load() once at commandlet startup and Unload() on shutdown.
 * All function pointers are null until Load() succeeds.
 */
class MAHLANYAPIPELINEPLUGIN_API FZigComputeBridge
{
public:
    // ------------------------------------------------------------------
    // Lifecycle
    // ------------------------------------------------------------------

    /** Load the shared library from LibPath and resolve all symbols.
     *  Returns false (and logs an error) if any symbol cannot be resolved. */
    static bool Load(const FString& LibPath);

    /** Release the library handle. Safe to call even if not loaded. */
    static void Unload();

    /** Returns true if the library is currently loaded. */
    static bool IsLoaded();

    // ------------------------------------------------------------------
    // Erosion function-pointer typedefs
    // ------------------------------------------------------------------

    /**
     * ThermalErosionPass(HeightInOut, Hardness, Width, Height, TalusAngle, Dt)
     * One thermal (talus) erosion pass.
     */
    using FThermalErosionFn = void(*)(float* HeightInOut,
                                      const float* Hardness,
                                      int32 Width,
                                      int32 Height,
                                      float TalusAngle,
                                      float Dt);

    /**
     * FluvialErosionPass(HeightInOut, WaterInOut, SedimentInOut, Hardness,
     *                    Width, Height, Rain, Ks, Kd)
     * One fluvial (hydraulic) erosion pass.
     */
    using FFluvialErosionFn = void(*)(float* HeightInOut,
                                      float* WaterInOut,
                                      float* SedimentInOut,
                                      const float* Hardness,
                                      int32 Width,
                                      int32 Height,
                                      float Rain,
                                      float Ks,
                                      float Kd);

    /**
     * AeolianErosionPass(HeightInOut, Hardness, Width, Height, WindStrength, Dt)
     * One aeolian (wind) erosion pass.
     */
    using FAeolianErosionFn = void(*)(float* HeightInOut,
                                      const float* Hardness,
                                      int32 Width,
                                      int32 Height,
                                      float WindStrength,
                                      float Dt);

    /**
     * MassWastingPass(HeightInOut, Hardness, Width, Height, SlopeFactor, Seed)
     * One stochastic mass-wasting (landslide) pass.
     */
    using FMassWastingFn    = void(*)(float* HeightInOut,
                                      const float* Hardness,
                                      int32 Width,
                                      int32 Height,
                                      float SlopeFactor,
                                      uint64 Seed);

    // ------------------------------------------------------------------
    // D-infinity hydrology function-pointer typedefs
    // ------------------------------------------------------------------

    /**
     * DInfFillPits(HeightInOut, Width, Height)
     * Fill topographic pits in-place (Wang & Liu algorithm).
     */
    using FDInfFillPitsFn   = void(*)(float* HeightInOut,
                                      int32 Width,
                                      int32 Height);

    /**
     * DInfFlowDirection(Height, FlowDirOut, Width, Height)
     * Compute D-infinity flow directions (Tarboton 1997).
     * FlowDirOut: angle in radians, row-major.
     */
    using FDInfFlowDirFn    = void(*)(const float* Height,
                                      float* FlowDirOut,
                                      int32 Width,
                                      int32 Height);

    /**
     * DInfFlowAccumulation(FlowDir, AccumOut, Width, Height)
     * Accumulate upstream contributing area using D-infinity directions.
     */
    using FDInfFlowAccumFn  = void(*)(const float* FlowDir,
                                      float* AccumOut,
                                      int32 Width,
                                      int32 Height);

    // ------------------------------------------------------------------
    // Voronoi / Lloyd relaxation function-pointer typedefs
    // ------------------------------------------------------------------

    /**
     * LloydRelax(PointsInOut, NPoints, NIters, DomainW, DomainH)
     * Interleaved (x,y) doubles. Relaxes points toward centroid of their
     * Voronoi cell for NIters iterations.
     */
    using FLloydRelaxFn     = void(*)(double* PointsInOut,
                                      int32 NPoints,
                                      int32 NIters,
                                      double DomainW,
                                      double DomainH);

    /**
     * VoronoiCellAreas(Points, NPoints, AreasOut)
     * Fills AreasOut[i] with the area of point i's Voronoi cell.
     */
    using FVoronoiAreasFn   = void(*)(const double* Points,
                                      int32 NPoints,
                                      double* AreasOut);

    // ------------------------------------------------------------------
    // Static function pointers (null until Load() succeeds)
    // ------------------------------------------------------------------

    static FThermalErosionFn  ThermalErosionPass;
    static FFluvialErosionFn  FluvialErosionPass;
    static FAeolianErosionFn  AeolianErosionPass;
    static FMassWastingFn     MassWastingPass;
    static FDInfFillPitsFn    DInfFillPits;
    static FDInfFlowDirFn     DInfFlowDirection;
    static FDInfFlowAccumFn   DInfFlowAccumulation;
    static FLloydRelaxFn      LloydRelax;
    static FVoronoiAreasFn    VoronoiCellAreas;

private:
    static void* LibHandle;

    /** Resolve a single exported symbol into FnPtr. Returns false on failure. */
    template<typename T>
    static bool Resolve(T& FnPtr, const char* Symbol);
};
