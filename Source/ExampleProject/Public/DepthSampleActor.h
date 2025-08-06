// Copyright (c) 2025 YaoZhoubo. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DepthSampleActor.generated.h"

class USceneCaptureComponent2D;

UCLASS()
class EXAMPLEPROJECT_API ADepthSampleActor : public AActor
{
	GENERATED_BODY()
	
public:	
	ADepthSampleActor();
	virtual void BeginPlay() override;
	virtual void BeginDestroy() override;
	virtual void Tick(float DeltaTime) override;

public:
	UPROPERTY()
	USceneComponent* Root;

	UPROPERTY(EditAnywhere)
	UStaticMeshComponent* StaticMesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = ShaderDemo)
	class UTextureRenderTarget2D* RenderTarget;

	UPROPERTY(EditAnywhere)
	uint32 TimeStamp;
};
