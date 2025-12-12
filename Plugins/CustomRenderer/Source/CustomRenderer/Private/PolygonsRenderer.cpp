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
	// 参数必须与HLSL代码中的参数匹配
	// 对于每个参数，提供C++类型和名称（与HLSL代码中使用的名称相同）
	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, DepthTexture)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, ColorTexture)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, BVHDataTexture)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, SegmentDataTexture)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, CustomTexture)
		SHADER_PARAMETER_SAMPLER(SamplerState, CustomTextureSampler)
		SHADER_PARAMETER(FMatrix44f, ScreenPositionToWorldPosition)
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

// 创建着色器           着色器类					着色器文件位置							着色器入口函数名     着色器类型
IMPLEMENT_GLOBAL_SHADER(FPolygonsRenderPS, "/CustomRenderer/PolygonsRenderShader.usf", "MainPixelShader", SF_Pixel);

// 着色器管理器实例初始化
FPolygonsRenderManager* FPolygonsRenderManager::Instance = nullptr;

void FPolygonsRenderManager::BeginRendering()
{
	// 如果句柄已经初始化并且有效，则无需执行任何操作
	if (OnOverlayRenderHandle.IsValid())
	{
		return;
	}

	// 获取渲染模块，将 Execute_RenderThread 添加到回调中，以便在场景渲染完成后每一帧执行
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
	if (InSceneProxy)
	{
		SceneProxy = InSceneProxy;
	}
}

void FPolygonsRenderManager::UnregisterSceneProxy(FPolygonsSceneProxy* InSceneProxy)
{
	SceneProxy = nullptr;
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
	PassParameters->DepthTexture = Parameters.DepthTexture;
	PassParameters->ColorTexture = Parameters.ColorTexture;

	// 创建CPU端渲染目标的RDG纹理引用
	FTextureRenderTargetResource* NodesDataTextureResource = SceneProxy->NodesDataTexture->GetRenderTargetResource();
	FRDGTextureRef NodesDataTextureRDG = GraphBuilder.RegisterExternalTexture(
		CreateRenderTarget(NodesDataTextureResource->GetRenderTargetTexture(), TEXT("NodesDataTexture"))
	);

	FTextureRenderTargetResource* SegmentsDataTextureResource = SceneProxy->SegmentsDataTexture->GetRenderTargetResource();
	FRDGTextureRef SegmentsDataTextureRDG = GraphBuilder.RegisterExternalTexture(
		CreateRenderTarget(SegmentsDataTextureResource->GetRenderTargetTexture(), TEXT("SegmentsDataTexture"))
	);

	FTextureRHIRef CustomTextureResource = SceneProxy->CustomTexture->GetResource()->TextureRHI;
	FRDGTextureRef CustomTextureRDG = GraphBuilder.RegisterExternalTexture(
		CreateRenderTarget(CustomTextureResource, TEXT("CustomTexture"))
	);

	PassParameters->BVHDataTexture = NodesDataTextureRDG;
	PassParameters->SegmentDataTexture = SegmentsDataTextureRDG;
	PassParameters->CustomTexture = CustomTextureRDG;

	// 设置采样器参数
	PassParameters->CustomTextureSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();

	// 绑定输出渲染目标
	FRDGTextureDesc OutputDesc = Parameters.ColorTexture->Desc;// 创建输出纹理（与颜色纹理相同的格式和尺寸）
	FRDGTextureRef OutputTexture = GraphBuilder.CreateTexture(OutputDesc, TEXT("ColorTextureOverlay"));
	PassParameters->RenderTargets[0] = FRenderTargetBinding(OutputTexture, ERenderTargetLoadAction::EClear);

	// 设置视口参数
	PassParameters->ViewportRect = Parameters.ViewportRect;

	// 计算屏幕位置到世界位置的变换矩阵
	FMatrix InvViewProjMatrix = Parameters.ViewMatrix * Parameters.ProjMatrix;
	InvViewProjMatrix = InvViewProjMatrix.Inverse(); 
	PassParameters->ScreenPositionToWorldPosition = FMatrix44f(InvViewProjMatrix);

	// 设置线参数
	PassParameters->LineWidth = SceneProxy->LineWidth;
	PassParameters->LineOpacity = SceneProxy->LineOpacity;
	PassParameters->LineColor = SceneProxy->LineColor;

	// 获取全局着色器
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

	// 用处理后的纹理覆盖ColorTexture
	AddCopyTexturePass(GraphBuilder, OutputTexture, Parameters.ColorTexture);
}