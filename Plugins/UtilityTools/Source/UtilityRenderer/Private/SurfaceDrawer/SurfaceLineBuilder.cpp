#include "SurfaceDrawer/SurfaceLineBuilder.h"

#include "LineCluster.h"

	
DEFINE_LOG_CATEGORY_STATIC(LogSurfaceLineBuilder, Log, All);
	
FLineBVHNode::~FLineBVHNode()
{
	if (Cluster)
	{
		delete Cluster;
	}

	if (LeftChild)
	{
		delete LeftChild;
		LeftChild = nullptr;
	}
	if (RightChild)
	{
		delete RightChild;
		RightChild = nullptr;
	}
}

FLineBVHBuilder::FLineBVHBuilder(const TArray<FPolygon>& InPolygons, const FBVHBuildConfig& InBuildConfig)
	: Root(nullptr), BuildConfig(InBuildConfig), TotalSegments(0)
{
	// 遍历所有多边形，为每个多边形创建线段簇
	for (int32 PolyIndex = 0; PolyIndex < InPolygons.Num(); ++PolyIndex)
	{
		const FPolygon& Polygon = InPolygons[PolyIndex];
		if (Polygon.Vertices.Num() < 2)
		{
			UE_LOG(LogSurfaceLineBuilder, Warning, TEXT("多边形 %d 顶点数不足2个，已跳过"), PolyIndex);
			continue;
		}
		
		// 为当前多边形创建线段簇
		const int32 MaxSegmentsPerCluster = 128;
		const int32 NumCluster = Polygon.Vertices.Num() / (MaxSegmentsPerCluster + 1) + 1;

		for(int32 i = 0; i < NumCluster; ++i)
		{
			FSegmentCluster* NewCluster = new FSegmentCluster(PolyIndex);

			const int32 StartIdx = i * MaxSegmentsPerCluster;
			const int32 EndIdx = FMath::Min(StartIdx + MaxSegmentsPerCluster, Polygon.Vertices.Num() - 1);
			for (int32 j = StartIdx; j < EndIdx; ++j)
			{
				FSegment Segment(Polygon.Vertices[j], Polygon.Vertices[j + 1], PolyIndex);
				NewCluster->AddSegment(Segment);

				TotalSegments++;
			}
			NewCluster->SegmentNumPerLOD[0] = NewCluster->Segments.Num();

			// TODO: 已完成，但经测试，在采用分页策略前，增加额外的LOD数据会使显存压力过大，导致严重性能问题。
			// 无论是合并还是分页，粗略估计，最终的Segments总数建议不超过100万时考虑开启LOD
			//NewCluster->GenerateLODLevel();

			AllClusters.Add(NewCluster);
		}
	}
	// 打印初始统计信息
	UE_LOG(LogSurfaceLineBuilder, Log, TEXT("初始统计: 多边形数=%d, 簇数=%d, 总线段数=%d"), InPolygons.Num(), AllClusters.Num(), TotalSegments);
}
	
FLineBVHBuilder::~FLineBVHBuilder()
{
	if (Root)
	{
		delete Root;
		Root = nullptr;
	}

	AllClusters.Empty();
}
	
void FLineBVHBuilder::Build()
{
	if (AllClusters.Num() == 0)
	{
		UE_LOG(LogSurfaceLineBuilder, Warning, TEXT("没有Cluster可构建BVH"));
		return;
	}
	
	double StartTime = FPlatformTime::Seconds();
	
	TArray<FSegmentCluster*> ClustersToBuild = AllClusters;

	if (BuildConfig.Strategy == EBVHBuildStrategy::SAH) 
	{
		Root = BuildRecursive_SAH(ClustersToBuild, 0);
	}
	else
	{
		Root = BuildRecursive_Middle(ClustersToBuild, 0);
	}
	
	BuildTimeMs = (FPlatformTime::Seconds() - StartTime) * 1000.0;
	UE_LOG(LogSurfaceLineBuilder, Log, TEXT("BVH构建完成, 构建策略: %s, 耗时: %.2f ms"),
		(BuildConfig.Strategy == EBVHBuildStrategy::SAH) ? TEXT("SAH") : TEXT("Middle"),
		BuildTimeMs);
}
	
void FLineBVHBuilder::GetStats(FBVHStats& OutStats) const
{
	OutStats.NumNodes = 0;
	OutStats.NumLeaves = 0;
	OutStats.MaxDepth = 0;
	OutStats.MemoryUsageMB = 0.0f;
	OutStats.BuildTimeMs = BuildTimeMs;
	
	if (!Root)
	{
		return;
	}
	
	// 递归遍历BVH树统计信息
	GetStatsRecursive(Root, 0, OutStats);
}

void FLineBVHBuilder::GetStatsRecursive(const FLineBVHNode* Node, int32 CurrentDepth, FBVHStats& OutStats) const
{
	if (!Node)
	{
		return;
	}
	
	// 统计节点数量、内存
	OutStats.NumNodes++;
	OutStats.MemoryUsageMB += sizeof(FGPULineBVHNode) / (1024.0f * 1024.0f);
	
	// 更新最大深度
	if (CurrentDepth > OutStats.MaxDepth)
	{
		OutStats.MaxDepth = CurrentDepth;
	}
	
	if (Node->bIsLeaf)
	{
		// 叶子节点统计、线段内存
		OutStats.NumLeaves++;

		if (Node->Cluster)
		{
			OutStats.MemoryUsageMB += sizeof(FGPUSegment) * (Node->Cluster->Segments.Num() / (1024.0f * 1024.0f));
			OutStats.MemoryUsageMB += sizeof(FGPUSegmentCluster) / (1024.0f * 1024.0f);
		}
	}
	else
	{
		// 递归遍历子节点
		GetStatsRecursive(Node->LeftChild, CurrentDepth + 1, OutStats);
		GetStatsRecursive(Node->RightChild, CurrentDepth + 1, OutStats);
	}
}

FLineBVHNode* FLineBVHBuilder::BuildRecursive_Middle(const TArray<FSegmentCluster*>& InClusters, int32 Depth)
{
	FLineBVHNode* Node = new FLineBVHNode();

	// 计算联合包围盒
	FBox UnionBox(ForceInit);
	for (FSegmentCluster* Cluster : InClusters)
	{
		if (Cluster)
		{
			UnionBox += Cluster->BoundingBox;
		}
	}
	Node->BoundingBox = UnionBox;

	if (InClusters.Num() == 0)
	{
		return Node;
	}

	// 检查是否达到叶子节点条件（一个叶子节点只保存一个Cluster）
	if (InClusters.Num() == 1)
	{
		Node->bIsLeaf = true;
		Node->Cluster = InClusters[0];

		return Node;
	}

	// 选择最长的轴作为分割轴
	FVector BoxSize = UnionBox.GetSize();
	int32 SplitAxis = 0;
	if (BoxSize.Y > BoxSize.X) SplitAxis = 1;
	if (BoxSize.Z > BoxSize[SplitAxis]) SplitAxis = 2;

	// 计算所有多边形中心点在分割轴上的中位数
	TArray<float> Centers;
	for (FSegmentCluster* Cluster : InClusters)
	{
		if (Cluster)
		{
			Centers.Add(Cluster->BoundingBox.GetCenter()[SplitAxis]);
		}
	}

	// 排序并找到中位数
	Centers.Sort();
	float Median = Centers[Centers.Num() / 2];

	// 根据中位数分割多边形
	TArray<FSegmentCluster*> LeftClusters, RightClusters;

	for (FSegmentCluster* Cluster : InClusters)
	{
		if (!Cluster) 
		{
			continue;
		}

		float Center = Cluster->BoundingBox.GetCenter()[SplitAxis];

		if (Center < Median)
		{
			LeftClusters.Add(Cluster);
		}
		else
		{
			RightClusters.Add(Cluster);
		}
	}

	// 检查分割结果，避免无限递归
	if (LeftClusters.Num() == InClusters.Num() && RightClusters.Num() == 0)
	{
		// 所有三角形都在左边，弹出一个给右边
		RightClusters.Add(LeftClusters.Pop());
	}
	if (RightClusters.Num() == InClusters.Num() && LeftClusters.Num() == 0)
	{
		// 所有三角形都在右边，弹出一个给左边
		LeftClusters.Add(RightClusters.Pop());
	}

	// 递归构建子树
	Node->bIsLeaf = false;
	Node->LeftChild = BuildRecursive_Middle(LeftClusters, Depth + 1);
	Node->RightChild = BuildRecursive_Middle(RightClusters, Depth + 1);

	return Node;
}
	
FLineBVHNode* FLineBVHBuilder::BuildRecursive_SAH(const TArray<FSegmentCluster*>& InClusters, int32 Depth)
{
	FLineBVHNode* Node = new FLineBVHNode();

	// 计算联合包围盒
	FBox UnionBox(ForceInit);
	for (FSegmentCluster* Cluster : InClusters)
	{
		if (Cluster)
		{
			UnionBox += Cluster->BoundingBox;
		}
	}
	Node->BoundingBox = UnionBox;

	// 检查是否达到叶子节点条件（一个叶子节点只保存一个Cluster）
	if (InClusters.Num() == 1)
	{
		Node->bIsLeaf = true;
		Node->Cluster = InClusters[0];
		return Node;
	}

	// SAH参数
	const int32 NumBins = 32; // 分箱数量
	float BestCost = UE_MAX_FLT;
	int32 BestAxis = -1;
	float BestSplit = 0.0f;

	FVector BoxSize = UnionBox.GetSize();
	float UnionSurfaceArea = 2.0f * (BoxSize.X * BoxSize.Y + BoxSize.X * BoxSize.Z + BoxSize.Y * BoxSize.Z);

	// 遍历三个轴
	for (int32 Axis = 0; Axis < 3; ++Axis)
	{
		if (BoxSize[Axis] < KINDA_SMALL_NUMBER)
		{
			continue;
		}

		// 创建分箱
		struct FBin
		{
			FBox Bounds;
			int32 Count;
			FBin() : Bounds(ForceInit), Count(0) {}
		};
		TArray<FBin> Bins;
		Bins.SetNum(NumBins);

		float BinWidth = BoxSize[Axis] / NumBins;
		float AxisStart = UnionBox.Min[Axis];

		// 将簇分配到分箱中
		for (FSegmentCluster* Cluster : InClusters)
		{
			if (!Cluster)
			{
				continue;
			}

			float Center = Cluster->BoundingBox.GetCenter()[Axis];
			int32 BinIndex = FMath::Clamp(FMath::FloorToInt((Center - AxisStart) / BinWidth), 0, NumBins - 1);
			Bins[BinIndex].Bounds += Cluster->BoundingBox;
			Bins[BinIndex].Count++;
		}

		// 计算前缀和后缀信息
		TArray<FBox> PrefixBounds;
		TArray<int32> PrefixCounts;
		PrefixBounds.SetNum(NumBins);
		PrefixCounts.SetNum(NumBins);

		TArray<FBox> SuffixBounds;
		TArray<int32> SuffixCounts;
		SuffixBounds.SetNum(NumBins);
		SuffixCounts.SetNum(NumBins);

		// 计算前缀（从左到右）
		FBox CurrentPrefixBounds(ForceInit);
		int32 CurrentPrefixCount = 0;
		for (int32 i = 0; i < NumBins; ++i)
		{
			CurrentPrefixBounds += Bins[i].Bounds;
			CurrentPrefixCount += Bins[i].Count;
			PrefixBounds[i] = CurrentPrefixBounds;
			PrefixCounts[i] = CurrentPrefixCount;
		}

		// 计算后缀（从右到左）
		FBox CurrentSuffixBounds(ForceInit);
		int32 CurrentSuffixCount = 0;
		for (int32 i = NumBins - 1; i >= 0; --i)
		{
			CurrentSuffixBounds += Bins[i].Bounds;
			CurrentSuffixCount += Bins[i].Count;
			SuffixBounds[i] = CurrentSuffixBounds;
			SuffixCounts[i] = CurrentSuffixCount;
		}

		// 遍历所有可能的分割位置
		for (int32 SplitBin = 0; SplitBin < NumBins - 1; ++SplitBin)
		{
			int32 LeftCount = PrefixCounts[SplitBin];
			int32 RightCount = SuffixCounts[SplitBin + 1];

			if (LeftCount == 0 || RightCount == 0)
				continue;

			FBox LeftBounds = PrefixBounds[SplitBin];
			FBox RightBounds = SuffixBounds[SplitBin + 1];

			FVector LeftSize = LeftBounds.GetSize();
			FVector RightSize = RightBounds.GetSize();

			float LeftSurfaceArea = 2.0f * (LeftSize.X * LeftSize.Y + LeftSize.X * LeftSize.Z + LeftSize.Y * LeftSize.Z);
			float RightSurfaceArea = 2.0f * (RightSize.X * RightSize.Y + RightSize.X * RightSize.Z + RightSize.Y * RightSize.Z);

			// SAH成本计算
			float Cost = 0.3f + (LeftSurfaceArea * LeftCount + RightSurfaceArea * RightCount) / UnionSurfaceArea;

			if (Cost < BestCost)
			{
				BestCost = Cost;
				BestAxis = Axis;
				BestSplit = AxisStart + (SplitBin + 1) * BinWidth;
			}
		}
	}

	// 如果没有找到合适的分割，使用中位数分割作为备选
	if (BestAxis == -1)
	{
		return BuildRecursive_Middle(InClusters, Depth);
	}

	// 根据最佳分割方案分割簇
	TArray<FSegmentCluster*> LeftClusters, RightClusters;

	for (FSegmentCluster* Cluster : InClusters)
	{
		if (!Cluster)
		{
			continue;
		}

		float Center = Cluster->BoundingBox.GetCenter()[BestAxis];
		if (Center < BestSplit)
		{
			LeftClusters.Add(Cluster);
		}
		else
		{
			RightClusters.Add(Cluster);
		}
	}

	// 检查分割结果，避免无限递归
	if (LeftClusters.Num() == 0 || RightClusters.Num() == 0)
	{
		// 分割失败，使用中位数分割
		return BuildRecursive_Middle(InClusters, Depth);
	}

	if (LeftClusters.Num() == InClusters.Num() || RightClusters.Num() == InClusters.Num())
	{
		// 分割无效，使用中位数分割
		return BuildRecursive_Middle(InClusters, Depth);
	}

	// 递归构建子树
	Node->bIsLeaf = false;
	Node->LeftChild = BuildRecursive_SAH(LeftClusters, Depth + 1);
	Node->RightChild = BuildRecursive_SAH(RightClusters, Depth + 1);

	return Node;
}
	
bool FLineDataConverter::ConvertToGPUData(const FLineBVHBuilder& Builder, FGPULineData& OutGPUData)
{
	if (!Builder.IsBuilt())
	{
		UE_LOG(LogSurfaceLineBuilder, Warning, TEXT("BVH树未构建, 不能转换为GPU数据"));
		return false;
	}
	
	OutGPUData.Reset();
	
	// 第一步：收集节点数据
	OutGPUData.RootNodeIndex = CollectNodesRecursive(Builder.Root, OutGPUData);
	
	// 第二步：收集Cluster数据
	CollectClustersRecursive(Builder.Root, OutGPUData);

	// 第三步：收集线段数据
	CollectSegmentsRecursive(Builder.Root, OutGPUData);
	
	// 第四步：为叶子节点分配正确的线段索引
	AssignIndices(OutGPUData);

	// 计算内存占用
	float NodesMemoryMB = OutGPUData.Nodes.Num() * sizeof(FGPULineBVHNode) / (1024.0f * 1024.0f);
	float ClustersMemoryMB = OutGPUData.Clusters.Num() * sizeof(FGPUSegmentCluster) / (1024.0f * 1024.0f);
	float SegmentsMemoryMB = OutGPUData.Segments.Num() * sizeof(FGPUSegment) / (1024.0f * 1024.0f);

	UE_LOG(LogSurfaceLineBuilder, Log, TEXT("BVHData到GPUData转换完成, GPU内存占用统计: 节点 %.2f MB, Cluster %.2f MB, 线段 %.2f MB"), 
		NodesMemoryMB, ClustersMemoryMB, SegmentsMemoryMB);
	
	return OutGPUData.IsValid();
}

int32 FLineDataConverter::CollectNodesRecursive(const FLineBVHNode* Node, FGPULineData& OutData)
{
	if (!Node) return -1;
	
	FGPULineBVHNode GPUBVHNode;
	GPUBVHNode.MinExtent = FVector3f(Node->BoundingBox.Min);
	GPUBVHNode.MaxExtent = FVector3f(Node->BoundingBox.Max);
	
	int32 CurrentNodeIndex = OutData.Nodes.Num();
	OutData.Nodes.Add(GPUBVHNode);
	
	if (Node->bIsLeaf)
	{
		// 叶子节点：记录线段数量
		OutData.Nodes[CurrentNodeIndex].LeftChild = -1;
		OutData.Nodes[CurrentNodeIndex].RightChild = -1;
		OutData.Nodes[CurrentNodeIndex].IsLeaf = 1;

		// 暂时设置临时值，后续通过AssignSegmentIndices正确设置
		OutData.Nodes[CurrentNodeIndex].ClusterIndex = -1;
	}
	else
	{
		// 内部节点：递归构建子节点
		OutData.Nodes[CurrentNodeIndex].LeftChild = CollectNodesRecursive(Node->LeftChild, OutData);
		OutData.Nodes[CurrentNodeIndex].RightChild = CollectNodesRecursive(Node->RightChild, OutData);
		OutData.Nodes[CurrentNodeIndex].IsLeaf = 0;
		OutData.Nodes[CurrentNodeIndex].ClusterIndex = -1;
	}
	
	return CurrentNodeIndex;
}
	
void FLineDataConverter::CollectClustersRecursive(const FLineBVHNode* Node, FGPULineData& OutData)
{
	if (!Node) return;
	
	if (Node->bIsLeaf)
	{
		// 叶子节点：收集Cluster
		FGPUSegmentCluster NewGPUCluster;

		NewGPUCluster.MinExtent = FVector3f(Node->Cluster->BoundingBox.Min);
		NewGPUCluster.SegmentStartIndex = Node->Cluster->SegmentStartIndex;

		NewGPUCluster.MaxExtent = FVector3f(Node->Cluster->BoundingBox.Max);
		NewGPUCluster.PolygonIndex = Node->Cluster->PolygonIndex;

		NewGPUCluster.AllSegmentNum = Node->Cluster->GetNumSegments();

		for (int32 LOD = 0; LOD < 8; ++LOD)
		{
			NewGPUCluster.SegmentNumPerLOD[LOD] = Node->Cluster->SegmentNumPerLOD[LOD];
		}

		OutData.Clusters.Add(NewGPUCluster);
	}
	else
	{
		// 内部节点：递归处理子节点
		CollectClustersRecursive(Node->LeftChild, OutData);
		CollectClustersRecursive(Node->RightChild, OutData);
	}
}

void FLineDataConverter::CollectSegmentsRecursive(const FLineBVHNode* Node, FGPULineData& OutData)
{
	if (!Node) return;

	if (Node->bIsLeaf)
	{
		// 叶子节点：收集线段
		for (const FSegment& Segment : Node->Cluster->Segments)
		{
			FGPUSegment GPUSegment;
			GPUSegment.Start = FVector3f(Segment.Start);
			GPUSegment.End = FVector3f(Segment.End);
			GPUSegment.PolygonIndex = Segment.PolygonIndex;

			OutData.Segments.Add(GPUSegment);
		}
	}
	else
	{
		// 内部节点：递归处理子节点
		CollectSegmentsRecursive(Node->LeftChild, OutData);
		CollectSegmentsRecursive(Node->RightChild, OutData);
	}
}

void FLineDataConverter::AssignIndices(FGPULineData& GPUData)
{
	// 首先为每个Cluster分配线段起始索引
	int32 CurrentSegmentIndex = 0;

	for (int32 ClusterIndex = 0; ClusterIndex < GPUData.Clusters.Num(); ++ClusterIndex)
	{
		FGPUSegmentCluster& Cluster = GPUData.Clusters[ClusterIndex];
		Cluster.SegmentStartIndex = CurrentSegmentIndex;

		// 更新当前线段索引
		CurrentSegmentIndex += Cluster.AllSegmentNum;
	}

	// 然后递归为每个叶子节点设置Cluster索引
	TFunction<void(int32, int32&)> SetLeafClusterIndicesRecursive =
		[&](int32 NodeIndex, int32& CurrentLeafIndex) -> void
		{
			if (NodeIndex < 0 || NodeIndex >= GPUData.Nodes.Num()) return;

			FGPULineBVHNode& Node = GPUData.Nodes[NodeIndex];

			if (Node.IsLeaf > 0.5f) // 叶子节点
			{
				if (CurrentLeafIndex < GPUData.Clusters.Num())
				{
					// 设置叶子节点对应的Cluster索引
					Node.ClusterIndex = static_cast<float>(CurrentLeafIndex);
					CurrentLeafIndex++;
				}
			}
			else // 内部节点
			{
				// 递归处理子节点
				if (Node.LeftChild >= 0)
				{
					SetLeafClusterIndicesRecursive(static_cast<int32>(Node.LeftChild), CurrentLeafIndex);
				}
				if (Node.RightChild >= 0)
				{
					SetLeafClusterIndicesRecursive(static_cast<int32>(Node.RightChild), CurrentLeafIndex);
				}
			}
		};

	int32 CurrentLeafIndex = 0;
	if (GPUData.RootNodeIndex >= 0)
	{
		SetLeafClusterIndicesRecursive(GPUData.RootNodeIndex, CurrentLeafIndex);
	}
}