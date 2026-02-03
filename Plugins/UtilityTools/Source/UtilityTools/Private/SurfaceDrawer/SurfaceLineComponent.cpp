#include "SurfaceDrawer/SurfaceLineComponent.h"

#include "SurfaceDrawer/SurfaceLineBuilder.h"
#include "SurfaceDrawer/SurfaceLineRenderer.h"


DEFINE_LOG_CATEGORY_STATIC(LogSurfaceLineComponent, Log, All);
	
USurfaceLineComponent::USurfaceLineComponent()
{
	PrimaryComponentTick.bCanEverTick = true;

	IsAsyncBuilding.store(false);
	
	// 初始化线段渲染默认参数
	LineWidth = 2.0f;
	LineOpacity = 0.5f;
	LineColor = FLinearColor::Green;
	bUseCustomTexture = false;
	bUsePixelUnit = false;
	bBuffersInitialized = false;
}

void USurfaceLineComponent::SetPolygons(const TArray<FPolygon>& InPolygons)
{
	AsyncBuildBVHData(InPolygons);
}

void USurfaceLineComponent::SetProperties(float InLineWidth, float InLineOpacity, const FLinearColor& InLineColor)
{
	LineWidth = InLineWidth;
	LineOpacity = InLineOpacity;
	LineColor = InLineColor;

	MarkRenderStateDirty();
}

void USurfaceLineComponent::SetLineColor(const FLinearColor& InLineColor)
{
	LineColor = InLineColor;

	MarkRenderStateDirty();
}

void USurfaceLineComponent::SetLineOpacity(float InLineOpacity)
{
	LineOpacity = InLineOpacity;

	MarkRenderStateDirty();
}

void USurfaceLineComponent::SetLineWidth(float InLineWidth)
{
	LineWidth = InLineWidth;

	MarkRenderStateDirty();
}

void USurfaceLineComponent::ClearPolygons()
{
	AsyncBuildBVHData(TArray<FPolygon>());

	MarkGeometryDataDirty();
}

void USurfaceLineComponent::OnRegister()
{
	Super::OnRegister();
	
	CreateSceneProxy();

	// Game 或者 PIE模式下注册代理
	UWorld* World = GetWorld();
	if (World && (World->WorldType == EWorldType::Game || World->WorldType == EWorldType::PIE))
	{
		FSurfaceLineRenderManager* SurfaceLineRenderManager = FSurfaceLineRenderManager::Get();
		SurfaceLineRenderManager->RegisterSceneProxy(SceneProxy);
	}
}
	
void USurfaceLineComponent::OnUnregister()
{
	FSurfaceLineRenderManager* SurfaceLineRenderManager = FSurfaceLineRenderManager::Get();
	SurfaceLineRenderManager->UnregisterSceneProxy(SceneProxy->GetProxyId());
	DestroySceneProxy();
	
	Super::OnUnregister();
}
	
void USurfaceLineComponent::CreateRenderState_Concurrent(FRegisterComponentContext* Context)
{
	Super::CreateRenderState_Concurrent(Context);
	
	if (SceneProxy.IsValid())
	{
		UpdateSceneProxy();
	}
}
	
void USurfaceLineComponent::DestroyRenderState_Concurrent()
{
	Super::DestroyRenderState_Concurrent();
}
	
bool USurfaceLineComponent::ShouldCreateRenderState() const
{
	return true;
}
	
void USurfaceLineComponent::AsyncBuildBVHData(const TArray<FPolygon>& InPolygons)
{
	if (InPolygons.Num() == 0)
	{
		UE_LOG(LogSurfaceLineComponent, Warning, TEXT("Polygons为空，跳过构建"));
		GPULineData.Reset();

		MarkGeometryDataDirty();
		return;
	}
	
	if (IsAsyncBuilding.load())
	{
		UE_LOG(LogSurfaceLineComponent, Warning, TEXT("正在进行上一次的AsyncBuildBVHData请求，跳过构建"));
		return;
	}
	
	IsAsyncBuilding.store(true);
	
	TWeakObjectPtr<USurfaceLineComponent> WeakThis(this);
	
	AsyncTask(ENamedThreads::AnyThread,
		[WeakThis, InPolygons]()
		{
			TSharedPtr<FLineBVHBuilder> NewLineBVHBuilder = MakeShared<FLineBVHBuilder>(InPolygons, WeakThis->BVHBuildConfig);
			NewLineBVHBuilder->Build();
	
			if (!WeakThis.IsValid())
			{
				UE_LOG(LogSurfaceLineComponent, Warning, TEXT("组件已销毁，取消AsyncBuildBVHData"));
				return;
			}
	
			NewLineBVHBuilder->GetStats(WeakThis->BVHStats);

			// 转换为GPU数据
			TSharedPtr<FGPULineData> NewGPULineData = MakeShared<FGPULineData>();
			FLineDataConverter::ConvertToGPUData(*NewLineBVHBuilder, *NewGPULineData);
			WeakThis->GPULineData = NewGPULineData;
			WeakThis->MarkRenderStateDirty();
			WeakThis->MarkGeometryDataDirty();

			WeakThis->IsAsyncBuilding.store(false);
		}
	);
}
	
void USurfaceLineComponent::CreateSceneProxy()
{
	check(IsInGameThread());

	SceneProxy = MakeShared<FSurfaceLineSceneProxy>(
		GPULineData,
		CustomTexture,
		LineWidth,
		LineOpacity,
		LineColor,
		bUseCustomTexture,
		bUsePixelUnit);
}
	
void USurfaceLineComponent::UpdateSceneProxy()
{
	if (SceneProxy.IsValid())
	{
		// 渲染线程更新参数
		ENQUEUE_RENDER_COMMAND(UpdateSceneProxyCommand)(
			[SceneProxyCopy = SceneProxy,
			GPULineDataCopy = GPULineData,
			CustomTextureCopy = CustomTexture,
			LineWidthCopy = LineWidth,
			LineOpacityCopy = LineOpacity,
			LineColorCopy = LineColor,
			bUseCustomTextureCopy = bUseCustomTexture,
			bUsePixelUnitCopy = bUsePixelUnit,
			bBuffersInitializedCopy = bBuffersInitialized](FRHICommandList& RHICmdList)
			{
				if (SceneProxyCopy.IsValid())
				{
					SceneProxyCopy->UpdateParameters_RenderThread(
						GPULineDataCopy,
						CustomTextureCopy,
						LineWidthCopy,
						LineOpacityCopy,
						LineColorCopy,
						bUseCustomTextureCopy,
						bUsePixelUnitCopy,
						bBuffersInitializedCopy);
				}
			});
		bBuffersInitialized = true;
	}
}
	
void USurfaceLineComponent::DestroySceneProxy()
{
	if (SceneProxy.IsValid())
	{
		SceneProxy.Reset();
	}
}

void USurfaceLineComponent::MarkGeometryDataDirty()
{
	bBuffersInitialized = false;
}
