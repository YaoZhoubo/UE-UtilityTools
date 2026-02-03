#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/TextureRenderTarget2D.h"
#include "SurfaceDrawer/BVHConfig.h"
#include "SurfaceLineComponent.generated.h"

	
class FSurfaceLineSceneProxy;
struct FGPULineData;

/**
 * @brief SurfaceLine组件 - 用于多边形线段贴地绘制
 *
 * 负责：
 * 1. 管理多边形线段数据及其BVH空间加速结构构建
 * 2. 管理多边形线段渲染参数配置（颜色、宽度、透明度等）
 * 3. 扩展渲染管线，进行贴地线段绘制
 *
 * 用户仅需考虑使用SetPolygons提供多边形数据，将自动构建BVH空间加速结构，
 * 然后使用SetProperties设置线段渲染参数（颜色、宽度、透明度等），将自动更新场景代理，
 */
UCLASS(HideCategories = (Cooking, AssetUserData, Navigation, Variable, ComponentReplication, Replication, Tags, Activation),
	ClassGroup = (SurfaceLine), BlueprintType, meta = (BlueprintSpawnableComponent))
class UTILITYTOOLS_API USurfaceLineComponent : public UActorComponent
{
	GENERATED_BODY()
	
public:
	USurfaceLineComponent();
	
	/// \brief 设置线段数据
	UFUNCTION(BlueprintCallable, Category = "SurfaceLineComponent")
	void SetPolygons(const TArray<FPolygon>& InPolygons);
	
	/// \brief 设置属性
	UFUNCTION(BlueprintCallable, Category = "SurfaceLineComponent")
	void SetProperties(float InLineWidth, float InLineOpacity, const FLinearColor& InLineColor);
	
	/// \brief 设置线段宽度
	UFUNCTION(BlueprintCallable, Category = "SurfaceLineComponent")
	void SetLineWidth(const float InLineWidth);
	
	/// \brief 设置线段透明度
	UFUNCTION(BlueprintCallable, Category = "SurfaceLineComponent")
	void SetLineOpacity(const float InLineOpacity);
	
	/// \brief 设置线段颜色
	UFUNCTION(BlueprintCallable, Category = "SurfaceLineComponent")
	void SetLineColor(const FLinearColor& InLineColor);
	
	/// \brief 清空多边形数据
	UFUNCTION(BlueprintCallable, Category = "SurfaceLineComponent")
	void ClearPolygons();
	
public:
	/// \brief BVH构建配置
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SurfaceLineComponent")
	FBVHBuildConfig BVHBuildConfig;
	
	/// \brief BVH统计信息
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SurfaceLineComponent")
	FBVHStats BVHStats;
	
	/// \brief 线宽
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SurfaceLineComponent")
	float LineWidth;
	
	/// \brief 线不透明度
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SurfaceLineComponent")
	float LineOpacity;
	
	/// \brief 线颜色
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SurfaceLineComponent")
	FLinearColor LineColor;
	
	/// \brief 是否使用自定义纹理
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SurfaceLineComponent")
	bool bUseCustomTexture;
	
	/// \brief 是否使用像素单位宽度
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SurfaceLineComponent")
	bool bUsePixelUnit;
	
	/// \brief 自定义纹理
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SurfaceLineComponent")
	TObjectPtr<UTexture2D> CustomTexture;
	
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
	void AsyncBuildBVHData(const TArray<FPolygon>& InPolygons);
	
	// 管理渲染代理
	void CreateSceneProxy();
	void UpdateSceneProxy();
	void DestroySceneProxy();

	void MarkGeometryDataDirty();
	
private:
	/// \brief BVH节点数据纹理
	TSharedPtr<FGPULineData> GPULineData;
	
	/// \brief 场景代理
	TSharedPtr<FSurfaceLineSceneProxy> SceneProxy;
	
	/// \brief 异步构建状态标志
	std::atomic<bool> IsAsyncBuilding;

	/// \brief 池化缓冲区状态标志
	bool bBuffersInitialized;
};
