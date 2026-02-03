#pragma once

#include "CoreMinimal.h"
#include "RenderGraphResources.h"

#include "SurfacePolygonBuilder.h"


/**
 * @brief SurfacePolygon场景代理
 * 
 * 作为USurfacePolygonComponent的渲染代理，保存其渲染相关的参数
 */
class FSurfacePolygonSceneProxy
{
public:
	FSurfacePolygonSceneProxy()
		: GPUPolygonData(nullptr)
		, Opacity(0.0f)
		, Color(FLinearColor::Black)
		, ProxyId(0)
		, bBuffersInitialized(false)
	{
	}

	FSurfacePolygonSceneProxy(
		const TSharedPtr<FGPUPolygonData>& InGPUPolygonData,
		float InOpacity,
		const FLinearColor& InColor
	)
		: GPUPolygonData(InGPUPolygonData)
		, Opacity(InOpacity)
		, Color(InColor)
		, bBuffersInitialized(false)
	{
	}

	/// \brief 更新参数
	void UpdateParameters_RenderThread(
		const TSharedPtr<FGPUPolygonData>& InGPUPolygonData,
		float InOpacity,
		const FLinearColor& InColor,
		bool InbBuffersInitialized)
	{
		check(IsInRenderingThread());

		GPUPolygonData = InGPUPolygonData;
		Opacity = InOpacity;
		Color = InColor;
		bBuffersInitialized = InbBuffersInitialized;
	}

	/// \brief 重置参数，释放资源引用
	void Reset()
	{
		GPUPolygonData.Reset();
		Opacity = 0.0f;
		Color = FLinearColor::Black;
	}

	/// \brief 获取代理ID
	uint32 GetProxyId() const { return ProxyId; }

private:
	TSharedPtr<FGPUPolygonData> GPUPolygonData;
	float Opacity;
	FLinearColor Color;

	uint32 ProxyId; ///< 唯一标识符

	// 池化缓冲区管理
	bool bBuffersInitialized;
	TRefCountPtr<FRDGPooledBuffer> BVHNodesPooledBuffer;
	TRefCountPtr<FRDGPooledBuffer> TrianglesPooledBuffer;
	void InitializePooledBuffers(FRDGBuilder& GraphBuilder);
	void ReleasePooledBuffers();

	friend class FSurfacePolygonRenderManager;
};

/**
 * @brief SurfacePolygon渲染管理器 - 单例
 *
 * 负责：
 * 1. 注册/注销场景代理
 * 2. 管理渲染委托的挂接和移除
 * 3. 添加渲染Pass
 */
class UTILITYRENDERER_API FSurfacePolygonRenderManager
{
public:
	static FSurfacePolygonRenderManager* Get()
	{
		if (!Instance)
		{
			Instance = new FSurfacePolygonRenderManager();
		}
		return Instance;
	};

	// 注册/注销组件
	void RegisterSceneProxy(const TSharedPtr<FSurfacePolygonSceneProxy>& InSceneProxy);
	void UnregisterSceneProxy(uint32 ProxyId);

	/// \brief 获取当前注册的代理数量
	int32 GetNumSceneProxies();

private:
	/// \brief 私有构造
	FSurfacePolygonRenderManager() = default;
	~FSurfacePolygonRenderManager();

	/// \brief 禁止拷贝
	FSurfacePolygonRenderManager(const FSurfacePolygonRenderManager&) = delete;
	FSurfacePolygonRenderManager& operator=(const FSurfacePolygonRenderManager&) = delete;

	/// \brief 将 Execute_RenderThread 挂接到渲染器。
	void BeginRendering();

	/// \brief 从渲染器移除 Execute_RenderThread
	void EndRendering();

	/// \brief 渲染线程执行函数
	void Execute_RenderThread(FPostOpaqueRenderParameters& Parameters);

private:
	/// \brief 单例实例
	static FSurfacePolygonRenderManager* Instance;

	/// \brief 渲染委托句柄
	FDelegateHandle OnOverlayRenderHandle;

	/// \brief 场景代理映射表（ProxyId -> SceneProxy）
	TMap<uint32, TSharedPtr<FSurfacePolygonSceneProxy>> SceneProxyMap;
	uint32 NextProxyId = 0; ///< 可以反映已经注册过的代理总数（包括已经注销的）

	FCriticalSection RenderThreadLock;
};