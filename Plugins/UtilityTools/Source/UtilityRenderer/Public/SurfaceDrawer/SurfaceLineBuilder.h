#pragma once

#include "CoreMinimal.h"
#include "Math/Bounds.h"
#include "BVHConfig.h"

struct FSegmentCluster;

/// \brief BVH节点结构，用于构建BVH树的节点层次结构
struct FLineBVHNode
{
	FBox BoundingBox;			///< 节点的包围盒
	bool bIsLeaf;				///< 是否为叶子节点

	FSegmentCluster* Cluster;	///< 存储的线段簇（仅叶子节点有效）

	FLineBVHNode* LeftChild;	///< 左子节点
	FLineBVHNode* RightChild;	///< 右子节点

	FLineBVHNode() : bIsLeaf(false), Cluster(nullptr), LeftChild(nullptr), RightChild(nullptr) {}
	~FLineBVHNode();
	FLineBVHNode(const FLineBVHNode&) = delete;
	FLineBVHNode& operator=(const FLineBVHNode&) = delete;
};

/// \brief BVH树构建器类，负责从多边形数据提取线段数据并构建BVH树
class UTILITYRENDERER_API FLineBVHBuilder
{
public:
	FLineBVHBuilder(const TArray<FPolygon>& InPolygons, const FBVHBuildConfig& InBuildConfig);
	~FLineBVHBuilder();

	/// \brief 构建BVH树
	void Build();

	/// \brief 检查BVH树是否已构建
	bool IsBuilt() const { return Root != nullptr; }

	/// \brief 获取统计信息
	void GetStats(FBVHStats& OutStats) const;

private:
	/// \brief 递归构建BVH树
	FLineBVHNode* BuildRecursive_Middle(const TArray<FSegmentCluster*>& InClusters, int32 Depth);
	FLineBVHNode* BuildRecursive_SAH(const TArray<FSegmentCluster*>& InClusters, int32 Depth);

	/// \brief 递归统计BVH树信息
	void GetStatsRecursive(const FLineBVHNode* Node, int32 CurrentDepth, FBVHStats& OutStats) const;

private:
	friend class FLineDataConverter;

	FLineBVHNode* Root;						///< BVH树的根节点
	TArray<FSegmentCluster*> AllClusters;	///< 所有线段簇
	FBVHBuildConfig BuildConfig;			///< BVH构建配置

	// --------------------------------------------------------------------
	// 用于调试的参数
	// --------------------------------------------------------------------
	double BuildTimeMs;						///< 构建耗时（毫秒）
	int32 TotalSegments;					///< 从多边形提取的线段总数
};

// =====================================================================
// 对应的GPU数据结构
// =====================================================================

/// \brief GPU BVH节点
struct FGPULineBVHNode
{
	FVector3f MinExtent;	///< 包围盒最小值					(12字节)
	int32 LeftChild;		///< 左子节点索引（-1表示无效）		(4字节)

	FVector3f MaxExtent;    ///< 包围盒最大值					(12字节)
	int32 RightChild;		///< 右子节点索引（-1表示无效）		(4字节)

	int32 ClusterIndex;		///< 簇起始索引（仅叶子节点有效）	(4字节)
	int32 IsLeaf;			///< 是否为叶子节点					(4字节)
	float Padding[2];		///< 填充							(4 * 2字节)

	FGPULineBVHNode()
		: LeftChild(-1.f), RightChild(-1.f), ClusterIndex(-1.f), IsLeaf(-1.f)
	{
	}
};

/// \brief GPU 线段簇
struct FGPUSegmentCluster
{
	FVector3f MinExtent;		///< 包围盒最小值		(12字节)
	int32 SegmentStartIndex;	///< 线段起点			(4字节)

	FVector3f MaxExtent;		///< 包围盒最大值		(12字节)
	int32 PolygonIndex;			///< 所属多边形索引		(4字节)

	int32 AllSegmentNum;		///< 总线段数量			(4字节)
	float Padding[3];			///< 填充				(4 * 3字节)

	int32 SegmentNumPerLOD[8];	///< 每个LOD的线段数量	(4 * 8字节)
};

/// \brief GPU线段数据
struct FGPUSegment
{
	FVector3f Start;    ///< 线段起点		(12字节)
	int32 PolygonIndex; ///< 所属多边形索引	(4字节)

	FVector3f End;      ///< 线段终点		(12字节)
	float Padding;		///< 填充			(4字节)

	FGPUSegment() : PolygonIndex(-1) {}
};

/// \brief 线 GPU数据
struct FGPULineData
{
	TArray<FGPULineBVHNode> Nodes;			///< 节点数据
	TArray<FGPUSegmentCluster> Clusters;	///< Cluster 数据
	TArray<FGPUSegment> Segments;			///< 线段数据
	int32 RootNodeIndex;					///< 根节点索引

	FGPULineData() : RootNodeIndex(-1) {}

	// 清空数据
	void Reset()
	{
		Nodes.Empty();
		Clusters.Empty();
		RootNodeIndex = -1;
	}

	// 检查数据是否有效
	bool IsValid() const
	{
		return RootNodeIndex >= 0 && Nodes.Num() > 0;
	}
};

/// \brief 提供BVH数据到GPU格式的转换器
class UTILITYRENDERER_API FLineDataConverter
{
public:
	/// \brief 转换为GPU数据格式
	static bool ConvertToGPUData(const FLineBVHBuilder& Builder, FGPULineData& OutGPUData);

private:
	/// \brief 收集节点数据
	static int32 CollectNodesRecursive(const FLineBVHNode* Node, FGPULineData& OutData);

	/// \brief 收集Cluster数据
	static void CollectClustersRecursive(const FLineBVHNode* Node, FGPULineData& OutData);

	/// \brief 收集线段数据
	static void CollectSegmentsRecursive(const FLineBVHNode* Node, FGPULineData& OutData);

	/// \brief 分配索引
	static void AssignIndices(FGPULineData& GPUData);
};