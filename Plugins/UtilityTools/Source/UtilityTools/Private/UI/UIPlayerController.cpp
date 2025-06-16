// Copyright (c) 2025 YaoZhoubo. All rights reserved.

#include "UI/UIPlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Camera/CameraActor.h"

void AUIPlayerController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	TArray<AActor*> CameraActors;
	UGameplayStatics::GetAllActorsOfClassWithTag(this, ACameraActor::StaticClass(), FName("Default"), CameraActors);

	if (!CameraActors.IsEmpty())
	{
		SetViewTarget(CameraActors[0]);
	}
}
