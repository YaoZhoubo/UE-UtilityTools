// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "VisibilityAnalysisActor.generated.h"

DECLARE_LOG_CATEGORY_CLASS(AVisibilityAnalysisActorLog, Log, All);

UCLASS(HideCategories = (Tick, Replication, Rendering, Collision, Actor,
	Input, HLOD, Physics, WorldPartition, Cooking, DataLayers, Networking, LevelInstance))
class UTILITYTOOLS_API AVisibilityAnalysisActor : public AActor
{
	GENERATED_BODY()
	
public:	
	AVisibilityAnalysisActor();

	virtual void Tick(float DeltaTime) override;

	UFUNCTION(BlueprintCallable)
	void UpdateParams(float DeltaTime = 0);

	UFUNCTION(BlueprintCallable)
	void DrawFrustum();

public:
	// base params
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = 256, ClampMax = 32768))
	float Distance;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = 10, ClampMax = 100))
	int32 FOV;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = 0.1, ClampMax = 100))
	float DepthError;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = 0.5, ClampMax = 2))
	float AspectRatio;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = 128, ClampMax = 2048))
	int32 DepthCaptureResolution;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = 0, ClampMax = 1))
	float Opacity;

	// debug frustum
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Frustum, meta = (AllowPrivateAccess = "true"))
	uint32 bOpenDebugFrustum : 1;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Frustum, meta = (AllowPrivateAccess = "true"))
	uint32 bOpenDebugInEdit : 1;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Frustum, meta = (AllowPrivateAccess = "true", ClampMin = 0.0, ClampMax = 1000.0))
	float FrustumNear;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Frustum, meta = (AllowPrivateAccess = "true", ClampMin = 0.0, ClampMax = 5.0))
	float FrustumLineThickness;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Frustum, meta = (AllowPrivateAccess = "true"))
	uint8 FrustumLineDepthPriority;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Frustum, meta = (AllowPrivateAccess = "true"))
	FColor FrustumColor;

protected:
	virtual void BeginPlay() override;
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void Destroyed() override;

private:
#if WITH_EDITOR
	class UMaterialExpression* CreateMaterialExpression(UMaterial* Material, TSubclassOf<UMaterialExpression> ExpressionClass);
	void CreateMaterial();
#endif // WITH_EDITOR

private:
	UPROPERTY()
	class USceneCaptureComponent2D* DepthCapture;
	UPROPERTY()
	class UDecalComponent* VisibilityDecal;
	UPROPERTY()
	class UMaterial* VisibilityMaterial;
	UPROPERTY()
	class UMaterialInstanceDynamic* VisibilityMID;
};