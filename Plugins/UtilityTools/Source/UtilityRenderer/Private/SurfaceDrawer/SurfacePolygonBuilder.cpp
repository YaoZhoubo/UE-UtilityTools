#include "SurfaceDrawer/SurfacePolygonBuilder.h"


DEFINE_LOG_CATEGORY_STATIC(LogSurfacePolygonBuilder, Log, All);


class FTimeLogScope
{
public:
	FTimeLogScope(const TCHAR* StaticLabel) : Label(StaticLabel), StartTime(FPlatformTime::Cycles()) {}
	~FTimeLogScope()
	{
		const uint32 EndTime = FPlatformTime::Cycles();
		UE_LOG(LogSurfacePolygonBuilder, Log, TEXT("%s 耗时 [%.2fs]"), Label, FPlatformTime::ToMilliseconds(EndTime - StartTime) / 1000.0f);
	}

private:
	const TCHAR* Label;
	const uint32 StartTime;
};

#define BUILD_TIME_LOG_SCOPE(Name) FTimeLogScope TimeLogScope_##Name(TEXT(#Name))

FPolygonBVHBuilder::FPolygonBVHBuilder(const TArray<FTriangle>& InTriangles, const FBVHBuildConfig& InBuildConfig) : Root(nullptr)
	, AllTriangles(InTriangles)
	, BuildConfig(InBuildConfig)
	, BuildTimeMs(0.0)
{
}

FPolygonBVHBuilder::~FPolygonBVHBuilder()
{
	if (Root)
	{
		delete Root;
		Root = nullptr;
	}
}

void FPolygonBVHBuilder::Build()
{
	BUILD_TIME_LOG_SCOPE(PolygonBVHBuild);

	if (AllTriangles.Num() == 0)
	{
		UE_LOG(LogSurfacePolygonBuilder, Warning, TEXT("没有三角形可构建BVH"));
		return;
	}

	double StartTime = FPlatformTime::Seconds();

	// 仅支持Middle构建
	if (BuildConfig.Strategy == EBVHBuildStrategy::Middle)
	{
		Root = BuildRecursive_Middle(AllTriangles, 0);
	}
	else
	{
		Root = BuildRecursive_Middle(AllTriangles, 0);
	}

	BuildTimeMs = (FPlatformTime::Seconds() - StartTime) * 1000.0;
}

void FPolygonBVHBuilder::GetStats(FBVHStats& OutStats) const
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

	// 使用局部变量累加字节数
	uint64 TotalBytes = 0;

	// 递归统计（需要修改GetStatsRecursive或使用lambda）
	TFunction<void(const FPolygonBVHNode*, int32)> StatsHelper;
	StatsHelper = [&](const FPolygonBVHNode* Node, int32 CurrentDepth)
		{
			if (!Node) return;

			OutStats.NumNodes++;
			TotalBytes += sizeof(FGPUPolygonBVHNode);

			if (CurrentDepth > OutStats.MaxDepth)
			{
				OutStats.MaxDepth = CurrentDepth;
			}

			if (Node->bIsLeaf)
			{
				OutStats.NumLeaves++;
				TotalBytes += sizeof(FGPUTriangle);
			}
			else
			{
				StatsHelper(Node->LeftChild, CurrentDepth + 1);
				StatsHelper(Node->RightChild, CurrentDepth + 1);
			}
		};

	StatsHelper(Root, 0);
 	OutStats.MemoryUsageMB = TotalBytes / (1024.0f * 1024.0f);
}

FPolygonBVHNode* FPolygonBVHBuilder::BuildRecursive_Middle(const TArray<FTriangle>& InTriangles, int32 Depth)
{
	// 添加深度限制防止栈溢出
	if (Depth > 64)
	{
		UE_LOG(LogSurfacePolygonBuilder, Warning, TEXT("达到最大构建深度 64，创建叶子节点"));
		return new FPolygonBVHNode();
	}

	FPolygonBVHNode* Node = new FPolygonBVHNode();

	// 计算联合包围盒
	FBox UnionBox(ForceInit);
	for (const FTriangle& Triangle : InTriangles)
	{
		UnionBox += Triangle.GetBoundingBox();
	}
	Node->BoundingBox = UnionBox;

	// 检查是否达到叶子节点条件（一个叶子节点只保存一个三角形）
	if (InTriangles.Num() == 1)
	{
		Node->bIsLeaf = true;
		Node->Triangle = InTriangles[0];
		
		return Node;
	}
	else if (InTriangles.IsEmpty())
	{
		Node->bIsLeaf = true;
		Node->Triangle = FTriangle();

		return Node;
	}

	// 选择最长的轴作为分割轴
	FVector BoxSize = UnionBox.GetSize();
	int32 SplitAxis = 0;
	if (BoxSize.Y > BoxSize.X) SplitAxis = 1;
	if (BoxSize.Z > BoxSize[SplitAxis]) SplitAxis = 2;

	// 计算所有多边形中心点在分割轴上的中位数
	TArray<float> Centers;
	for (const FTriangle& Triangle : InTriangles)
	{
		Centers.Add(Triangle.GetBoundingBox().GetCenter()[SplitAxis]);
	}

	// 排序并找到中位数
	Centers.Sort();
	float Median = Centers[Centers.Num() / 2];

	// 根据中位数分割多边形
	TArray<FTriangle> LeftTriangles, RightTriangles;

	for (const FTriangle& Triangle : InTriangles)
	{
		float Center = Triangle.GetBoundingBox().GetCenter()[SplitAxis];

		if (Center < Median)
		{
			LeftTriangles.Add(Triangle);
		}
		else
		{
			RightTriangles.Add(Triangle);
		}
	}

	// 检查分割结果，避免无限递归
	if (LeftTriangles.Num() == InTriangles.Num() && RightTriangles.Num() == 0)
	{
		// 所有三角形都在左边，弹出一个给右边
		//UE_LOG(LogSurfacePolygonBuilder, Warning, TEXT("标准分割失败，所有三角形都在左边"));
		RightTriangles.Add(LeftTriangles.Pop());
	}
	if (RightTriangles.Num() == InTriangles.Num() && LeftTriangles.Num() == 0)
	{
		// 所有三角形都在右边，弹出一个给左边
		//UE_LOG(LogSurfacePolygonBuilder, Warning, TEXT("标准分割失败，所有三角形都在右边"));
		LeftTriangles.Add(RightTriangles.Pop());
	}

	// 递归构建子树
	Node->bIsLeaf = false;
	Node->LeftChild = BuildRecursive_Middle(LeftTriangles, Depth + 1);
	Node->RightChild = BuildRecursive_Middle(RightTriangles, Depth + 1);

	return Node;
}

void FPolygonBVHBuilder::GetStatsRecursive(const FPolygonBVHNode* Node, int32 CurrentDepth, FBVHStats& OutStats) const
{
	if (!Node)
	{
		return;
	}

	// 统计节点数量、内存
	OutStats.NumNodes++;
	OutStats.MemoryUsageMB += sizeof(FGPUPolygonBVHNode);

	// 更新最大深度
	if (CurrentDepth > OutStats.MaxDepth)
	{
		OutStats.MaxDepth = CurrentDepth;
	}

	if (Node->bIsLeaf)
	{
		// 叶子节点统计、线段内存
		OutStats.NumLeaves++;
		OutStats.MemoryUsageMB += sizeof(FGPUTriangle);
	}
	else
	{
		// 递归遍历子节点
		GetStatsRecursive(Node->LeftChild, CurrentDepth + 1, OutStats);
		GetStatsRecursive(Node->RightChild, CurrentDepth + 1, OutStats);
	}
}

bool FPolygonGPUConverter::ConvertToGPUData(const FPolygonBVHBuilder& Builder, FGPUPolygonData& OutGPUData)
{
	BUILD_TIME_LOG_SCOPE(ConvertToGPUData);

	if (!Builder.IsBuilt())
	{
		UE_LOG(LogSurfacePolygonBuilder, Warning, TEXT("BVH树未构建, 不能转换为GPU数据"));
		return false;
	}

	OutGPUData.Reset();

	// 第一步：收集BVH节点数据
	OutGPUData.RootNodeIndex = CollectNodesRecursive(Builder.Root, OutGPUData);

	// 第二步：收集多边形面数据和顶点数据
	CollectTrianglesRecursive(Builder.Root,OutGPUData);

	// 第三步：为叶子节点分配正确的三角形索引
	AssignTriangleIndices(OutGPUData);

	//UE_LOG(LogSurfacePolygonBuilder, Log, TEXT("BVHData到GPUData转换完成: %d 个节点, %d 个三角形"), OutGPUData.Nodes.Num(), OutGPUData.Triangles.Num());

	return OutGPUData.IsValid();
}

int32 FPolygonGPUConverter::CollectNodesRecursive(const FPolygonBVHNode* BVHNodes, FGPUPolygonData& OutGPUData)
{
	if (!BVHNodes) return -1;

	FGPUPolygonBVHNode NewGPUNode;
	NewGPUNode.MinExtent = FVector3f(BVHNodes->BoundingBox.Min);
	NewGPUNode.MaxExtent = FVector3f(BVHNodes->BoundingBox.Max);

	int32 CurrentIndex = OutGPUData.Nodes.Num();
	OutGPUData.Nodes.Add(NewGPUNode);

	if (BVHNodes->bIsLeaf)
	{
		// 叶子节点：
		OutGPUData.Nodes[CurrentIndex].LeftChild = -1;
		OutGPUData.Nodes[CurrentIndex].RightChild = -1;
		OutGPUData.Nodes[CurrentIndex].IsLeaf = 1.0f;

		// 暂时设置默认值，后续通过AssignPolygonIndices正确设置
		OutGPUData.Nodes[CurrentIndex].TriangleIndex = -1; 
	}
	else
	{
		// 内部节点：递归处理子节点
		OutGPUData.Nodes[CurrentIndex].LeftChild = CollectNodesRecursive(BVHNodes->LeftChild, OutGPUData);
		OutGPUData.Nodes[CurrentIndex].RightChild = CollectNodesRecursive(BVHNodes->RightChild, OutGPUData);
		OutGPUData.Nodes[CurrentIndex].IsLeaf = 0.0f;
		OutGPUData.Nodes[CurrentIndex].TriangleIndex = -1;
	}

	return CurrentIndex;
}

void FPolygonGPUConverter::CollectTrianglesRecursive(const FPolygonBVHNode* BVHNode, FGPUPolygonData& OutGPUData)
{
	if (!BVHNode) return;

	if (BVHNode->bIsLeaf)
	{
		// 叶子节点：收集三角形 多边形面和顶点数据
		FGPUTriangle NewGPUTriangle;
		NewGPUTriangle.Vertex1 = FVector3f(BVHNode->Triangle.Vertex1);
		NewGPUTriangle.Vertex2 = FVector3f(BVHNode->Triangle.Vertex2);
		NewGPUTriangle.Vertex3 = FVector3f(BVHNode->Triangle.Vertex3);
		NewGPUTriangle.PolygonIndex = BVHNode->Triangle.PolygonIndex;

		OutGPUData.Triangles.Add(NewGPUTriangle);
	}
	else
	{
		CollectTrianglesRecursive(BVHNode->LeftChild, OutGPUData);
		CollectTrianglesRecursive(BVHNode->RightChild, OutGPUData);
	}
}

void FPolygonGPUConverter::AssignTriangleIndices(FGPUPolygonData& OutGPUData)
{
	float CurrentTriangleIndex = 0.0f;
	for (int32 i = 0; i < OutGPUData.Nodes.Num(); i++)
	{
		FGPUPolygonBVHNode& Node = OutGPUData.Nodes[i];
		if (Node.IsLeaf)
		{
			// 叶子节点，设置三角形索引
			Node.TriangleIndex = CurrentTriangleIndex++;
		}
	}
}
