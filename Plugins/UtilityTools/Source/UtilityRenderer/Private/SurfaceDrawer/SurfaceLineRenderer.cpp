#include "SurfaceDrawer/SurfaceLineRenderer.h"

#include "GlobalShader.h"
#include "ShaderParameterStruct.h"
#include "ShaderParameterMacros.h"
#include "RHICommandList.h"
#include "RenderingThread.h"
#include "Modules/ModuleManager.h"
#include "RendererInterface.h"
#include "SceneView.h"
#include "SceneInterface.h"
#include "RenderGraphResources.h"
#include "RenderTargetPool.h"
#include "PixelShaderUtils.h"
#include "RenderGraphEvent.h"
#include "SceneTexturesConfig.h"

#include "../Private/SceneRendering.h"

	
class FSurfaceLineRenderPS : public FGlobalShader
{
public:
	// 声明全局着色器
	DECLARE_GLOBAL_SHADER(FSurfaceLineRenderPS);
	
	// 告诉引擎此着色器使用结构作为其参数
	SHADER_USE_PARAMETER_STRUCT(FSurfaceLineRenderPS, FGlobalShader);
	
	// 着色器参数结构声明
	// 参数与HLSL代码中的参数匹配
	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D, DepthTexture)									// 深度纹理
		SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D, ColorTexture)									// 颜色纹理
		SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D<uint2>, CustomDepthTexture)						// 自定义深度纹理
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<FGPULineBVHNode>, LineBVHNodeData)			// BVH节点数据
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<FGPUSegmentCluster>, SegmentClusterData)	// 线段簇数据
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<FGPUSegment>, SegmentData)					// 线段数据
		SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D, CustomTexture)									// 自定义纹理
		SHADER_PARAMETER_SAMPLER(SamplerState, CustomTextureSampler)								// 自定义纹理采样器
		SHADER_PARAMETER(FMatrix44f, ScreenToWorld)													// 屏幕到世界坐标变换矩阵
		SHADER_PARAMETER(FMatrix44f, InvViewMatrix)													// 视图矩阵的逆矩阵
		SHADER_PARAMETER(FIntRect, ViewportRect)													// 视口矩形
		SHADER_PARAMETER(float, LineWidth)															// 线宽
		SHADER_PARAMETER(float, LineOpacity)														// 线透明度
		SHADER_PARAMETER(FVector4f, LineColor)														// 线颜色
		SHADER_PARAMETER(uint32, bUseCustomTexture)													// 是否使用自定义纹理
		SHADER_PARAMETER(uint32, bUsePixelUnit)														// 是否使用像素单位
		RENDER_TARGET_BINDING_SLOTS()																// 渲染目标绑定槽
	END_SHADER_PARAMETER_STRUCT()
	
public:
	// 由引擎调用，以确定为此着色器编译哪些变体
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return true;
	}
};
	
// 实现全局着色器		着色器类				着色器文件位置							着色器入口函数名	着色器类型
IMPLEMENT_GLOBAL_SHADER(FSurfaceLineRenderPS, "/UtilityTools/SurfaceLineRenderShader.usf", "MainPixelShader", SF_Pixel);
	
// 着色器管理器实例初始化
FSurfaceLineRenderManager* FSurfaceLineRenderManager::Instance = nullptr;
	
FSurfaceLineRenderManager::~FSurfaceLineRenderManager()
{
	// 确保渲染结束
	EndRendering();

	FScopeLock Lock(&RenderThreadLock);
	for (auto& ProxyPair : SceneProxyMap)
	{
		if (ProxyPair.Value.IsValid())
		{
			// 发送渲染命令释放GPU资源
			ENQUEUE_RENDER_COMMAND(ReleaseSurfaceLineResources)(
				[SceneProxyCopy = ProxyPair.Value](FRHICommandList& RHICmdList)
				{
					SceneProxyCopy->ReleasePooledBuffers();
				});
		}
	}
	SceneProxyMap.Empty();
}
	
void FSurfaceLineRenderManager::RegisterSceneProxy(const TSharedPtr<FSurfaceLineSceneProxy>& InSceneProxy)
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
	
void FSurfaceLineRenderManager::UnregisterSceneProxy(uint32 ProxyId)
{
	TSharedPtr<FSurfaceLineSceneProxy> SceneProxyToRemove;
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
		ENQUEUE_RENDER_COMMAND(ReleaseSurfaceLineResources_ProxyId)(
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

int32 FSurfaceLineRenderManager::GetNumSceneProxies()
{
	FScopeLock Lock(&RenderThreadLock);
	
	return SceneProxyMap.Num();
}
	
void FSurfaceLineRenderManager::BeginRendering()
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
		OnOverlayRenderHandle = RendererModule->RegisterPostOpaqueRenderDelegate(
			FPostOpaqueRenderDelegate::CreateRaw(this, &FSurfaceLineRenderManager::Execute_RenderThread)
		);
	}
}

void FSurfaceLineRenderManager::EndRendering()
{
	if (!OnOverlayRenderHandle.IsValid())
	{
		return;
	}

	const FName RendererModuleName("Renderer");
	IRendererModule* RendererModule = FModuleManager::GetModulePtr<IRendererModule>(RendererModuleName);
	if (RendererModule)
	{
		RendererModule->RemovePostOpaqueRenderDelegate(OnOverlayRenderHandle);
	}

	OnOverlayRenderHandle.Reset();
}

void FSurfaceLineRenderManager::Execute_RenderThread(FPostOpaqueRenderParameters& Parameters)
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
	TArray<TSharedPtr<FSurfaceLineSceneProxy>> ProxiesToRender;
	for (auto& ProxyPair : SceneProxyMap)
	{
		if (ProxyPair.Value.IsValid())
		{
			ProxiesToRender.Add(ProxyPair.Value);
		}
	}
	Lock.Unlock();

	// 为每个场景代理创建渲染Pass
	for (TSharedPtr<FSurfaceLineSceneProxy> LocalSceneProxy : ProxiesToRender)
	{
		if (!LocalSceneProxy.IsValid() || !LocalSceneProxy->GPULineData.IsValid() || !LocalSceneProxy->GPULineData->IsValid())
		{
			continue;
		}

		FRDGBuilder& GraphBuilder = *Parameters.GraphBuilder;

		// 初始化池化缓冲区
		LocalSceneProxy->InitializePooledBuffers(GraphBuilder);

		// 设置着色器参数
		FSurfaceLineRenderPS::FParameters* PassParameters = GraphBuilder.AllocParameters<FSurfaceLineRenderPS::FParameters>();

		// DepthTexture 
		FRDGTextureSRVRef DepthTextureSRV = GraphBuilder.CreateSRV(FRDGTextureSRVDesc::Create(Parameters.DepthTexture));
		PassParameters->DepthTexture = DepthTextureSRV;

		// ColorTexture
		FRDGTextureSRVRef ColorTextureSRV = GraphBuilder.CreateSRV(FRDGTextureSRVDesc::Create(Parameters.ColorTexture));
		PassParameters->ColorTexture = ColorTextureSRV;

		// CustomDepthTexture
		if (Parameters.SceneTexturesUniformParams)
		{
			PassParameters->CustomDepthTexture = Parameters.SceneTexturesUniformParams->GetContents()->CustomStencilTexture;
		}
		else
		{
			// 使用默认全局白色纹理替代无效的自定义纹理
			FRDGTextureRef CustomTextureRDG = GraphBuilder.RegisterExternalTexture(
				CreateRenderTarget(GBlackTextureWithSRV->GetTextureRHI(), TEXT("GlobalBlackTexture")));
			FRDGTextureSRVRef CustomDataTextureSRV = GraphBuilder.CreateSRV(FRDGTextureSRVDesc::Create(CustomTextureRDG));
			PassParameters->CustomDepthTexture = CustomDataTextureSRV;
		}

		// 使用持久化缓冲区
		if (LocalSceneProxy->bBuffersInitialized)
		{
			// 注册持久化缓冲区到当前帧的RDG
			FRDGBuffer* BVHNodesRDGBuffer = GraphBuilder.RegisterExternalBuffer(LocalSceneProxy->BVHNodesPooledBuffer);
			PassParameters->LineBVHNodeData = GraphBuilder.CreateSRV(BVHNodesRDGBuffer);

			FRDGBuffer* ClustersRDGBuffer = GraphBuilder.RegisterExternalBuffer(LocalSceneProxy->ClustersPooledBuffer);
			PassParameters->SegmentClusterData = GraphBuilder.CreateSRV(ClustersRDGBuffer);

			FRDGBuffer* SegmentsRDGBuffer = GraphBuilder.RegisterExternalBuffer(LocalSceneProxy->SegmentsPooledBuffer);
			PassParameters->SegmentData = GraphBuilder.CreateSRV(SegmentsRDGBuffer);
		}
		else
		{
			continue;
		}

		// CustomTexture
		if (LocalSceneProxy->bUseCustomTexture)
		{
			if (LocalSceneProxy->CustomTexture)
			{
				FTextureResource* CustomTextureResource = LocalSceneProxy->CustomTexture->GetResource();
				if (CustomTextureResource)
				{
					FRDGTextureRef CustomTextureRDG = GraphBuilder.RegisterExternalTexture(
						CreateRenderTarget(CustomTextureResource->GetTextureRHI(), TEXT("CustomTexture")));
					FRDGTextureSRVRef CustomDataTextureSRV = GraphBuilder.CreateSRV(FRDGTextureSRVDesc::Create(CustomTextureRDG));
					PassParameters->CustomTexture = CustomDataTextureSRV;
				}
			}
			else
			{
				// 使用默认全局白色纹理替代无效的自定义纹理
				FRDGTextureRef CustomTextureRDG = GraphBuilder.RegisterExternalTexture(
					CreateRenderTarget(GBlackTextureWithSRV->GetTextureRHI(), TEXT("GlobalBlackTexture")));
				FRDGTextureSRVRef CustomDataTextureSRV = GraphBuilder.CreateSRV(FRDGTextureSRVDesc::Create(CustomTextureRDG));
				PassParameters->CustomTexture = CustomDataTextureSRV;
			}
		}
		else
		{
			// 使用默认全局白色纹理占位
			FRDGTextureRef CustomTextureRDG = GraphBuilder.RegisterExternalTexture(
				CreateRenderTarget(GBlackTextureWithSRV->GetTextureRHI(), TEXT("GlobalBlackTexture")));
			FRDGTextureSRVRef CustomDataTextureSRV = GraphBuilder.CreateSRV(FRDGTextureSRVDesc::Create(CustomTextureRDG));
			PassParameters->CustomTexture = CustomDataTextureSRV;
		}

		// 设置采样器参数
		PassParameters->CustomTextureSampler = TStaticSamplerState<SF_Bilinear, AM_Border, AM_Border, AM_Border>::GetRHI();

		// 计算屏幕位置到世界位置的变换矩阵
		FMatrix InvViewProjMatrix = (Parameters.ViewMatrix * Parameters.ProjMatrix).Inverse();

		PassParameters->ScreenToWorld = FMatrix44f(InvViewProjMatrix);

		// 设置视图矩阵的逆
		PassParameters->InvViewMatrix = FMatrix44f(Parameters.ViewMatrix).Inverse();

		// 设置视口参数
		PassParameters->ViewportRect = Parameters.ViewportRect;

		// 设置线参数
		PassParameters->LineWidth = LocalSceneProxy->LineWidth;
		PassParameters->LineOpacity = LocalSceneProxy->LineOpacity;
		PassParameters->LineColor = LocalSceneProxy->LineColor;
		PassParameters->bUseCustomTexture = LocalSceneProxy->bUseCustomTexture;
		PassParameters->bUsePixelUnit = LocalSceneProxy->bUsePixelUnit;

		// 绑定输出渲染目标
		PassParameters->RenderTargets[0] = FRenderTargetBinding(
			Parameters.ColorTexture,
			Parameters.ColorTexture->HasBeenProduced() ? ERenderTargetLoadAction::ELoad : ERenderTargetLoadAction::ENoAction);

		// 获取着色器
		TShaderMapRef<FSurfaceLineRenderPS> PixelShader(GetGlobalShaderMap(GMaxRHIFeatureLevel));

		// 添加全屏pass
		FPixelShaderUtils::AddFullscreenPass(
			GraphBuilder,
			GetGlobalShaderMap(GMaxRHIFeatureLevel),
			RDG_EVENT_NAME("SurfaceLineRender_%d", LocalSceneProxy->GetProxyId()),
			PixelShader,
			PassParameters,
			FIntRect(),
			TStaticBlendState<>::GetRHI(),
			TStaticRasterizerState<>::GetRHI(),
			TStaticDepthStencilState<>::GetRHI()
			);
	}
}

void FSurfaceLineSceneProxy::InitializePooledBuffers(FRDGBuilder& GraphBuilder)
{
	if (bBuffersInitialized)
	{
		return;
	}
	// 节点数据缓冲区
	FRDGBufferDesc BVHNodesDesc = FRDGBufferDesc::CreateStructuredDesc(
		sizeof(FGPULineBVHNode), GPULineData->Nodes.Num());
	FRDGBuffer* BVHNodesBuffer = GraphBuilder.CreateBuffer(
		BVHNodesDesc, TEXT("BVHNodesPooledBuffer"), 
		ERDGBufferFlags::MultiFrame);
	GraphBuilder.QueueBufferUpload(
		BVHNodesBuffer, GPULineData->Nodes.GetData(),
		GPULineData->Nodes.Num() * sizeof(FGPULineBVHNode));
	BVHNodesPooledBuffer = GraphBuilder.ConvertToExternalBuffer(BVHNodesBuffer);

	// 创建簇数据缓冲区
	FRDGBufferDesc ClustersDesc = FRDGBufferDesc::CreateStructuredDesc(
		sizeof(FGPUSegmentCluster), GPULineData->Clusters.Num());
	FRDGBuffer* ClustersBuffer = GraphBuilder.CreateBuffer(
		ClustersDesc, TEXT("ClustersPooledBuffer"),
		ERDGBufferFlags::MultiFrame);
	GraphBuilder.QueueBufferUpload(
		ClustersBuffer, GPULineData->Clusters.GetData(),
		GPULineData->Clusters.Num() * sizeof(FGPUSegmentCluster));
	ClustersPooledBuffer = GraphBuilder.ConvertToExternalBuffer(ClustersBuffer);

	// 创建线段数据缓冲区
	FRDGBufferDesc SegmentsDesc = FRDGBufferDesc::CreateStructuredDesc(
		sizeof(FGPUSegment), GPULineData->Segments.Num());
	FRDGBuffer* SegmentsBuffer = GraphBuilder.CreateBuffer(
		SegmentsDesc, TEXT("SegmentsPooledBuffer"));
	GraphBuilder.QueueBufferUpload(
		SegmentsBuffer, GPULineData->Segments.GetData(),
		GPULineData->Segments.Num() * sizeof(FGPUSegment));
	SegmentsPooledBuffer = GraphBuilder.ConvertToExternalBuffer(SegmentsBuffer);

	bBuffersInitialized = true;
}

void FSurfaceLineSceneProxy::ReleasePooledBuffers()
{
	if (BVHNodesPooledBuffer)
	{
		BVHNodesPooledBuffer.SafeRelease();
	}
	if (ClustersPooledBuffer)
	{
		ClustersPooledBuffer.SafeRelease();
	}
	if (SegmentsPooledBuffer)
	{
		SegmentsPooledBuffer.SafeRelease();
	}
	bBuffersInitialized = false;
}