// Copyright(c) 2025 YaoZhoubo. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "BVHDrawer/BVHAccelerator.h"
#include "BVHDrawer/BVHConfig.h"
#include "Engine/TextureRenderTarget2D.h"
#include "BVHComponent.generated.h"


class  FPolygonsSceneProxy;

/// \brief BVH更新委托声明
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FiveParams(
	FAfterUpdateDataTexturesSignature,
	UTextureRenderTarget2D*, NewNodesDataTexture,
	UTextureRenderTarget2D*, NewSegmentsDataTexture,
	float, NewLineWidth,
	float, NewLineOpacity,
	FLinearColor, NewLineColor);

 /**
  * @brief BVH加速组件
  *
  * 用于管理和构建多边形数据的BVH空间加速结构
  * 负责将多边形数据转换为GPU友好的纹理格式
  */
UCLASS( ClassGroup=(BVH), BlueprintType, 
	meta = (BlueprintSpawnableComponent) )
class UTILITYTOOLS_API UBVHComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	UBVHComponent();

	//~ Begin UActorComponent Interface.
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void CreateRenderState_Concurrent(FRegisterComponentContext* Context) override;
	virtual void DestroyRenderState_Concurrent() override;
	virtual bool ShouldCreateRenderState() const override { return true; }
	//~ End UActorComponent Interface.

	//~ Begin UObject  Interface.
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
	//~ End UObject  Interface.

	UFUNCTION(BlueprintCallable, Category = "BVHComponent")
	void SetPolygons(const TArray<FPolygon>& InPolygons);

	UFUNCTION(BlueprintCallable, Category = "BVHComponent")
	void SetLineWidth(const float InLineWidth);

	UFUNCTION(BlueprintCallable, Category = "BVHComponent")
	void SetLineOpacity(const float InLineOpacity);

	UFUNCTION(BlueprintCallable, Category = "BVHComponent")
	void SetLineColor(const FLinearColor& InLineColor);

	UFUNCTION(BlueprintCallable, Category = "BVHComponent")
	void ClearPolygons();

	UFUNCTION(BlueprintCallable, CallInEditor, Category = "BVHComponent")
	void GenerateRandomTestData();

public:
	/// \brief BVH构建配置
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BVHComponent")
	FBVHBuildConfig BuildConfig;

	/// \brief BVH统计信息
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "BVHComponent")
	FBVHStats BVHStats;

	UPROPERTY(EditAnywhere, Category = "BVHComponent")
	TObjectPtr<UTexture2D> CustomTexture;

	/// \brief 数据纹理更新后触发
	UPROPERTY(BlueprintAssignable, Category = "BVHComponent")
	FAfterUpdateDataTexturesSignature AfterBuildBVHData;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RandomTest")
	int32 RandomPolygonCount = 5;								  
																  
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RandomTest")
	int32 RandomPointsPerPolygon = 10;							  
																  
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RandomTest")
	float RandomGenerationRadius = 1000.0f;

private:
	void AsyncBuildBVHData();

	UTextureRenderTarget2D* CreateDefaultTextureForBVH(const FString& TextureName);

	// 管理渲染代理
	void CreateSceneProxy();
	void UpdateSceneProxy();
	void DestroySceneProxy();

private:
	UPROPERTY()
	TArray<FPolygon> Polygons;

	UPROPERTY(VisibleAnywhere, Category = "BVHComponent")
	TObjectPtr<UTextureRenderTarget2D> NodesDataTexture;

	UPROPERTY(VisibleAnywhere, Category = "BVHComponent")
	TObjectPtr<UTextureRenderTarget2D> SegmentsDataTexture;

	UPROPERTY(VisibleAnywhere, Category = "BVHComponent")
	float LineWidth;

	UPROPERTY(VisibleAnywhere, Category = "BVHComponent")
	float LineOpacity;

	UPROPERTY(VisibleAnywhere, Category = "BVHComponent")
	FLinearColor LineColor;

	FPolygonsSceneProxy* RenderParameters = nullptr;
};