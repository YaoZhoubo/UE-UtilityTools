// Copyright(c) 2025 YaoZhoubo. All rights reserved.

#include "BVHDrawer/BVHComponent.h"

#include "PolygonsRenderer.h"


DEFINE_LOG_CATEGORY_STATIC(LogBVHComponent, Log, All);

UBVHComponent::UBVHComponent()
{
	PrimaryComponentTick.bCanEverTick = true;

	NodesDataTexture = nullptr;
	SegmentsDataTexture = nullptr;
}

void UBVHComponent::BeginPlay()
{
	Super::BeginPlay();
}

void UBVHComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
}

#if WITH_EDITOR
void UBVHComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	FName PropertyName = (PropertyChangedEvent.Property != nullptr) ?
		PropertyChangedEvent.Property->GetFName() : NAME_None;

	// BuildConfig改变时，更新数据纹理
	if (PropertyName == GET_MEMBER_NAME_CHECKED(UBVHComponent, BuildConfig))
	{
		AsyncBuildBVHData();
	}
}
#endif

void UBVHComponent::SetPolygons(const TArray<FPolygon>& InPolygons)
{
	Polygons = InPolygons;

	AsyncBuildBVHData();
}

void UBVHComponent::ClearPolygons()
{
	Polygons.Empty();
}

void UBVHComponent::GenerateRandomTestData()
{
	ClearPolygons();

	// 计算网格划分：尽量接近正方形的网格
	int32 GridSize = FMath::CeilToInt(FMath::Sqrt(static_cast<float>(RandomPolygonCount)));

	// 计算每个单元格的大小
	float CellSize = (2.0f * RandomGenerationRadius) / GridSize;
	float HalfCellSize = CellSize * 0.5f;

	for (int32 Count = 0; Count < RandomPolygonCount; ++Count)
	{
		FPolygon NewPolygon;

		// 计算当前多边形所在的网格位置
		int32 GridX = Count % GridSize;
		int32 GridY = Count / GridSize;

		// 计算单元格的中心位置
		float CenterX = -RandomGenerationRadius + (GridX + 0.5f) * CellSize;
		float CenterY = -RandomGenerationRadius + (GridY + 0.5f) * CellSize;
		FVector CellCenter = FVector(CenterX, CenterY, 100.f);

		if (RandomPointsPerPolygon < 3)
		{
			// 如果点数少于3，生成随机散点
			for (int32 i = 0; i < RandomPointsPerPolygon; ++i)
			{
				FVector RandomPoint = CellCenter + FVector(
					FMath::FRandRange(-HalfCellSize, HalfCellSize),
					FMath::FRandRange(-HalfCellSize, HalfCellSize),
					100.f
				);
				NewPolygon.Vertices.Add(RandomPoint);
			}
		}
		else
		{
			// 生成凸多边形点（按角度排序）
			TArray<float> Angles;
			TArray<FVector> UnsortedPoints;

			for (int32 i = 0; i < RandomPointsPerPolygon; ++i)
			{
				float Angle = FMath::FRandRange(0.0f, 2.0f * PI);
				float Distance = FMath::FRandRange(0.0f, HalfCellSize * 0.9f);

				FVector Point = CellCenter + FVector(
					FMath::Cos(Angle) * Distance,
					FMath::Sin(Angle) * Distance,
					100.f
				);

				Angles.Add(Angle);
				UnsortedPoints.Add(Point);
			}

			// 按角度排序
			for (int32 i = 0; i < RandomPointsPerPolygon - 1; ++i)
			{
				for (int32 j = i + 1; j < RandomPointsPerPolygon; ++j)
				{
					if (Angles[i] > Angles[j])
					{
						Angles.Swap(i, j);
						UnsortedPoints.Swap(i, j);
					}
				}
			}

			NewPolygon.Vertices = UnsortedPoints;
		}

		Polygons.Add(NewPolygon);
	}

	AsyncBuildBVHData();
}

void UBVHComponent::SetLineColor(const FLinearColor& InLineColor)
{
	LineColor = InLineColor;

	UpdateSceneProxy();
}

void UBVHComponent::SetLineOpacity(const float InLineOpacity)
{
	LineOpacity = InLineOpacity;

	UpdateSceneProxy();
}

void UBVHComponent::SetLineWidth(const float InLineWidth)
{
	LineWidth = InLineWidth;

	UpdateSceneProxy();
}

void UBVHComponent::AsyncBuildBVHData()
{
	TWeakObjectPtr<UBVHComponent> WeakThis(this);

	AsyncTask(ENamedThreads::AnyThread,
		[WeakThis]()
		{
			if (WeakThis->Polygons.Num() == 0)
			{
				UE_LOG(LogBVHComponent, Warning, TEXT("Polygons为空"));
				return;
			}

			// 记录构建开始时间
			double BuildStartTime = FPlatformTime::Seconds();

			// 转换为BVH需要的格式
			TArray<TArray<FVector>> PolygonsForBVH;
			for (const FPolygon& Polygon : WeakThis->Polygons)
			{
				PolygonsForBVH.Add(Polygon.Vertices);
			}

			TSharedPtr<FPolygonBVH> NewBVHAccelerator = MakeShared<FPolygonBVH>(PolygonsForBVH, WeakThis->BuildConfig.LeafPrimitiveLimit);
			if (!NewBVHAccelerator.IsValid())
			{
				return;
			}
			NewBVHAccelerator->Build();

			if (!WeakThis.IsValid())
			{
				UE_LOG(LogBVHComponent, Warning, TEXT("组件已销毁，取消AsyncBuildBVHData"));
				return;
			}

			// 记录构建耗时
			WeakThis->BVHStats.BuildTimeMs = (FPlatformTime::Seconds() - BuildStartTime) * 1000.0;

			NewBVHAccelerator->GetStats(WeakThis->BVHStats.NumNodes, WeakThis->BVHStats.NumLeaves, WeakThis->BVHStats.MaxDepth, WeakThis->BVHStats.MemoryUsageMB);

			// 确保在游戏线程创建纹理
			AsyncTask(ENamedThreads::GameThread,
				[WeakThis, NewBVHAccelerator]()
				{
					FGPUBVHData NewBVHGPUData;
					if (!WeakThis->NodesDataTexture)
					{
						WeakThis->NodesDataTexture = WeakThis->CreateDefaultTextureForBVH(TEXT("NodesDataTexture"));
					}
					if (!WeakThis->SegmentsDataTexture)
					{
						WeakThis->SegmentsDataTexture = WeakThis->CreateDefaultTextureForBVH(TEXT("SegmentsDataTexture"));
					}

					FBVHGPUConverter::ConvertToRenderTargets(*NewBVHAccelerator, WeakThis->NodesDataTexture, WeakThis->SegmentsDataTexture);
					NewBVHAccelerator->GetStats(WeakThis->BVHStats.NumNodes, WeakThis->BVHStats.NumLeaves, WeakThis->BVHStats.MaxDepth, WeakThis->BVHStats.MemoryUsageMB);

					if (WeakThis->AfterBuildBVHData.IsBound())
					{
						WeakThis->AfterBuildBVHData.Broadcast(WeakThis->NodesDataTexture, WeakThis->SegmentsDataTexture, WeakThis->LineWidth, WeakThis->LineOpacity, WeakThis->LineColor);
					}

					WeakThis->UpdateSceneProxy();
				}
			);
		}
	);
}

UTextureRenderTarget2D* UBVHComponent::CreateDefaultTextureForBVH(const FString& TextureName)
{
	UTextureRenderTarget2D* RenderTarget = NewObject<UTextureRenderTarget2D>(this, *TextureName, RF_Transient);

	if (RenderTarget)
	{
		RenderTarget->InitAutoFormat(1, 1);
		RenderTarget->RenderTargetFormat = RTF_RGBA32f; // 对应GPU数据格式
		RenderTarget->ClearColor = FLinearColor::Black;
		RenderTarget->TargetGamma = 0.0f; // 禁用gamma校正，保持线性空间
		RenderTarget->UpdateResource();
	}

	return RenderTarget;
}

void UBVHComponent::CreateSceneProxy()
{
	check(IsInGameThread());
	RenderParameters = new FPolygonsSceneProxy(NodesDataTexture, SegmentsDataTexture, CustomTexture, LineWidth, LineOpacity, LineColor);
}

void UBVHComponent::UpdateSceneProxy()
{
	if (RenderParameters)
	{
		// 更新渲染代理参数
		RenderParameters->UpdateParameters(NodesDataTexture, SegmentsDataTexture, CustomTexture, LineWidth, LineOpacity, LineColor);
	}
}

void UBVHComponent::DestroySceneProxy()
{
	check(IsInGameThread());
	if (RenderParameters)
	{
		delete RenderParameters;
		RenderParameters = nullptr;
	}
}

void UBVHComponent::CreateRenderState_Concurrent(FRegisterComponentContext* Context)
{
	Super::CreateRenderState_Concurrent(Context);

	CreateSceneProxy();

	FPolygonsRenderManager* PolygonsRenderManager = FPolygonsRenderManager::Get();
	PolygonsRenderManager->RegisterSceneProxy(RenderParameters);
	PolygonsRenderManager->BeginRendering();
}

void UBVHComponent::DestroyRenderState_Concurrent()
{
	FPolygonsRenderManager* PolygonsRenderManager = FPolygonsRenderManager::Get();
	PolygonsRenderManager->UnregisterSceneProxy(RenderParameters);
	PolygonsRenderManager->EndRendering();

	DestroySceneProxy();

	Super::DestroyRenderState_Concurrent();
}