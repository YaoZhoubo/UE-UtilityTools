#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SurfaceDrawer/BVHConfig.h"
#include "SurfacePolygonComponent.generated.h"


class  FSurfacePolygonSceneProxy;
struct FGPUPolygonData;
/**
 * @brief USurfacePolygon组件 - 用于多边形面贴地绘制
 * 
 * 负责：
 * 1. 管理多边形数据及其BVH空间加速结构构建
 * 2. 管理多边形面渲染参数配置（颜色、透明度等）
 * 3. 扩展渲染管线，进行贴地面绘制
 *
 * 用户仅需考虑使用SetTriangles提供三角形数据，将自动构建BVH空间加速结构，
 * 然后使用SetProperties设置渲染参数（颜色、不透明度等），将自动更新场景代理，
 */
UCLASS(HideCategories = (Cooking, AssetUserData, Navigation, Variable, ComponentReplication, Replication, Tags, Activation),
	ClassGroup = (SurfaceLine), BlueprintType, meta = (BlueprintSpawnableComponent))
class UTILITYTOOLS_API USurfacePolygonComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USurfacePolygonComponent();

	/// \brief 设置三角形数据
	UFUNCTION(BlueprintCallable, Category = "SurfacePolygonComponent")
	void SetTriangles(const TArray<FTriangle>& InTriangles);

	/// \brief 设置属性
	UFUNCTION(BlueprintCallable, Category = "SurfacePolygonComponent")
	void SetProperties(float InOpacity, const FLinearColor& InColor);

	/// \brief 设置不透明度
	UFUNCTION(BlueprintCallable, Category = "SurfacePolygonComponent")
	void SetOpacity(const float InOpacity);

	/// \brief 设置颜色
	UFUNCTION(BlueprintCallable, Category = "SurfacePolygonComponent")
	void SetColor(const FLinearColor& InColor);

	/// \brief 清空三角形数据
	UFUNCTION(BlueprintCallable, Category = "SurfacePolygonComponent")
	void ClearTriangles();

protected:
	//~ Begin UActorComponent Interface.
	virtual void OnRegister() override;
	virtual void OnUnregister() override;
	virtual void CreateRenderState_Concurrent(FRegisterComponentContext* Context) override;
	virtual void DestroyRenderState_Concurrent() override;
	virtual bool ShouldCreateRenderState() const override;
	//~ End UActorComponent Interface.

private:
	/// \brief 异步构建BVH数据
	void AsyncBuildBVHData(const TArray<FTriangle>& InTriangles);

	// 管理渲染代理
	void CreateSceneProxy();
	void UpdateSceneProxy();
	void DestroySceneProxy();

	void MarkGeometryDataDirty();

public:
	/// \brief BVH构建配置
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SurfacePolygonComponent")
	FBVHBuildConfig BVHBuildConfig;

	/// \brief BVH统计信息
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SurfacePolygonComponent")
	FBVHStats BVHStats;

private:
	/// \brief GPU 多边形面数据
	TSharedPtr<FGPUPolygonData> GPUPolygonData;

	/// \brief 场景代理
	TSharedPtr<FSurfacePolygonSceneProxy> SceneProxy;

	/// \brief 异步构建状态标志
	std::atomic<bool> IsAsyncBuilding;

	/// \brief 池化缓冲区状态标志
	bool bBuffersInitialized;

	/// \brief 颜色
	FLinearColor Color;

	/// \brief 不透明度
	float Opacity;
};