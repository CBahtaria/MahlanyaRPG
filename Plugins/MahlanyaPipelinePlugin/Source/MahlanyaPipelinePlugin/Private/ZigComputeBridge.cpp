// Copyright Charles Bartaria. All Rights Reserved.

#include "ZigComputeBridge.h"
#include "MahlanyaPipelineTypes.h"
#include "HAL/PlatformProcess.h"

// ---------------------------------------------------------------------------
// Static member definitions
// ---------------------------------------------------------------------------

void* FZigComputeBridge::LibHandle = nullptr;

FZigComputeBridge::FThermalErosionFn  FZigComputeBridge::ThermalErosionPass  = nullptr;
FZigComputeBridge::FFluvialErosionFn  FZigComputeBridge::FluvialErosionPass  = nullptr;
FZigComputeBridge::FAeolianErosionFn  FZigComputeBridge::AeolianErosionPass  = nullptr;
FZigComputeBridge::FMassWastingFn     FZigComputeBridge::MassWastingPass     = nullptr;
FZigComputeBridge::FDInfFillPitsFn    FZigComputeBridge::DInfFillPits        = nullptr;
FZigComputeBridge::FDInfFlowDirFn     FZigComputeBridge::DInfFlowDirection   = nullptr;
FZigComputeBridge::FDInfFlowAccumFn   FZigComputeBridge::DInfFlowAccumulation= nullptr;
FZigComputeBridge::FLloydRelaxFn      FZigComputeBridge::LloydRelax          = nullptr;
FZigComputeBridge::FVoronoiAreasFn    FZigComputeBridge::VoronoiCellAreas    = nullptr;

// ---------------------------------------------------------------------------
// Resolve helper
// ---------------------------------------------------------------------------

template<typename T>
bool FZigComputeBridge::Resolve(T& FnPtr, const char* Symbol)
{
    void* Addr = FPlatformProcess::GetDllExport(LibHandle, ANSI_TO_TCHAR(Symbol));
    if (!Addr)
    {
        UE_LOG(LogMahlanyaPipeline, Error,
               TEXT("FZigComputeBridge: failed to resolve symbol '%s' from libmahlanya_compute.so"),
               ANSI_TO_TCHAR(Symbol));
        return false;
    }
    FnPtr = reinterpret_cast<T>(Addr);
    return true;
}

// ---------------------------------------------------------------------------
// Load
// ---------------------------------------------------------------------------

bool FZigComputeBridge::Load(const FString& LibPath)
{
    if (LibHandle)
    {
        UE_LOG(LogMahlanyaPipeline, Warning,
               TEXT("FZigComputeBridge::Load called but library is already loaded."));
        return true;
    }

    UE_LOG(LogMahlanyaPipeline, Log,
           TEXT("FZigComputeBridge: loading '%s'"), *LibPath);

    LibHandle = FPlatformProcess::GetDllHandle(*LibPath);
    if (!LibHandle)
    {
        UE_LOG(LogMahlanyaPipeline, Error,
               TEXT("FZigComputeBridge: GetDllHandle failed for '%s'"), *LibPath);
        return false;
    }

    // Resolve all 9 function pointers. Abort on first failure so the caller
    // knows the bridge is incomplete.
    bool bOk = true;
    bOk &= Resolve(ThermalErosionPass,   "mahlanya_thermal_erosion_pass");
    bOk &= Resolve(FluvialErosionPass,   "mahlanya_fluvial_erosion_pass");
    bOk &= Resolve(AeolianErosionPass,   "mahlanya_aeolian_erosion_pass");
    bOk &= Resolve(MassWastingPass,      "mahlanya_mass_wasting_pass");
    bOk &= Resolve(DInfFillPits,         "mahlanya_dinf_fill_pits");
    bOk &= Resolve(DInfFlowDirection,    "mahlanya_dinf_flow_direction");
    bOk &= Resolve(DInfFlowAccumulation, "mahlanya_dinf_flow_accumulation");
    bOk &= Resolve(LloydRelax,           "mahlanya_lloyd_relax");
    bOk &= Resolve(VoronoiCellAreas,     "mahlanya_voronoi_cell_areas");

    if (!bOk)
    {
        // Null out all pointers so IsLoaded() returns false
        Unload();
        return false;
    }

    UE_LOG(LogMahlanyaPipeline, Log,
           TEXT("FZigComputeBridge: all 9 symbols resolved successfully."));
    return true;
}

// ---------------------------------------------------------------------------
// Unload
// ---------------------------------------------------------------------------

void FZigComputeBridge::Unload()
{
    if (LibHandle)
    {
        FPlatformProcess::FreeDllHandle(LibHandle);
        LibHandle = nullptr;
        UE_LOG(LogMahlanyaPipeline, Log, TEXT("FZigComputeBridge: library unloaded."));
    }

    ThermalErosionPass   = nullptr;
    FluvialErosionPass   = nullptr;
    AeolianErosionPass   = nullptr;
    MassWastingPass      = nullptr;
    DInfFillPits         = nullptr;
    DInfFlowDirection    = nullptr;
    DInfFlowAccumulation = nullptr;
    LloydRelax           = nullptr;
    VoronoiCellAreas     = nullptr;
}

// ---------------------------------------------------------------------------
// IsLoaded
// ---------------------------------------------------------------------------

bool FZigComputeBridge::IsLoaded()
{
    return LibHandle != nullptr;
}
