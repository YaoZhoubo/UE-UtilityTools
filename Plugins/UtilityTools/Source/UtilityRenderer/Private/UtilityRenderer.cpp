#include "UtilityRenderer.h"


void FUtilityRendererModule::StartupModule()
{
	FString ShaderDirectory = FPaths::Combine(FPaths::ProjectPluginsDir(), TEXT("UtilityTools/Shaders/Private"));
	AddShaderSourceDirectoryMapping("/UtilityTools", ShaderDirectory);
}

void FUtilityRendererModule::ShutdownModule()
{
}

IMPLEMENT_MODULE(FUtilityRendererModule, UtilityRenderer)