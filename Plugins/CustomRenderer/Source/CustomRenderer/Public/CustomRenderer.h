// Copyright(c) 2025 YaoZhoubo. All rights reserved.

#pragma once

#include "Modules/ModuleManager.h"

class FCustomRendererModule : public IModuleInterface
{
public:

	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
