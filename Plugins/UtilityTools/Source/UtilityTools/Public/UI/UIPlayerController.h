// Copyright (c) 2025 YaoZhoubo. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "UIPlayerController.generated.h"

/**
 * 
 */
UCLASS()
class UTILITYTOOLS_API AUIPlayerController : public APlayerController
{
	GENERATED_BODY()
	
public:
	//~ Begin AController Interface
	virtual void OnPossess(APawn* InPawn) override;
	//~ End AController Interface
};
