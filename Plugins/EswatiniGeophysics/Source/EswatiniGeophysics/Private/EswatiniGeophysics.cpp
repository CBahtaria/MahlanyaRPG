// Copyright Charles Bartaria / BRT Inc. All Rights Reserved.

#include "Modules/ModuleManager.h"

class FEswatiniGeophysicsModule : public IModuleInterface
{
public:
    virtual void StartupModule() override {}
    virtual void ShutdownModule() override {}
};

IMPLEMENT_MODULE(FEswatiniGeophysicsModule, EswatiniGeophysics)
