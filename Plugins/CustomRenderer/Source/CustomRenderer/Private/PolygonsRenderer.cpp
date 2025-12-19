// Copyright(c) 2025 YaoZhoubo. All rights reserved.

#include "PolygonsRenderer.h"

#include "GlobalShader.h"
#include "ShaderParameterStruct.h"
#include "RenderGraphUtils.h"
#include "RenderTargetPool.h"
#include "Modules/ModuleManager.h"
#include "PixelShaderUtils.h"


class FPolygonsRenderPS : public FGlobalShader
{
public:
	// 声明全局着色器
	DECLARE_GLOBAL_SHADER(FPolygonsRenderPS);

	// 告诉引擎此着色器使用结构作为其参数
	SHADER_USE_PARAMETER_STRUCT(FPolygonsRenderPS, FGlobalShader);

	// 着色器参数结构声明
	// 参数与HLSL代码中的参数匹配
	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D, DepthTexture)
		SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D, ColorTexture)
		SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D, BVHDataTexture)
		SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D, SegmentDataTexture)
		SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D, CustomTexture)
		SHADER_PARAMETER_SAMPLER(SamplerState, CustomTextureSampler)
		SHADER_PARAMETER(FMatrix44f, ScreenPositionToWorldPosition)
		SHADER_PARAMETER(FMatrix44f, InvViewMatrix)
		SHADER_PARAMETER(FIntRect, ViewportRect)
		SHADER_PARAMETER(float, LineWidth)
		SHADER_PARAMETER(float, LineOpacity)
		SHADER_PARAMETER(FVector4f, LineColor)
		RENDER_TARGET_BINDING_SLOTS()
	END_SHADER_PARAMETER_STRUCT()

public:
	// 由引擎调用，以确定为此着色器编译哪些变体
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return true;
	}
};

// 创建着色器			着色器类			着色器文件位置							着色器入口函数名	着色器类型
IMPLEMENT_GLOBAL_SHADER(FPolygonsRenderPS, "/CustomRenderer/PolygonsRenderShader.usf", "MainPixelShader", SF_Pixel);

// 着色器管理器实例初始化
FPolygonsRenderManager* FPolygonsRenderManager::Instance = nullptr;

void FPolygonsRenderManager::BeginRendering()
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
			FPostOpaqueRenderDelegate::CreateRaw(this, &FPolygonsRenderManager::Execute_RenderThread)
		);
	}
}

void FPolygonsRenderManager::EndRendering()
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

	if (SceneProxy)
	{
		SceneProxy->Reset();
	}
}

void FPolygonsRenderManager::RegisterSceneProxy(FPolygonsSceneProxy* InSceneProxy)
{
	FPolygonsSceneProxy* LocalSceneProxy = InSceneProxy;

	// 在渲染线程中注册
	ENQUEUE_RENDER_COMMAND(RegisterSceneProxyCommand)(
		[this, LocalSceneProxy](FRHICommandList& RHICmdList)
		{
			SceneProxy = LocalSceneProxy;
		});
}

void FPolygonsRenderManager::UnregisterSceneProxy(FPolygonsSceneProxy* InSceneProxy)
{
	// 在渲染线程中注销
	ENQUEUE_RENDER_COMMAND(RegisterSceneProxyCommand)(
		[this](FRHICommandList& RHICmdList)
		{
			SceneProxy = nullptr;
		});
}

void FPolygonsRenderManager::Execute_RenderThread(FPostOpaqueRenderParameters& Parameters)
{
	// 检查是否在渲染线程
	check(IsInRenderingThread());

	if (!SceneProxy || !SceneProxy->NodesDataTexture || !SceneProxy->SegmentsDataTexture)
	{
		return;
	}

	FRDGBuilder& GraphBuilder = *Parameters.GraphBuilder;

	// 设置着色器参数
	FPolygonsRenderPS::FParameters* PassParameters = GraphBuilder.AllocParameters<FPolygonsRenderPS::FParameters>();

	// 绑定输入纹理
	FRDGTextureSRVRef DepthTextureSRV = GraphBuilder.CreateSRV(FRDGTextureSRVDesc::Create(Parameters.DepthTexture));
	PassParameters->DepthTexture = DepthTextureSRV;

	FRDGTextureSRVRef ColorTextureSRV = GraphBuilder.CreateSRV(FRDGTextureSRVDesc::Create(Parameters.ColorTexture));
	PassParameters->ColorTexture = ColorTextureSRV;

	// 创建GPU端的RDG纹理引用
	// NodesDataTexture
	FTextureRenderTargetResource* NodesDataTextureResource = SceneProxy->NodesDataTexture->GetRenderTargetResource();
	FRDGTextureRef NodesDataTextureRDG = GraphBuilder.RegisterExternalTexture(
		CreateRenderTarget(NodesDataTextureResource->GetRenderTargetTexture(), TEXT("NodesDataTexture"))
	);
	FRDGTextureSRVRef NodesDataTextureSRV = GraphBuilder.CreateSRV(FRDGTextureSRVDesc::Create(NodesDataTextureRDG));
	PassParameters->BVHDataTexture = NodesDataTextureSRV;

	// SegmentsDataTexture
	FTextureRenderTargetResource* SegmentsDataTextureResource = SceneProxy->SegmentsDataTexture->GetRenderTargetResource();
	FRDGTextureRef SegmentsDataTextureRDG = GraphBuilder.RegisterExternalTexture(
		CreateRenderTarget(SegmentsDataTextureResource->GetRenderTargetTexture(), TEXT("SegmentsDataTexture"))
	);
	FRDGTextureSRVRef SegmentsDataTextureSRV = GraphBuilder.CreateSRV(FRDGTextureSRVDesc::Create(SegmentsDataTextureRDG));
	PassParameters->SegmentDataTexture = SegmentsDataTextureSRV;

	// CustomTexture
	if(ensureMsgf(SceneProxy->CustomTexture, TEXT("请确保CustomTexture有效，否则会导致该pass无效")))
	{
		FTextureResource* CustomTextureResource = SceneProxy->CustomTexture->GetResource();
		if (CustomTextureResource)
		{
			FRDGTextureRef CustomTextureRDG = GraphBuilder.RegisterExternalTexture(
				CreateRenderTarget(CustomTextureResource->TextureRHI, TEXT("CustomTexture"))
			);
			FRDGTextureSRVRef CustomDataTextureSRV = GraphBuilder.CreateSRV(FRDGTextureSRVDesc::Create(CustomTextureRDG));
			PassParameters->CustomTexture = CustomDataTextureSRV;
		}
	}
	else
	{
		return;
	}

	// 绑定输出渲染目标
	PassParameters->RenderTargets[0] = FRenderTargetBinding(Parameters.ColorTexture, Parameters.ColorTexture->HasBeenProduced() ? ERenderTargetLoadAction::ELoad : ERenderTargetLoadAction::ENoAction);

	// 设置视口参数
	PassParameters->ViewportRect = Parameters.ViewportRect;

	// 计算屏幕位置到世界位置的变换矩阵
	FMatrix InvViewProjMatrix = Parameters.ViewMatrix * Parameters.ProjMatrix;
	InvViewProjMatrix = InvViewProjMatrix.Inverse();
	PassParameters->ScreenPositionToWorldPosition = FMatrix44f(InvViewProjMatrix);

	PassParameters->InvViewMatrix = FMatrix44f(Parameters.ViewMatrix).Inverse();

	// 设置线参数
	PassParameters->LineWidth = SceneProxy->LineWidth;
	PassParameters->LineOpacity = SceneProxy->LineOpacity;
	PassParameters->LineColor = SceneProxy->LineColor;

	// 设置采样器参数
	PassParameters->CustomTextureSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();

	// 获取着色器
	TShaderMapRef<FPolygonsRenderPS> PixelShader(GetGlobalShaderMap(GMaxRHIFeatureLevel));

	// 添加全屏pass
	FPixelShaderUtils::AddFullscreenPass(
		GraphBuilder,
		GetGlobalShaderMap(GMaxRHIFeatureLevel),
		RDG_EVENT_NAME("OnOverlayRender_ColorTextureOverlay"),
		PixelShader,
		PassParameters,
		FIntRect(),
		TStaticBlendState<>::GetRHI(),
		TStaticRasterizerState<>::GetRHI(),
		TStaticDepthStencilState<false, CF_Always>::GetRHI()
	);
}