#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "BVHDrawer/BVHConfig.h"
#include "Engine/TextureRenderTarget2D.h"
#include "BVHComponent.generated.h"


class  FPolygonsSceneProxy;

/// \brief 数据纹理更新委托声明
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FiveParams(
	FAfterUpdateDataTexturesSignature,
	UTextureRenderTarget2D*, NewNodesDataTexture,
	UTextureRenderTarget2D*, NewSegmentsDataTexture,
	float, NewLineWidth,
	float, NewLineOpacity,
	FLinearColor, NewLineColor);

/**
 * @brief BVH加速组件 - 用于多边形线段绘制
 *
 * 主要功能：
 * 1. 管理多边形线段数据的BVH空间加速结构构建
 * 2. 将BVH数据转换为GPU友好的纹理格式
 * 3. 提供线段渲染参数配置（颜色、宽度、透明度等）
 * 4. 扩展渲染管线，进行贴地线段绘制
 *
 * 用户仅需考虑使用SetPolygons提供多边形数据，将自动构建BVH空间加速结构并渲染线段，
 * 线段渲染参数配置（颜色、宽度、透明度等）同理，
   注意，SetProperties、SetLineWidth等均会触发场景代理更新。
 *
 * 注：当前版本专门针对多边形线段数据进行优化，不支持其他图元类型。
 * 未来如果需要支持点、面等其他图元，需重新设计。
 */
UCLASS(ClassGroup = (BVH), BlueprintType, meta = (BlueprintSpawnableComponent))
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

	/// \brief 设置多边形数据
	UFUNCTION(BlueprintCallable, Category = "BVHComponent")
	void SetPolygons(const TArray<FPolygon>& InPolygons);

	/// \brief 设置线段属性
	UFUNCTION(BlueprintCallable, Category = "BVHComponent")
	void SetProperties(float InLineWidth, float InLineOpacity, const FLinearColor& InLineColor);

	/// \brief 设置线段宽度
	UFUNCTION(BlueprintCallable, Category = "BVHComponent")
	void SetLineWidth(const float InLineWidth);

	/// \brief 设置线段透明度
	UFUNCTION(BlueprintCallable, Category = "BVHComponent")
	void SetLineOpacity(const float InLineOpacity);

	/// \brief 设置线段颜色
	UFUNCTION(BlueprintCallable, Category = "BVHComponent")
	void SetLineColor(const FLinearColor& InLineColor);

	/// \brief 清空多边形数据
	UFUNCTION(BlueprintCallable, Category = "BVHComponent")
	void ClearPolygons();

	/// \brief 生成随机测试数据（用于测试）
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "BVHComponent")
	void GenerateRandomTestData();

public:
	/// \brief BVH构建配置
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BVHComponent")
	FBVHBuildConfig BuildConfig;

	/// \brief BVH统计信息
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "BVHComponent")
	FBVHStats BVHStats;

	/// \brief 自定义纹理（暂时仅支持对称纹理）
	UPROPERTY(EditAnywhere, Category = "BVHComponent")
	TObjectPtr<UTexture2D> CustomTexture;

	/// \brief 数据纹理更新后触发
	UPROPERTY(BlueprintAssignable, Category = "BVHComponent")
	FAfterUpdateDataTexturesSignature AfterBuildBVHData;

	// 随机测试数据生成参数
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RandomTest")
	int32 RandomPolygonCount = 20;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RandomTest")
	int32 RandomPointsPerPolygon = 10;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RandomTest")
	float RandomGenerationRadius = 1000.0f;

private:
	/// \brief 异步构建BVH数据
	void AsyncBuildBVHData();

	/// \brief 创建默认BVH纹理
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

	FPolygonsSceneProxy* PolygonsSceneProxy = nullptr;

	std::atomic<bool> IsAsyncBuilding;
};