// Copyright (c) 2025 YaoZhoubo. All rights reserved.


#include "Shaders/UseShaderActor.h"

#include "Shaders/MyShader.h"
#include "GlobalShader.h"
#include "RHICommandList.h"
#include "RenderGraphUtils.h"

#include "LevelEditor.h"
#include "SLevelViewport.h"
#include "Slate/SceneViewport.h"

AUseShaderActor::AUseShaderActor()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

}

// Called when the game starts or when spawned
void AUseShaderActor::BeginPlay()
{
	Super::BeginPlay();
}

void AUseShaderActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);


	// 提交渲染命令
	ENQUEUE_RENDER_COMMAND(DrawTriangleCommand)(
		[this](FRHICommandListImmediate& RHICmdList)
		{
			FLevelEditorModule& LevelEditorModule = FModuleManager::GetModuleChecked<FLevelEditorModule>("LevelEditor");
			TSharedPtr<SLevelViewport> ActiveLevelViewport = LevelEditorModule.GetFirstActiveLevelViewport();
			if (!ActiveLevelViewport.IsValid()) return;

			TSharedPtr<FSceneViewport> SceneViewport = ActiveLevelViewport->GetSceneViewport();
			if (!SceneViewport) return;

			FRHIViewport* ViewportRHI = SceneViewport->GetViewportRHI();
			if (!ViewportRHI) return;

			// 获取后备缓冲区的纹理
			FRHITexture* BackBuffer = RHIGetViewportBackBuffer(ViewportRHI);

			if (!BackBuffer) return;

			FRHIRenderPassInfo RenderPassInfo(BackBuffer, ERenderTargetActions::Load_Store);
			RHICmdList.BeginRenderPass(RenderPassInfo, TEXT("OutputToViewport"));

			// 1. 获取着色器
			FGlobalShaderMap* ShaderMap = GetGlobalShaderMap(GMaxRHIShaderPlatform);
			TShaderMapRef<FMyShaderVS> VertexShader(ShaderMap);
			TShaderMapRef<FMyShaderPS> PixelShader(ShaderMap);

			// 2. 定义顶点数据（裁剪空间坐标 [-1, 1]）
			struct FSimpleVertex
			{
				FVector4f Position;
			};

			TArray<FSimpleVertex> Vertices;
			Vertices.Add({ FVector4f(0.0f, 0.5f, 0.0f, 1.0f) });  // 顶点1
			Vertices.Add({ FVector4f(0.5f, -0.5f, 0.0f, 1.0f) }); // 顶点2
			Vertices.Add({ FVector4f(-0.5f, -0.5f, 0.0f, 1.0f) }); // 顶点3

			// 3. 创建顶点缓冲区
			FRHIResourceCreateInfo CreateInfo(TEXT("VertexBuffer"));
			FBufferRHIRef VertexBuffer = RHICmdList.CreateVertexBuffer(
				Vertices.Num() * sizeof(FSimpleVertex),
				BUF_Volatile | BUF_Static,
				CreateInfo
			);

			// 填充数据
			void* Data = RHICmdList.LockBuffer(VertexBuffer, 0, Vertices.Num() * sizeof(FSimpleVertex), RLM_WriteOnly);
			FMemory::Memcpy(Data, Vertices.GetData(), Vertices.Num() * sizeof(FSimpleVertex));
			RHICmdList.UnlockBuffer(VertexBuffer);

			// 4. 配置图形管线
			FGraphicsPipelineStateInitializer GraphicsPSOInit;
			RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);
			GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();
			GraphicsPSOInit.BlendState = TStaticBlendState<>::GetRHI();
			GraphicsPSOInit.RasterizerState = TStaticRasterizerState<>::GetRHI();
			GraphicsPSOInit.PrimitiveType = PT_TriangleList;

			// 顶点声明（仅位置）
			FVertexDeclarationElementList Elements;
			Elements.Add(FVertexElement(0, 0, VET_Float4, 0, sizeof(FSimpleVertex)));
			GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = RHICreateVertexDeclaration(Elements);

			// 绑定着色器
			GraphicsPSOInit.BoundShaderState.VertexShaderRHI = VertexShader.GetVertexShader();
			GraphicsPSOInit.BoundShaderState.PixelShaderRHI = PixelShader.GetPixelShader();

			// 5. 绘制三角形
			SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit, 0);
			RHICmdList.SetStreamSource(0, VertexBuffer, 0);
			RHICmdList.DrawPrimitive(0, 1, 1); // 绘制1个图元（3个顶点）

			RHICmdList.EndRenderPass();
		});
}

