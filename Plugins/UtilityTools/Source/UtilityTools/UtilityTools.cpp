// Copyright Epic Games, Inc. All Rights Reserved.

#include "UtilityTools.h"

#include "Shaders/MyShader.h"
#include "Interfaces/IPluginManager.h"

#define LOCTEXT_NAMESPACE "FUtilityToolsModule"

void FUtilityToolsModule::StartupModule()
{
	FString PluginShaderDir = FPaths::Combine(IPluginManager::Get().FindPlugin(TEXT("UtilityTools"))->GetBaseDir(), TEXT("Shaders"));
	AddShaderSourceDirectoryMapping(TEXT("/UtilityTools/Shaders"), PluginShaderDir);
}

void FUtilityToolsModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FUtilityToolsModule, UtilityTools)