#include "SurfaceDrawer/SurfacePolygonRenderer.h"

#include "GlobalShader.h"
#include "ShaderParameterStruct.h"
#include "ShaderParameterMacros.h"
#include "RHICommandList.h"
#include "RenderingThread.h"
#include "Modules/ModuleManager.h"
#include "RendererInterface.h"
#include "SceneView.h"
#include "SceneInterface.h"
#include "RenderGraphUtils.h"
#include "PixelShaderUtils.h"
#include "RenderGraphEvent.h"

#include "../Private/SceneRendering.h"


class FSurfacePolygonRenderPS : public FGlobalShader
{
public:
	// 声明全局着色器
	DECLARE_GLOBAL_SHADER(FSurfacePolygonRenderPS);

	// 告诉引擎此着色器使用结构作为其参数
	SHADER_USE_PARAMETER_STRUCT(FSurfacePolygonRenderPS, FGlobalShader);

	// 着色器参数结构声明
	// 参数与HLSL代码中的参数匹配
	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D, DepthTexture)
		SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D, ColorTexture)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<FGPUPolygonBVHNode>, PolygonBVHNodeData)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<FGPUTriangle>, TriangleData)
		SHADER_PARAMETER(FMatrix44f, ScreenToWorld)
		SHADER_PARAMETER(FMatrix44f, InvViewMatrix)
		SHADER_PARAMETER(FIntRect, ViewportRect)
		SHADER_PARAMETER(float, Opacity)
		SHADER_PARAMETER(FVector4f, Color)
		RENDER_TARGET_BINDING_SLOTS()
	END_SHADER_PARAMETER_STRUCT()

public:
	// 由引擎调用，以确定为此着色器编译哪些变体
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return true;
	}
};

// 创建着色器			着色器类				着色器文件位置									着色器入口函数名  着色器类型
IMPLEMENT_GLOBAL_SHADER(FSurfacePolygonRenderPS, "/UtilityTools/SurfacePolygonRenderShader.usf", "MainPixelShader", SF_Pixel);

// 着色器管理器实例初始化
FSurfacePolygonRenderManager* FSurfacePolygonRenderManager::Instance = nullptr;

FSurfacePolygonRenderManager::~FSurfacePolygonRenderManager()
{
	// 确保渲染结束
	EndRendering();
}

void FSurfacePolygonRenderManager::RegisterSceneProxy(const TSharedPtr<FSurfacePolygonSceneProxy>& InSceneProxy)
{
	if (InSceneProxy.IsValid())
	{
		FScopeLock Lock(&RenderThreadLock);

		// 分配唯一ID
		uint32 NewProxyId = NextProxyId++;
		InSceneProxy->ProxyId = NewProxyId;

		// 检查是否是第一个代理
		bool bWasEmpty = SceneProxyMap.IsEmpty();

		SceneProxyMap.Add(NewProxyId, InSceneProxy);

		// 如果是第一个代理，开始渲染
		if (bWasEmpty)
		{
			BeginRendering();
		}
	}
}

void FSurfacePolygonRenderManager::UnregisterSceneProxy(uint32 ProxyId)
{
	TSharedPtr<FSurfacePolygonSceneProxy> SceneProxyToRemove;
	bool bMapIsEmpty = false;

	{
		FScopeLock Lock(&RenderThreadLock);

		if (SceneProxyMap.RemoveAndCopyValue(ProxyId, SceneProxyToRemove))
		{
			bMapIsEmpty = SceneProxyMap.IsEmpty();
		}
		else
		{
			bMapIsEmpty = SceneProxyMap.IsEmpty();
		}
	}

	if (SceneProxyToRemove.IsValid())
	{
		// 在渲染线程释放资源
		ENQUEUE_RENDER_COMMAND(ReleaseSurfacePolygonResources)(
			[SceneProxyToRemove](FRHICommandList& RHICmdList)
			{
				SceneProxyToRemove->ReleasePooledBuffers();
			});
	}

	// 如果Map为空，停止渲染
	if (bMapIsEmpty)
	{
		EndRendering();
	}
}

int32 FSurfacePolygonRenderManager::GetNumSceneProxies()
{
	FScopeLock Lock(&RenderThreadLock);

	return SceneProxyMap.Num();
}

void FSurfacePolygonRenderManager::BeginRendering()
{
	// 如果已经注册回调，直接返回
	if (OnOverlayRenderHandle.IsValid())
	{
		return;
	}

	// 获取渲染模块，将 Execute_RenderThread 注册到OverlayRender阶段
	const FName RendererModuleName("Renderer");
	IRendererModule* RendererModule = FModuleManager::GetModulePtr<IRendererModule>(RendererModuleName);
	if (RendererModule)
	{
		OnOverlayRenderHandle = RendererModule->RegisterOverlayRenderDelegate(
			FPostOpaqueRenderDelegate::CreateRaw(this, &FSurfacePolygonRenderManager::Execute_RenderThread)
		);
	}
}

void FSurfacePolygonRenderManager::EndRendering()
{
	if (!OnOverlayRenderHandle.IsValid())
	{
		return;
	}

	const FName RendererModuleName("Renderer");
	IRendererModule* RendererModule = FModuleManager::GetModulePtr<IRendererModule>(RendererModuleName);
	if (RendererModule)
	{
		RendererModule->RemoveOverlayRenderDelegate(OnOverlayRenderHandle);
	}

	OnOverlayRenderHandle.Reset();
}

void FSurfacePolygonRenderManager::Execute_RenderThread(FPostOpaqueRenderParameters& Parameters)
{
	// 检查是否在渲染线程
	check(IsInRenderingThread());

	// 仅在Game和PIE中渲染
	const FSceneView* SceneView = static_cast<const FSceneView*>(Parameters.View);
	const FSceneInterface* Scene = SceneView->Family->Scene;
	if (UWorld* World = Scene->GetWorld())
	{
		if (!(World->WorldType == EWorldType::Game || World->WorldType == EWorldType::PIE))
		{
			return;
		}
	}

	// 复制SceneProxyMap为本地Proxies
	FScopeLock Lock(&RenderThreadLock);
	if (SceneProxyMap.IsEmpty())
	{
		return;
	}
	TArray<TSharedPtr<FSurfacePolygonSceneProxy>> ProxiesToRender;
	for (auto& ProxyPair : SceneProxyMap)
	{
		if (ProxyPair.Value.IsValid())
		{
			ProxiesToRender.Add(ProxyPair.Value);
		}
	}
	Lock.Unlock();

	// 为每个场景代理创建渲染Pass
	for (TSharedPtr<FSurfacePolygonSceneProxy> LocalSceneProxy : ProxiesToRender)
	{ 
		if (!LocalSceneProxy.IsValid() || !LocalSceneProxy->GPUPolygonData.IsValid() || !LocalSceneProxy->GPUPolygonData->IsValid())
		{
			continue;
		}

		FRDGBuilder& GraphBuilder = *Parameters.GraphBuilder;

		// 初始化持久化缓冲区
		LocalSceneProxy->InitializePooledBuffers(GraphBuilder);

		// 设置着色器参数
		FSurfacePolygonRenderPS::FParameters* PassParameters = GraphBuilder.AllocParameters<FSurfacePolygonRenderPS::FParameters>();

		// DepthTexture 
		FRDGTextureSRVRef DepthTextureSRV = GraphBuilder.CreateSRV(FRDGTextureSRVDesc::Create(Parameters.DepthTexture));
		PassParameters->DepthTexture = DepthTextureSRV;

		// ColorTexture
		FRDGTextureSRVRef ColorTextureSRV = GraphBuilder.CreateSRV(FRDGTextureSRVDesc::Create(Parameters.ColorTexture));
		PassParameters->ColorTexture = ColorTextureSRV;

		// 使用持久化缓冲区
		if (LocalSceneProxy->bBuffersInitialized)
		{
			// 注册持久化缓冲区到当前帧的RDG
			FRDGBuffer* BVHNodesRDGBuffer = GraphBuilder.RegisterExternalBuffer(LocalSceneProxy->BVHNodesPooledBuffer);
			PassParameters->PolygonBVHNodeData = GraphBuilder.CreateSRV(BVHNodesRDGBuffer);

			FRDGBuffer* TrianglesRDGBuffer = GraphBuilder.RegisterExternalBuffer(LocalSceneProxy->TrianglesPooledBuffer);
			PassParameters->TriangleData = GraphBuilder.CreateSRV(TrianglesRDGBuffer);
		}

		// 计算屏幕位置到世界位置的变换矩阵
		FMatrix InvViewProjMatrix = (Parameters.ViewMatrix * Parameters.ProjMatrix).Inverse();

		PassParameters->ScreenToWorld = FMatrix44f(InvViewProjMatrix);

		// 设置视图矩阵的逆
		PassParameters->InvViewMatrix = FMatrix44f(Parameters.ViewMatrix).Inverse();

		// 设置视口参数
		PassParameters->ViewportRect = Parameters.ViewportRect;

		// 设置线参数
		PassParameters->Color = LocalSceneProxy->Color;
		PassParameters->Opacity = LocalSceneProxy->Opacity;

		// 绑定输出渲染目标
		PassParameters->RenderTargets[0] = FRenderTargetBinding(Parameters.ColorTexture, Parameters.ColorTexture->HasBeenProduced() ? ERenderTargetLoadAction::ELoad : ERenderTargetLoadAction::ENoAction);

		// 获取着色器
		TShaderMapRef<FSurfacePolygonRenderPS> PixelShader(GetGlobalShaderMap(GMaxRHIFeatureLevel));

		// 添加全屏pass
		FPixelShaderUtils::AddFullscreenPass(
			GraphBuilder,
			GetGlobalShaderMap(GMaxRHIFeatureLevel),
			RDG_EVENT_NAME("SurfacePolygonRender_%d", LocalSceneProxy->GetProxyId()),
			PixelShader,
			PassParameters,
			FIntRect(),
			TStaticBlendState<>::GetRHI(),
			TStaticRasterizerState<>::GetRHI(),
			TStaticDepthStencilState<>::GetRHI()
			);
	}
}

void FSurfacePolygonSceneProxy::InitializePooledBuffers(FRDGBuilder& GraphBuilder)
{
	if (bBuffersInitialized)
	{
		return;
	}

	// BVH节点数据
	FRDGBufferDesc BVHNodesDesc = FRDGBufferDesc::CreateStructuredDesc(sizeof(FGPUPolygonBVHNode), GPUPolygonData->Nodes.Num());
	auto test1 = GPUPolygonData->Nodes.Num() * sizeof(FGPUPolygonBVHNode);

	FRDGBuffer* BVHNodesBuffer = GraphBuilder.CreateBuffer(BVHNodesDesc, TEXT("BVHNodesBuffer"));
	GraphBuilder.QueueBufferUpload(
		BVHNodesBuffer,
		GPUPolygonData->Nodes.GetData(),
		GPUPolygonData->Nodes.Num() * sizeof(FGPUPolygonBVHNode)
	);
	// 转换为持久化缓冲区
	BVHNodesPooledBuffer = GraphBuilder.ConvertToExternalBuffer(BVHNodesBuffer);

	// 三角形数据
	FRDGBufferDesc TrianglesDesc = FRDGBufferDesc::CreateStructuredDesc(sizeof(FGPUTriangle), GPUPolygonData->Triangles.Num());
	FRDGBuffer* TrianglesBuffer = GraphBuilder.CreateBuffer(TrianglesDesc, TEXT("TrianglesBuffer"));
	GraphBuilder.QueueBufferUpload(
		TrianglesBuffer,
		GPUPolygonData->Triangles.GetData(),
		GPUPolygonData->Triangles.Num() * sizeof(FGPUTriangle)
	);
	TrianglesPooledBuffer = GraphBuilder.ConvertToExternalBuffer(TrianglesBuffer);

	bBuffersInitialized = true;
}

void FSurfacePolygonSceneProxy::ReleasePooledBuffers()
{
	if (BVHNodesPooledBuffer)
	{
		BVHNodesPooledBuffer.SafeRelease();
	}
	if (TrianglesPooledBuffer)
	{
		TrianglesPooledBuffer.SafeRelease();
	}

	bBuffersInitialized = false;
}
