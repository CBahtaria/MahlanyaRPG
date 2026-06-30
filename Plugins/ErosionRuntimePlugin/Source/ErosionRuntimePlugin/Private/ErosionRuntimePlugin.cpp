// Copyright Charles Bartaria / BRT Inc. All Rights Reserved.

#include "Modules/ModuleManager.h"
#include "Interfaces/IPluginManager.h"
#include "ShaderCore.h"

class FErosionRuntimePluginModule : public IModuleInterface
{
public:
    virtual void StartupModule() override
    {
        // Map /Plugin/ErosionRuntimePlugin/ → Plugins/ErosionRuntimePlugin/Shaders/
        // so IMPLEMENT_GLOBAL_SHADER can locate ErosionCS.usf at startup.
        FString ShaderDir = FPaths::Combine(
            IPluginManager::Get().FindPlugin(TEXT("ErosionRuntimePlugin"))->GetBaseDir(),
            TEXT("Shaders"));
        AddShaderSourceDirectoryMapping(TEXT("/Plugin/ErosionRuntimePlugin"), ShaderDir);
    }

    virtual void ShutdownModule() override {}
};

IMPLEMENT_MODULE(FErosionRuntimePluginModule, ErosionRuntimePlugin)
