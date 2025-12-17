// Copyright(c) 2025 YaoZhoubo. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/TextureRenderTarget2D.h"


class UBVHComponent;

class  FPolygonsSceneProxy
{
public:
	FPolygonsSceneProxy() : NodesDataTexture(nullptr), SegmentsDataTexture(nullptr), CustomTexture(nullptr), LineWidth(0.0f), LineOpacity(0.0f) {}
	FPolygonsSceneProxy(UTextureRenderTarget2D* InNodesDataTexture, UTextureRenderTarget2D* InSegmentsDataTexture, UTexture2D* InCustomTexture, float InLineWidth, float InLineOpacity, FLinearColor InLineColor)
		: NodesDataTexture(InNodesDataTexture)
		, SegmentsDataTexture(InSegmentsDataTexture)
		, CustomTexture(InCustomTexture)
		, LineWidth(InLineWidth)
		, LineOpacity(InLineOpacity)
		, LineColor(InLineColor)
	{
	}

	void UpdateParameters(UTextureRenderTarget2D* InNodesDataTexture, UTextureRenderTarget2D* InSegmentsDataTexture, UTexture2D* InCustomTexture, float InLineWidth, float InLineOpacity, const FLinearColor& InLineColor)
	{
		NodesDataTexture = InNodesDataTexture;
		SegmentsDataTexture = InSegmentsDataTexture;
		CustomTexture = InCustomTexture;
		LineWidth = InLineWidth;
		LineOpacity = InLineOpacity;
		LineColor = InLineColor;
	}

	void Reset()
	{
		NodesDataTexture = nullptr;
		SegmentsDataTexture = nullptr;
		LineWidth = 0.0f;
		LineOpacity = 0.0f;
		LineColor = FLinearColor::White;
	}

private:
	TObjectPtr<UTextureRenderTarget2D> NodesDataTexture;
	TObjectPtr<UTextureRenderTarget2D> SegmentsDataTexture;
	UTexture2D* CustomTexture;
	float LineWidth;
	float LineOpacity;
	FLinearColor LineColor;

	friend class FPolygonsRenderManager;
};

class CUSTOMRENDERER_API FPolygonsRenderManager
{
public:
	static FPolygonsRenderManager* Get()
	{
		if (!Instance)
		{
			Instance = new FPolygonsRenderManager();
		}
		return Instance;
	};

	/// \brief 将 Execute_RenderThread 挂接到渲染器。
	void BeginRendering();

	/// \brief 从渲染器移除 Execute_RenderThread
	void EndRendering();

	// 注册/注销组件
	void RegisterSceneProxy(FPolygonsSceneProxy* InSceneProxy);
	void UnregisterSceneProxy(FPolygonsSceneProxy* InSceneProxy);

private:
	/// \brief 私有构造函数
	FPolygonsRenderManager() = default;

	/// \brief 着色器管理器实例
	static FPolygonsRenderManager* Instance;

	/// \brief 添加了 Execute_RenderThread 的委托的句柄
	FDelegateHandle OnOverlayRenderHandle;

	/// \brief 管理着色器参数
	FPolygonsSceneProxy* SceneProxy;

	void Execute_RenderThread(FPostOpaqueRenderParameters& Parameters);
};