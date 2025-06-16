// Copyright (c) 2025 YaoZhoubo. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "UseShaderActor.generated.h"

UCLASS()
class UTILITYTOOLS_API AUseShaderActor : public AActor
{
	GENERATED_BODY()
	
public:	
	AUseShaderActor();

protected:
	virtual void BeginPlay() override;

public:	
	virtual void Tick(float DeltaTime) override;

};
