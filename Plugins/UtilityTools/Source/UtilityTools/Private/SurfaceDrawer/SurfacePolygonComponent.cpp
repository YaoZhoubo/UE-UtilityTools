#include "SurfaceDrawer/SurfacePolygonComponent.h"

#include "SurfaceDrawer/SurfacePolygonBuilder.h"
#include "SurfaceDrawer/SurfacePolygonRenderer.h"


DEFINE_LOG_CATEGORY_STATIC(LogSurfacePolygonComponent, Log, All);

USurfacePolygonComponent::USurfacePolygonComponent()
{
	PrimaryComponentTick.bCanEverTick = true;

	IsAsyncBuilding.store(false);

	// 初始化渲染默认参数
	Opacity = 0.5f;
	Color = FLinearColor::Green;
	bBuffersInitialized = false;
}

void USurfacePolygonComponent::SetTriangles(const TArray<FTriangle>& InTriangles)
{
	AsyncBuildBVHData(InTriangles);
}

void USurfacePolygonComponent::SetProperties(float InOpacity, const FLinearColor& InColor)
{
	Opacity = InOpacity;
	Color = InColor;

	MarkRenderStateDirty();
}

void USurfacePolygonComponent::SetOpacity(const float InOpacity)
{
	Opacity = InOpacity;

	MarkRenderStateDirty();
}

void USurfacePolygonComponent::SetColor(const FLinearColor& InColor)
{
	Color = InColor;

	MarkRenderStateDirty();
}

void USurfacePolygonComponent::ClearTriangles()
{
	AsyncBuildBVHData(TArray<FTriangle>());

	MarkGeometryDataDirty();
}

void USurfacePolygonComponent::OnRegister()
{
	Super::OnRegister();

	CreateSceneProxy();

	// Game 或者 PIE模式下注册代理
	UWorld* World = GetWorld();
	if (World && (World->WorldType == EWorldType::Game || World->WorldType == EWorldType::PIE))
	{
		FSurfacePolygonRenderManager* RenderManager = FSurfacePolygonRenderManager::Get();
		RenderManager->RegisterSceneProxy(SceneProxy);
	}
}

void USurfacePolygonComponent::OnUnregister()
{
	FSurfacePolygonRenderManager* SurfacePolygonRenderManager = FSurfacePolygonRenderManager::Get();
	SurfacePolygonRenderManager->UnregisterSceneProxy(SceneProxy->GetProxyId());
	DestroySceneProxy();

	Super::OnUnregister();
}

void USurfacePolygonComponent::CreateRenderState_Concurrent(FRegisterComponentContext* Context)
{
	Super::CreateRenderState_Concurrent(Context);

	if (SceneProxy.IsValid())
	{
		UpdateSceneProxy();
	}
}

void USurfacePolygonComponent::DestroyRenderState_Concurrent()
{
	Super::DestroyRenderState_Concurrent();
}

bool USurfacePolygonComponent::ShouldCreateRenderState() const
{
	return true;
}

void USurfacePolygonComponent::AsyncBuildBVHData(const TArray<FTriangle>& InTriangles)
{
	if (InTriangles.Num() == 0)
	{
		UE_LOG(LogSurfacePolygonComponent, Warning, TEXT("Triangles为空，跳过构建"));
		GPUPolygonData.Reset();

		MarkGeometryDataDirty();
		return;
	}

	if (IsAsyncBuilding.load())
	{
		UE_LOG(LogSurfacePolygonComponent, Warning, TEXT("正在进行上一次的AsyncBuildBVHData请求，跳过构建"));
		return;
	}

	IsAsyncBuilding.store(true);

	TWeakObjectPtr<USurfacePolygonComponent> WeakThis(this);

	AsyncTask(ENamedThreads::AnyThread,
		[WeakThis, InTriangles]()
		{
			TSharedPtr<FPolygonBVHBuilder> NewPolygonBVHBuilder = MakeShared<FPolygonBVHBuilder>(InTriangles, WeakThis->BVHBuildConfig);
			NewPolygonBVHBuilder->Build();

			if (!WeakThis.IsValid())
			{
				UE_LOG(LogSurfacePolygonComponent, Warning, TEXT("组件已销毁，取消AsyncBuildBVHData"));
				return;
			}

			NewPolygonBVHBuilder->GetStats(WeakThis->BVHStats);

			// 转换为GPU数据
			TSharedPtr<FGPUPolygonData> NewGPUPolygonData = MakeShared<FGPUPolygonData>();
			FPolygonGPUConverter::ConvertToGPUData(*NewPolygonBVHBuilder, *NewGPUPolygonData);
			WeakThis->GPUPolygonData = NewGPUPolygonData;
			WeakThis->MarkGeometryDataDirty();
			WeakThis->MarkRenderStateDirty();

			WeakThis->IsAsyncBuilding.store(false);
		}
	);
}

void USurfacePolygonComponent::CreateSceneProxy()
{
	check(IsInGameThread());

	SceneProxy = MakeShared<FSurfacePolygonSceneProxy>(
		GPUPolygonData,
		Opacity,
		Color);
}

void USurfacePolygonComponent::UpdateSceneProxy()
{
	if (SceneProxy.IsValid())
	{
		// 渲染线程更新参数
		ENQUEUE_RENDER_COMMAND(UpdateSceneProxyCommand)(
			[SceneProxyCopy = SceneProxy,
			GPUPolygonDataCopy = GPUPolygonData,
			OpacityCopy = Opacity,
			ColorCopy = Color,
			bBuffersInitializedCopy = bBuffersInitialized](FRHICommandList& RHICmdList)
			{
				if (SceneProxyCopy.IsValid())
				{
					SceneProxyCopy->UpdateParameters_RenderThread(
						GPUPolygonDataCopy,
						OpacityCopy,
						ColorCopy,
						bBuffersInitializedCopy);
				}
			});
		bBuffersInitialized = true;
	}
}

void USurfacePolygonComponent::DestroySceneProxy()
{
	if (SceneProxy.IsValid())
	{
		SceneProxy.Reset();
	}
}

void USurfacePolygonComponent::MarkGeometryDataDirty()
{
	bBuffersInitialized = false;
}
