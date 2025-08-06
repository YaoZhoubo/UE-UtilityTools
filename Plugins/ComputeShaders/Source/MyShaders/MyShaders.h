#pragma once
 
#include "CoreMinimal.h"
 
#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"
 
class MYSHADERS_API FMyShadersModule : public IModuleInterface
{
public:
	static inline FMyShadersModule& Get()
	{
		return FModuleManager::LoadModuleChecked<FMyShadersModule>("FMyShadersModule");
	}

	static inline bool IsAvailable()
	{
		return FModuleManager::Get().IsModuleLoaded("FMyShadersModule");
	}

public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};