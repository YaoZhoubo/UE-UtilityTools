// Copyright(c) 2025 YaoZhoubo. All rights reserved.

#include "BVHDrawer/BVHAccelerator.h"


DEFINE_LOG_CATEGORY_STATIC(LogBVHAccelerator, Log, All);

FPolygonBVH::FPolygonBVH(const TArray<TArray<FVector>>& InPolygons, int32 InLeafSegmentLimit)
	: Root(nullptr), LeafSegmentLimit(InLeafSegmentLimit)
{
	// 遍历所有多边形
	for (int32 PolyIndex = 0; PolyIndex < InPolygons.Num(); ++PolyIndex)
	{
		const TArray<FVector>& Polygon = InPolygons[PolyIndex];
		if (Polygon.Num() < 2)
		{
			UE_LOG(LogBVHAccelerator, Warning, TEXT("多边形 %d 顶点数不足2个，已跳过"), PolyIndex);
			continue;
		}

		// 将多边形的边提取为线段（闭合多边形）
		for (int32 i = 0; i < Polygon.Num(); ++i)
		{
			int32 NextIndex = (i + 1) % Polygon.Num();
			AllSegments.Add(FSegment(Polygon[i], Polygon[NextIndex], PolyIndex));
		}
	}

	UE_LOG(LogBVHAccelerator, Log, TEXT("初始化完成，共 %d 个线段"), AllSegments.Num());
}

FPolygonBVH::~FPolygonBVH()
{
	if (Root)
	{
		delete Root;
		Root = nullptr;
	}
}

void FPolygonBVH::Build()
{
	if (AllSegments.Num() == 0)
	{
		UE_LOG(LogBVHAccelerator, Warning, TEXT("没有线段可构建BVH"));
		return;
	}

	double StartTime = FPlatformTime::Seconds();

	TArray<FSegment> SegmentsToBuild = AllSegments;
	Root = BuildRecursive(SegmentsToBuild, 0); // 从根节点(0)开始递归构建

	double BuildTimeMs = (FPlatformTime::Seconds() - StartTime) * 1000.0;
	UE_LOG(LogBVHAccelerator, Log, TEXT("BVH构建完成，耗时: %.2f ms"), BuildTimeMs);
}

void FPolygonBVH::GetStats(int32& OutNumNodes, int32& OutNumLeaves, int32& OutMaxDepth, float& OutMemoryUsageMB) const
{
	OutNumNodes = 0;
	OutNumLeaves = 0;
	OutMaxDepth = 0;
	OutMemoryUsageMB = 0;

	if (!Root)
	{
		return;
	}

	// 递归遍历BVH树统计信息
	GetStatsRecursive(Root, 0, OutNumNodes, OutNumLeaves, OutMaxDepth, OutMemoryUsageMB);

	// 转换为MB
	OutMemoryUsageMB = OutMemoryUsageMB / (1024.0f * 1024.0f);
}

float FPolygonBVH::QueryClosestDistance(const FVector& Point, float InitialMaxDistance /*= FLT_MAX*/) const
{
	if (!Root)
	{
		UE_LOG(LogBVHAccelerator, Warning, TEXT("BVH树还没有构建， 请先调用Build()."));
		return FLT_MAX;
	}

	if (InitialMaxDistance <= 0.0f)
	{
		InitialMaxDistance = FLT_MAX;
	}

	return QueryClosestDistanceRecursive(Root, Point, InitialMaxDistance);
}

float FPolygonBVH::QueryClosestDistanceRecursive(const FBVHNode* Node, const FVector& Point, float CurrentMinDistance) const
{
	if (!Node) return CurrentMinDistance;

	// 计算点到节点包围盒的最短距离（快速估计）
	float DistanceToBox = FMath::Sqrt(Node->BoundingBox.ComputeSquaredDistanceToPoint(Point));
	if (DistanceToBox >= CurrentMinDistance)
	{
		return CurrentMinDistance; // 不可能有更近的线段，直接跳过
	}

	if (Node->bIsLeaf)
	{
		// 叶子节点：计算点到所有线段的精确距离
		for (const FSegment& Segment : Node->Segments)
		{
			float Distance = Segment.DistanceToPoint(Point);
			if (Distance < CurrentMinDistance)
			{
				CurrentMinDistance = Distance;
			}
		}
	}
	else
	{
		// 内部节点：先处理可能更近的子节点
		// 计算点到左右子节点包围盒的距离
		float LeftBoxDist = FMath::Sqrt(Node->LeftChild->BoundingBox.ComputeSquaredDistanceToPoint(Point));
		float RightBoxDist = FMath::Sqrt(Node->RightChild->BoundingBox.ComputeSquaredDistanceToPoint(Point));

		// 按距离由近到远处理，提高剪枝效率
		if (LeftBoxDist < RightBoxDist)
		{
			CurrentMinDistance = QueryClosestDistanceRecursive(Node->LeftChild, Point, CurrentMinDistance);
			CurrentMinDistance = QueryClosestDistanceRecursive(Node->RightChild, Point, CurrentMinDistance);
		}
		else
		{
			CurrentMinDistance = QueryClosestDistanceRecursive(Node->RightChild, Point, CurrentMinDistance);
			CurrentMinDistance = QueryClosestDistanceRecursive(Node->LeftChild, Point, CurrentMinDistance);
		}
	}

	return CurrentMinDistance;
}

void FPolygonBVH::GetStatsRecursive(const FBVHNode* Node, int32 CurrentDepth, int32& OutNumNodes, int32& OutNumLeaves, int32& OutMaxDepth, float& OutMemoryUsageMB) const
{
	if (!Node)
	{
		return;
	}

	// 统计节点数量、内存
	OutNumNodes++;
	OutMemoryUsageMB += sizeof(FBVHNode);

	// 更新最大深度
	if (CurrentDepth > OutMaxDepth)
	{
		OutMaxDepth = CurrentDepth;
	}

	if (Node->bIsLeaf)
	{
		// 叶子节点统计、线段内存
		OutNumLeaves++;
		OutMemoryUsageMB += sizeof(FSegment) * Node->Segments.Num();
	}
	else
	{
		// 递归遍历子节点
		GetStatsRecursive(Node->LeftChild, CurrentDepth + 1, OutNumNodes, OutNumLeaves, OutMaxDepth, OutMemoryUsageMB);
		GetStatsRecursive(Node->RightChild, CurrentDepth + 1, OutNumNodes, OutNumLeaves, OutMaxDepth, OutMemoryUsageMB);
	}
}

FBVHNode* FPolygonBVH::BuildRecursive(TArray<FSegment>& Segments, int32 Depth)
{
	FBVHNode* Node = new FBVHNode();

	// 计算当前节点所有线段的联合包围盒
	FBox UnionBox;
	for (const FSegment& Segment : Segments)
	{
		UnionBox += Segment.GetBoundingBox();
	}
	Node->BoundingBox = UnionBox;

	// 检查是否达到叶子节点条件，如果线段数量少于阈值，创建叶子节点
	if (Segments.Num() <= LeafSegmentLimit)
	{
		Node->bIsLeaf = true;
		Node->Segments = Segments;
		return Node;
	}

	// 使用SAH策略寻找最佳分割平面
	int32 BestAxis = -1; // 0=X, 1=Y, 2=Z
	float BestSplitValue = 0.0f;
	if (FindBestSplitPlane(Segments, BestAxis, BestSplitValue))
	{
		// 根据分割平面将线段分配到左右子树
		TArray<FSegment> LeftSegments, RightSegments;

		for (const FSegment& Segment : Segments)
		{
			FBox SegmentBox = Segment.GetBoundingBox();
			float Center = SegmentBox.GetCenter()[BestAxis]; // 线段中心在分割轴上的坐标

			if (Center < BestSplitValue)
			{
				LeftSegments.Add(Segment);
			}
			else
			{
				RightSegments.Add(Segment);
			}
		}

		// 左右线段都存在的情况下，创建内部节点（内部节点必须有两个子节点）
		if (LeftSegments.Num() > 0 && RightSegments.Num() > 0)
		{
			Node->bIsLeaf = false;
			Node->LeftChild = BuildRecursive(LeftSegments, Depth + 1);
			Node->RightChild = BuildRecursive(RightSegments, Depth + 1);

			return Node;
		}
	}

	// 无法有效分割或分割后某边为空，创建为叶子节点
	Node->bIsLeaf = true;
	Node->Segments = Segments;
	return Node;
}

bool FPolygonBVH::FindBestSplitPlane(const TArray<FSegment>& Segments, int32& OutBestAxis, float& OutBestSplitValue)
{
	// 初始化最佳代价为最大值
	float BestCost = FLT_MAX; // 最小表面积阈值
	OutBestAxis = -1;
	OutBestSplitValue = 0.0f;

	// 计算包围盒表面积的辅助函数
	auto ComputeSurfaceArea = [](const FBox& Box) -> float
		{
			if (!Box.IsValid) return 0.0f;
			FVector Size = Box.GetSize();
			return 2.0f * (Size.X * Size.Y + Size.X * Size.Z + Size.Y * Size.Z);
		};

	// 尝试三个轴(X, Y, Z)
	for (int32 Axis = 0; Axis < 3; ++Axis)
	{
		// 收集所有线段中心点在该轴上的坐标
		TArray<float> Centers;
		for (const FSegment& Segment : Segments)
		{
			FBox Box = Segment.GetBoundingBox();
			Centers.Add(Box.GetCenter()[Axis]);
		}

		// 排序以便选择分割点
		Centers.Sort(); 

		// 尝试多个候选分割点
		int32 NumCandidates = FMath::Min(5, Centers.Num());
		for (int32 i = 0; i < NumCandidates; ++i)
		{
			int32 SplitIndex = i * (Centers.Num() - 1) / (NumCandidates - 1);
			float SplitValue = Centers[SplitIndex];

			// 计算分割后的包围盒和线段数量
			FBox LeftBox(ForceInit), RightBox(ForceInit);
			int32 LeftCount = 0, RightCount = 0;

			for (const FSegment& Segment : Segments)
			{
				FBox SegmentBox = Segment.GetBoundingBox();
				float Center = SegmentBox.GetCenter()[Axis];

				if (Center < SplitValue)
				{
					LeftBox += SegmentBox;
					LeftCount++;
				}
				else
				{
					RightBox += SegmentBox;
					RightCount++;
				}
			}

			// 计算SAH代价
			float LeftArea = ComputeSurfaceArea(LeftBox);
			float RightArea = ComputeSurfaceArea(RightBox);
			float TotalArea = LeftArea + RightArea;

			const float TraversalCost = 1.0f;
			const float IntersectCost = 1.0f;

			float Cost = TraversalCost +
				(LeftArea / TotalArea) * LeftCount * IntersectCost +
				(RightArea / TotalArea) * RightCount * IntersectCost;

			if (Cost < BestCost && LeftCount > 0 && RightCount > 0)
			{
				BestCost = Cost;
				OutBestAxis = Axis;
				OutBestSplitValue = SplitValue;
			}
		}
	}

	return (BestCost < FLT_MAX);
}

DEFINE_LOG_CATEGORY_STATIC(LogBVHGPUConverter, Log, All);

bool FBVHGPUConverter::ConvertToGPUData(const FPolygonBVH& BVH, FGPUBVHData& OutGPUData)
{
	if (!BVH.IsBuilt())
	{
		UE_LOG(LogBVHGPUConverter, Warning, TEXT("BVH树未构建, 不能转换为GPU数据"));
		return false;
	}

	OutGPUData.Reset();

	// 第一步：收集节点数据
	OutGPUData.RootNodeIndex = CollectNodesRecursive(BVH.Root, OutGPUData.Nodes);

	// 第二步：收集线段数据
	CollectSegmentsRecursive(BVH.Root, OutGPUData.Segments);

	// 第三步：为叶子节点分配正确的线段索引
	AssignSegmentIndices(OutGPUData);

	//// 打印转换前的CPU数据
	//PrintCPUData(BVH);

	//// 打印转换后的GPU数据
	//PrintGPUData(OutGPUData);


	UE_LOG(LogBVHGPUConverter, Log, TEXT("BVH GPU 转换完成: %d nodes, %d segments"), OutGPUData.Nodes.Num(), OutGPUData.Segments.Num());

	return OutGPUData.IsValid();
}

bool FBVHGPUConverter::ConvertToRenderTargets(const FPolygonBVH& BVHData, UTextureRenderTarget2D* OutBVHDataRenderTarget, UTextureRenderTarget2D* OutSegmentDataRenderTarget)
{
	if (!BVHData.IsBuilt())
	{
		UE_LOG(LogBVHGPUConverter, Warning, TEXT("无效的GPU数据，无法转换到渲染目标"));
		return false;
	}

	FGPUBVHData NewBVHGPUData;
	if (!ConvertToGPUData(BVHData, NewBVHGPUData))
	{
		return false;
	}

	// 计算需要的纹理尺寸
	int32 BVHNodesCount = NewBVHGPUData.Nodes.Num();
	int32 SegmentsCount = NewBVHGPUData.Segments.Num();

	// 确保纹理尺寸合理（2的幂次方，且不超过最大尺寸）
	int32 BVHTextureWidth = FMath::RoundUpToPowerOfTwo(FMath::Max(16, (int32)FMath::Sqrt((float)BVHNodesCount * 3)));
	int32 BVHTextureHeight = BVHTextureWidth;

	int32 SegmentsTextureWidth = FMath::RoundUpToPowerOfTwo(FMath::Max(16, (int32)FMath::Sqrt((float)SegmentsCount * 2)));
	int32 SegmentsTextureHeight = SegmentsTextureWidth;

	// 限制最大尺寸（能容下130万个节点，LeafSegmentLimit * 65万个点）
	BVHTextureWidth = FMath::Min(BVHTextureWidth, 2048);
	BVHTextureHeight = FMath::Min(BVHTextureHeight, 2048);
	SegmentsTextureWidth = FMath::Min(SegmentsTextureWidth, 2048);
	SegmentsTextureHeight = FMath::Min(SegmentsTextureHeight, 2048);

	// 设置纹理
	if (OutBVHDataRenderTarget && OutBVHDataRenderTarget->SizeX != BVHTextureWidth && OutBVHDataRenderTarget->SizeY != BVHTextureHeight)
	{
		OutBVHDataRenderTarget->ResizeTarget(BVHTextureWidth, BVHTextureHeight);
	}
	if (OutSegmentDataRenderTarget && OutSegmentDataRenderTarget->SizeX != BVHTextureWidth && OutSegmentDataRenderTarget->SizeY != BVHTextureHeight)
	{
		OutSegmentDataRenderTarget->ResizeTarget(SegmentsTextureWidth, SegmentsTextureHeight);
	}

	// 填充纹理数据
	FillBVHRenderTarget(OutBVHDataRenderTarget, NewBVHGPUData);
	FillSegmentRenderTarget(OutSegmentDataRenderTarget, NewBVHGPUData);

	FlushRenderingCommands();

	UE_LOG(LogBVHGPUConverter, Log, TEXT("BVH数据已转换到纹理: BVH节点纹理(%dx%d), 线段纹理(%dx%d)"),
		BVHTextureWidth, BVHTextureHeight, SegmentsTextureWidth, SegmentsTextureHeight);

	return true;
}

void FBVHGPUConverter::PrintCPUData(const FPolygonBVH& BVH)
{
	if (!BVH.IsBuilt())
	{
		UE_LOG(LogBVHGPUConverter, Warning, TEXT("CPU BVH数据未构建"));
		return;
	}

	UE_LOG(LogBVHGPUConverter, Log, TEXT("========== CPU BVH数据 =========="));
	UE_LOG(LogBVHGPUConverter, Log, TEXT("总线段数: %d"), BVH.AllSegments.Num());

	// 打印所有线段
	UE_LOG(LogBVHGPUConverter, Log, TEXT("--- 所有线段 ---"));
	for (int32 i = 0; i < BVH.AllSegments.Num(); ++i)
	{
		const FSegment& Segment = BVH.AllSegments[i];
		UE_LOG(LogBVHGPUConverter, Log, TEXT("线段[%d]: 起点(%f, %f, %f), 终点(%f, %f, %f), 多边形索引: %d"),
			i, Segment.Start.X, Segment.Start.Y, Segment.Start.Z,
			Segment.End.X, Segment.End.Y, Segment.End.Z, Segment.Index);
	}

	// 递归打印节点树
	UE_LOG(LogBVHGPUConverter, Log, TEXT("--- 节点树结构 ---"));
	PrintCPUNodeRecursive(BVH.Root);
}

void FBVHGPUConverter::PrintGPUData(const FGPUBVHData& GPUData)
{
	UE_LOG(LogBVHGPUConverter, Log, TEXT("========== GPU BVH数据 =========="));
	UE_LOG(LogBVHGPUConverter, Log, TEXT("根节点索引: %d"), GPUData.RootNodeIndex);
	UE_LOG(LogBVHGPUConverter, Log, TEXT("总节点数: %d"), GPUData.Nodes.Num());
	UE_LOG(LogBVHGPUConverter, Log, TEXT("总线段数: %d"), GPUData.Segments.Num());

	// 打印所有GPU线段
	UE_LOG(LogBVHGPUConverter, Log, TEXT("--- GPU线段数据 ---"));
	for (int32 i = 0; i < GPUData.Segments.Num(); ++i)
	{
		const FGPUSegment& Segment = GPUData.Segments[i];
		UE_LOG(LogBVHGPUConverter, Log, TEXT("GPU线段[%d]: 起点(%f, %f, %f), 终点(%f, %f, %f), 多边形索引: %d"),
			i, Segment.Start.X, Segment.Start.Y, Segment.Start.Z,
			Segment.End.X, Segment.End.Y, Segment.End.Z, Segment.PolygonIndex);
	}

	// 打印GPU节点数组
	UE_LOG(LogBVHGPUConverter, Log, TEXT("--- GPU节点数组 ---"));
	for (int32 i = 0; i < GPUData.Nodes.Num(); ++i)
	{
		const FGPUBVHNode& Node = GPUData.Nodes[i];
		UE_LOG(LogBVHGPUConverter, Log, TEXT("GPU节点[%d]: 包围盒Min(%f, %f, %f) Max(%f, %f, %f)"),
			i, Node.MinExtent.X, Node.MinExtent.Y, Node.MinExtent.Z,
			Node.MaxExtent.X, Node.MaxExtent.Y, Node.MaxExtent.Z);
		UE_LOG(LogBVHGPUConverter, Log, TEXT("       左子节点: %d, 右子节点: %d, 线段起始索引: %d, 线段数量: %d"),
			Node.LeftChild, Node.RightChild, Node.SegmentStart, Node.SegmentCount);
	}

	// 递归打印GPU节点树
	UE_LOG(LogBVHGPUConverter, Log, TEXT("--- GPU节点树结构 ---"));
	if (GPUData.RootNodeIndex >= 0 && GPUData.RootNodeIndex < GPUData.Nodes.Num())
	{
		PrintGPUNodeRecursive(GPUData, GPUData.RootNodeIndex);
	}
}

void FBVHGPUConverter::CollectSegmentsRecursive(const FBVHNode* Node, TArray<FGPUSegment>& OutSegments)
{
	if (!Node) return;

	if (Node->bIsLeaf)
	{
		// 叶子节点：收集其所有线段
		for (const FSegment& Segment : Node->Segments)
		{
			FGPUSegment GPUSegment;
			GPUSegment.Start = Segment.Start;
			GPUSegment.End = Segment.End;
			GPUSegment.PolygonIndex = Segment.Index;
			OutSegments.Add(GPUSegment);
		}
	}
	else
	{
		// 内部节点：递归处理子节点
		CollectSegmentsRecursive(Node->LeftChild, OutSegments);
		CollectSegmentsRecursive(Node->RightChild, OutSegments);
	}
}

int32 FBVHGPUConverter::CollectNodesRecursive(const FBVHNode* CPUNode, TArray<FGPUBVHNode>& OutGPUNode)
{
	if (!CPUNode) return -1;

	FGPUBVHNode GPUBVHNode;
	GPUBVHNode.MinExtent = CPUNode->BoundingBox.Min;
	GPUBVHNode.MaxExtent = CPUNode->BoundingBox.Max;

	// 初始化所有字段
	GPUBVHNode.LeftChild = -1;
	GPUBVHNode.RightChild = -1;
	GPUBVHNode.SegmentStart = -1;
	GPUBVHNode.SegmentCount = 0;

	int32 CurrentNodeIndex = OutGPUNode.Num();
	OutGPUNode.Add(GPUBVHNode);

	if (CPUNode->bIsLeaf)
	{
		// 叶子节点：记录线段数量
		OutGPUNode[CurrentNodeIndex].SegmentCount = CPUNode->Segments.Num();
		// SegmentStart 会在 AssignSegmentIndices 中设置
	}
	else
	{
		// 内部节点：递归构建子节点
		int32 LeftChildIndex = CollectNodesRecursive(CPUNode->LeftChild, OutGPUNode);
		int32 RightChildIndex = CollectNodesRecursive(CPUNode->RightChild, OutGPUNode);

		OutGPUNode[CurrentNodeIndex].LeftChild = LeftChildIndex;
		OutGPUNode[CurrentNodeIndex].RightChild = RightChildIndex;
	}

	return CurrentNodeIndex;
}

void FBVHGPUConverter::AssignSegmentIndices(FGPUBVHData& GPUData)
{
	int32 CurrentSegmentIndex = 0;

	// 遍历所有节点，只处理叶子节点
	for (int32 i = 0; i < GPUData.Nodes.Num(); ++i)
	{
		FGPUBVHNode& Node = GPUData.Nodes[i];

		// 如果是叶子节点且有线段
		if (Node.LeftChild == -1 && Node.RightChild == -1 && Node.SegmentCount > 0)
		{
			Node.SegmentStart = CurrentSegmentIndex;
			CurrentSegmentIndex += Node.SegmentCount;

			// 验证：检查线段索引是否有效
			if (Node.SegmentStart + Node.SegmentCount > GPUData.Segments.Num())
			{
				UE_LOG(LogBVHGPUConverter, Warning, TEXT("线段索引超出范围: 节点 %d, 起始索引 %d, 数量 %d, 总线段数 %d"),
					i, Node.SegmentStart, Node.SegmentCount, GPUData.Segments.Num());
			}
		}
	}

	// 验证线段索引分配是否正确
	if (CurrentSegmentIndex != GPUData.Segments.Num())
	{
		UE_LOG(LogBVHGPUConverter, Warning, TEXT("线段索引分配不正确: 期望 %d 条线段, 但得到了 %d 条"),
			GPUData.Segments.Num(), CurrentSegmentIndex);
	}
}

UTextureRenderTarget2D* FBVHGPUConverter::CreateRenderTarget(UObject* Outer, int32 Width, int32 Height, const FString& RenderTargetName)
{
	// 创建渲染目标对象
	UTextureRenderTarget2D* RenderTarget = NewObject<UTextureRenderTarget2D>(Outer, *RenderTargetName);

	if (RenderTarget)
	{
		// 设置渲染目标属性
		RenderTarget->RenderTargetFormat = RTF_RGBA32f;
		RenderTarget->InitAutoFormat(Width, Height);
		RenderTarget->ClearColor = FLinearColor::Black;
		RenderTarget->TargetGamma = 0.0f; // 禁用gamma校正，保持线性空间
		RenderTarget->UpdateResource();
		FlushRenderingCommands();
	}

	return RenderTarget;
}

void FBVHGPUConverter::FillBVHRenderTarget(UTextureRenderTarget2D* RenderTarget, const FGPUBVHData& BVHData)
{
	if (!RenderTarget)
	{
		return;
	}

	// 获取渲染目标尺寸
	int32 Width = RenderTarget->SizeX;
	int32 Height = RenderTarget->SizeY;

	// 创建临时缓冲区
	TArray<FLinearColor> TextureData;
	TextureData.Init(FLinearColor::Black, Width * Height);

	// 填充BVH节点数据（每个节点占用3个像素）
	const TArray<FGPUBVHNode>& Nodes = BVHData.Nodes;
	for (int32 i = 0; i < Nodes.Num(); i++)
	{
		int32 PixelIndex = i * 3;
		int32 X = PixelIndex % Width;
		int32 Y = PixelIndex / Width;

		if (Y >= Height) break;

		const FGPUBVHNode& Node = Nodes[i];

		// 像素0：MinExtent和左子节点
		if (X < Width && Y < Height)
		{
			FLinearColor& Pixel0 = TextureData[Y * Width + X];
			Pixel0.R = Node.MinExtent.X;
			Pixel0.G = Node.MinExtent.Y;
			Pixel0.B = Node.MinExtent.Z;
			Pixel0.A = (float)Node.LeftChild;
		}

		// 像素1：MaxExtent和右子节点
		PixelIndex = i * 3 + 1;
		X = PixelIndex % Width;
		Y = PixelIndex / Width;
		if (X < Width && Y < Height)
		{
			FLinearColor& Pixel1 = TextureData[Y * Width + X];
			Pixel1.R = Node.MaxExtent.X;
			Pixel1.G = Node.MaxExtent.Y;
			Pixel1.B = Node.MaxExtent.Z;
			Pixel1.A = (float)Node.RightChild;
		}

		// 像素2：线段信息和节点类型
		PixelIndex = i * 3 + 2;
		X = PixelIndex % Width;
		Y = PixelIndex / Width;
		if (X < Width && Y < Height)
		{
			FLinearColor& Pixel2 = TextureData[Y * Width + X];
			Pixel2.R = (float)Node.SegmentStart;
			Pixel2.G = (float)Node.SegmentCount;
			Pixel2.B = (Node.LeftChild == -1 && Node.RightChild == -1) ? 1.0f : 0.0f; // 叶子节点=1, 内部节点=0
			Pixel2.A = 0.0f; // 保留
		}
	}

	// 更新渲染目标
	FTextureRenderTargetResource* Resource = RenderTarget->GameThread_GetRenderTargetResource();
	if (Resource)
	{
		// 使用渲染命令更新纹理数据
		ENQUEUE_RENDER_COMMAND(UpdateBVHTextureData)(
			[Resource, Width, Height, LocalTextureData = TextureData](FRHICommandListImmediate& RHICmdList)
			{
				// 锁定纹理进行写入
				uint32 Stride = 0;
				uint8* Data = (uint8*)RHICmdList.LockTexture2D(
					Resource->GetRenderTargetTexture(),
					0,
					RLM_WriteOnly,
					Stride,
					false
				);

				if (Data)
				{
					// 计算每行数据大小
					const uint32 BytesPerPixel = sizeof(FLinearColor);
					const uint32 BytesPerRow = Width * BytesPerPixel;

					// 复制数据
					for (int32 Y = 0; Y < Height; Y++)
					{
						uint8* DestRow = Data + Y * Stride;
						const uint8* SrcRow = (uint8*)&LocalTextureData[Y * Width];
						FMemory::Memcpy(DestRow, SrcRow, BytesPerRow);
					}

					RHICmdList.UnlockTexture2D(Resource->GetRenderTargetTexture(), 0, false);
				}
			}
			);
	}
}

void FBVHGPUConverter::FillSegmentRenderTarget(UTextureRenderTarget2D* RenderTarget, const FGPUBVHData& BVHData)
{
	if (!RenderTarget)
	{
		return;
	}

	// 获取渲染目标尺寸
	int32 Width = RenderTarget->SizeX;
	int32 Height = RenderTarget->SizeY;

	// 创建临时缓冲区
	TArray<FLinearColor> TextureData;
	TextureData.Init(FLinearColor::Black, Width * Height);

	// 填充图元数据
	const TArray<FGPUSegment>& Segments = BVHData.Segments;
	for (int32 i = 0; i < Segments.Num(); i++)
	{
		int32 PixelIndex = i * 2; // 每个线段占用2个像素
		int32 X = PixelIndex % Width;
		int32 Y = PixelIndex / Width;

		if (Y >= Height) break;

		const FGPUSegment& Segment = Segments[i];

		// 像素0：线段起点和多边形索引
		if (X < Width && Y < Height)
		{
			FLinearColor& Pixel0 = TextureData[Y * Width + X];
			Pixel0.R = Segment.Start.X;
			Pixel0.G = Segment.Start.Y;
			Pixel0.B = Segment.Start.Z;
			Pixel0.A = (float)Segment.PolygonIndex;
		}

		// 像素1：线段终点
		PixelIndex = i * 2 + 1;
		X = PixelIndex % Width;
		Y = PixelIndex / Width;
		if (X < Width && Y < Height)
		{
			FLinearColor& Pixel1 = TextureData[Y * Width + X];
			Pixel1.R = Segment.End.X;
			Pixel1.G = Segment.End.Y;
			Pixel1.B = Segment.End.Z;
			Pixel1.A = 0.0f;
		}
	}

	// 更新渲染目标
	FTextureRenderTargetResource* Resource = RenderTarget->GameThread_GetRenderTargetResource();
	if (Resource)
	{
		ENQUEUE_RENDER_COMMAND(UpdateSegmentTextureData)(
			[Resource, Width, Height, LocalTextureData = TextureData](FRHICommandListImmediate& RHICmdList)
			{
				uint32 Stride = 0;
				uint8* Data = (uint8*)RHICmdList.LockTexture2D(
					Resource->GetRenderTargetTexture(),
					0,
					RLM_WriteOnly,
					Stride,
					false
				);

				if (Data)
				{
					const uint32 BytesPerPixel = sizeof(FLinearColor);
					const uint32 BytesPerRow = Width * BytesPerPixel;

					for (int32 Y = 0; Y < Height; Y++)
					{
						uint8* DestRow = Data + Y * Stride;
						const uint8* SrcRow = (uint8*)&LocalTextureData[Y * Width];
						FMemory::Memcpy(DestRow, SrcRow, BytesPerRow);
					}

					RHICmdList.UnlockTexture2D(Resource->GetRenderTargetTexture(), 0, false);
				}
			}
			);
	}
}

void FBVHGPUConverter::PrintCPUNodeRecursive(const FBVHNode* Node, int32 Depth /*= 0*/)
{
	if (!Node) return;

	FString Indent = FString::ChrN(Depth * 2, ' ');

	if (Node->bIsLeaf)
	{
		UE_LOG(LogBVHGPUConverter, Log, TEXT("%s叶子节点[深度%d]: 包围盒Min(%f, %f, %f) Max(%f, %f, %f), 线段数: %d"),
			*Indent, Depth,
			Node->BoundingBox.Min.X, Node->BoundingBox.Min.Y, Node->BoundingBox.Min.Z,
			Node->BoundingBox.Max.X, Node->BoundingBox.Max.Y, Node->BoundingBox.Max.Z,
			Node->Segments.Num());

		// 打印叶子节点中的线段
		for (int32 i = 0; i < Node->Segments.Num(); ++i)
		{
			const FSegment& Segment = Node->Segments[i];
			UE_LOG(LogBVHGPUConverter, Log, TEXT("%s  线段[%d]: 起点(%f, %f, %f), 终点(%f, %f, %f), 多边形索引: %d"),
				*Indent, i,
				Segment.Start.X, Segment.Start.Y, Segment.Start.Z,
				Segment.End.X, Segment.End.Y, Segment.End.Z, Segment.Index);
		}
	}
	else
	{
		UE_LOG(LogBVHGPUConverter, Log, TEXT("%s内部节点[深度%d]: 包围盒Min(%f, %f, %f) Max(%f, %f, %f)"),
			*Indent, Depth,
			Node->BoundingBox.Min.X, Node->BoundingBox.Min.Y, Node->BoundingBox.Min.Z,
			Node->BoundingBox.Max.X, Node->BoundingBox.Max.Y, Node->BoundingBox.Max.Z);

		// 递归打印子节点
		PrintCPUNodeRecursive(Node->LeftChild, Depth + 1);
		PrintCPUNodeRecursive(Node->RightChild, Depth + 1);
	}
}

void FBVHGPUConverter::PrintGPUNodeRecursive(const FGPUBVHData& GPUData, int32 NodeIndex, int32 Depth /*= 0*/)
{
	if (NodeIndex < 0 || NodeIndex >= GPUData.Nodes.Num()) return;

	const FGPUBVHNode& Node = GPUData.Nodes[NodeIndex];
	FString Indent = FString::ChrN(Depth * 2, ' ');

	if (Node.LeftChild == -1 && Node.RightChild == -1) // 叶子节点
	{
		UE_LOG(LogBVHGPUConverter, Log, TEXT("%sGPU叶子节点[索引%d, 深度%d]: 线段起始索引: %d, 线段数量: %d"),
			*Indent, NodeIndex, Depth, Node.SegmentStart, Node.SegmentCount);

		// 打印叶子节点对应的线段
		if (Node.SegmentStart >= 0 && Node.SegmentStart + Node.SegmentCount <= GPUData.Segments.Num())
		{
			for (int32 i = 0; i < Node.SegmentCount; ++i)
			{
				int32 SegmentIndex = Node.SegmentStart + i;
				const FGPUSegment& Segment = GPUData.Segments[SegmentIndex];
				UE_LOG(LogBVHGPUConverter, Log, TEXT("%s  GPU线段[%d]: 起点(%f, %f, %f), 终点(%f, %f, %f), 多边形索引: %d"),
					*Indent, SegmentIndex,
					Segment.Start.X, Segment.Start.Y, Segment.Start.Z,
					Segment.End.X, Segment.End.Y, Segment.End.Z, Segment.PolygonIndex);
			}
		}
	}
	else // 内部节点
	{
		UE_LOG(LogBVHGPUConverter, Log, TEXT("%sGPU内部节点[索引%d, 深度%d]: 左子节点: %d, 右子节点: %d"),
			*Indent, NodeIndex, Depth, Node.LeftChild, Node.RightChild);

		// 递归打印子节点
		PrintGPUNodeRecursive(GPUData, Node.LeftChild, Depth + 1);
		PrintGPUNodeRecursive(GPUData, Node.RightChild, Depth + 1);
	}
}
