#include "MyShaders.h"
 
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "RHI.h"
#include "GlobalShader.h"
#include "RHICommandList.h"
#include "RenderGraphBuilder.h"
#include "RenderTargetPool.h"
#include "Runtime/Core/Public/Modules/ModuleManager.h"
#include "Interfaces/IPluginManager.h"
 
#define LOCTEXT_NAMESPACE "MyShadersModule"
 
void FMyShadersModule::StartupModule()
{
	// 模块加载到内存后，此代码将执行；具体执行时间在每个模块的.uplugin文件中指定
	// 将虚拟着色器源目录映射到插件的实际着色器目录。
	FString PluginShaderDir = FPaths::Combine(IPluginManager::Get().FindPlugin(TEXT("ComputeShaders"))->GetBaseDir(), TEXT("Shaders/Private"));
	AddShaderSourceDirectoryMapping(TEXT("/MyShaders"), PluginShaderDir);
}
 
void FMyShadersModule::ShutdownModule()
{
}
 
#undef LOCTEXT_NAMESPACE
 
IMPLEMENT_MODULE(FMyShadersModule, MyShaders)