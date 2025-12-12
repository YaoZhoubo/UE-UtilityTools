// Copyright(c) 2025 YaoZhoubo. All rights reserved.

#include "CustomRenderer.h"

#define LOCTEXT_NAMESPACE "FCustomRendererModule"

void FCustomRendererModule::StartupModule()
{
	FString ShaderDirectory = FPaths::Combine(FPaths::ProjectPluginsDir(), TEXT("CustomRenderer/Shaders/Private"));
	AddShaderSourceDirectoryMapping("/CustomRenderer", ShaderDirectory);
}

void FCustomRendererModule::ShutdownModule()
{
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FCustomRendererModule, CustomRenderer)