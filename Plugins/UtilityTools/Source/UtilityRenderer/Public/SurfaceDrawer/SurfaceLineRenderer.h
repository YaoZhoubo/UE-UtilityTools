#pragma once

#include "CoreMinimal.h"
#include "RenderGraphResources.h"

#include "SurfaceLineBuilder.h"


class FSurfaceLineRenderManager;
/**
 * @brief SurfaceLine场景代理
 *
 * 作为USurfaceLineComponent的渲染代理，保存其渲染相关的参数
 * 
 * TODO: 如果参数较多，将Proxy放到公共文件，直接用对应的Component来传递参数
 */
class FSurfaceLineSceneProxy
{
public:
	FSurfaceLineSceneProxy() 
		: GPULineData(nullptr)
		, CustomTexture(nullptr)
		, LineWidth(0.0f)
		, LineOpacity(0.0f)
		, bUseCustomTexture(false)
		, bUsePixelUnit(false)
		, ProxyId(0)
		, bBuffersInitialized(false)
	{
	}
	
	FSurfaceLineSceneProxy(
		const TSharedPtr<FGPULineData>& InGPULineData,
		UTexture2D* InCustomTexture,
		float InLineWidth, 
		float InLineOpacity,
		FLinearColor InLineColor,
		bool InbUseCustomTexture,
		bool InbUsePixelUnit
	)
		: GPULineData(InGPULineData)
		, CustomTexture(InCustomTexture)
		, LineWidth(InLineWidth)
		, LineOpacity(InLineOpacity)
		, LineColor(InLineColor)
		, bUseCustomTexture(InbUseCustomTexture)
		, bUsePixelUnit(InbUsePixelUnit)
		, bBuffersInitialized(false)
	{
	}
	
	/// \brief 更新参数
	void UpdateParameters_RenderThread(
		const TSharedPtr<FGPULineData>& InGPULineData,
		UTexture2D* InCustomTexture,
		float InLineWidth, 
		float InLineOpacity, 
		const FLinearColor& InLineColor,
		bool InbUseCustomTexture,
		bool InbUsePixelUnit,
		bool InbBuffersInitialized)
	{
		check(IsInRenderingThread());
	
		GPULineData = InGPULineData;
		CustomTexture = InCustomTexture;
		LineWidth = InLineWidth;
		LineOpacity = InLineOpacity;
		LineColor = InLineColor;
		bUseCustomTexture = InbUseCustomTexture;
		bUsePixelUnit = InbUsePixelUnit;
		bBuffersInitialized = InbBuffersInitialized;
	}
	
	/// \brief 重置参数，释放资源引用
	void Reset()
	{
		GPULineData.Reset();
		CustomTexture = nullptr;
		LineWidth = 0.0f;
		LineOpacity = 0.0f;
		LineColor = FLinearColor::Black;
		bUseCustomTexture = false;
		bUsePixelUnit = false;
	}

	/// \brief 获取代理ID
	uint32 GetProxyId() const { return ProxyId; }

private:
	TSharedPtr<FGPULineData> GPULineData;
	UTexture2D* CustomTexture;
	float LineWidth;
	float LineOpacity;
	FLinearColor LineColor;
	bool bUseCustomTexture;
	bool bUsePixelUnit;

	uint32 ProxyId; ///< 唯一标识符
	
	// 池化缓冲区管理
	bool bBuffersInitialized;
	TRefCountPtr<FRDGPooledBuffer> BVHNodesPooledBuffer;
	TRefCountPtr<FRDGPooledBuffer> ClustersPooledBuffer;
	TRefCountPtr<FRDGPooledBuffer> SegmentsPooledBuffer;
	void InitializePooledBuffers(FRDGBuilder& GraphBuilder);
	void ReleasePooledBuffers();

	friend FSurfaceLineRenderManager;
};
	
/**
	* @brief SurfaceLine渲染管理器 - 单例
	*
	* 负责：
	* 1. 注册/注销场景代理
	* 2. 管理渲染委托的挂接和移除
	* 3. 添加渲染Pass
	*/
class UTILITYRENDERER_API FSurfaceLineRenderManager
{
public:
	static FSurfaceLineRenderManager* Get()
	{
		if (!Instance)
		{
			Instance = new FSurfaceLineRenderManager();
		}
		return Instance;
	};
	
	// 注册/注销组件
	void RegisterSceneProxy(const TSharedPtr<FSurfaceLineSceneProxy>& InSceneProxy);
	void UnregisterSceneProxy(uint32 ProxyId);

	/// \brief 获取当前注册的代理数量
	int32 GetNumSceneProxies();
	
private:
	/// \brief 私有构造
	FSurfaceLineRenderManager() = default;
	~FSurfaceLineRenderManager();
	
	/// \brief 禁止拷贝
	FSurfaceLineRenderManager(const FSurfaceLineRenderManager&) = delete;
	FSurfaceLineRenderManager& operator=(const FSurfaceLineRenderManager&) = delete;

	/// \brief 将 Execute_RenderThread 挂接到渲染器。
	void BeginRendering();

	/// \brief 从渲染器移除 Execute_RenderThread
	void EndRendering();

	/// \brief 渲染线程执行函数
	void Execute_RenderThread(FPostOpaqueRenderParameters& Parameters);

private:
	/// \brief 单例实例
	static FSurfaceLineRenderManager* Instance;
	
	/// \brief 渲染委托句柄
	FDelegateHandle OnOverlayRenderHandle;

	/// \brief 场景代理映射表（ProxyId -> SceneProxy）
	TMap<uint32, TSharedPtr<FSurfaceLineSceneProxy>> SceneProxyMap;
	uint32 NextProxyId = 0; ///< 可以反映已经注册过的代理总数（包括已经注销的）

	FCriticalSection RenderThreadLock;
};