// Copyright (c) 2025 YaoZhoubo. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SumCalculateShaderUseActor.generated.h"


class UTextureRenderTarget2D;

UCLASS()
class EXAMPLEPROJECT_API ASumCalculateShaderUseActor : public AActor
{
	GENERATED_BODY()
	
public:	
	ASumCalculateShaderUseActor();
	virtual void BeginPlay() override;
	virtual void BeginDestroy() override;
	virtual void Tick(float DeltaTime) override;

	UFUNCTION(CallInEditor)
	void UpdateParams();

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	UTextureRenderTarget2D* InputTexture;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	class USceneCaptureComponent2D* CaptureComponent;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float Value1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float Value2;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<float> ResultArray;
};
