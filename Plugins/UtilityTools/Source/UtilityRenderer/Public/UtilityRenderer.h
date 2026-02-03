#pragma once

#include "Modules/ModuleManager.h"

class FUtilityRendererModule : public IModuleInterface
{
public:

	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
