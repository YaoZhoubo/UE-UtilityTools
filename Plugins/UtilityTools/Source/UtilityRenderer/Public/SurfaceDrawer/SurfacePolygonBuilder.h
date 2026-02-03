#pragma once

#include "CoreMinimal.h"
#include "Math/Bounds.h"
#include "BVHConfig.h"


/// \brief 多边形BVH节点
struct FPolygonBVHNode
{
	FBox BoundingBox;				///< 节点的包围盒
	bool bIsLeaf;					///< 是否为叶子节点

	FTriangle Triangle;				///< 存储的三角形（仅叶子节点有效）

	FPolygonBVHNode* LeftChild;		///< 左子节点
	FPolygonBVHNode* RightChild;	///< 右子节点

	FPolygonBVHNode()
		: bIsLeaf(false)
		, LeftChild(nullptr)
		, RightChild(nullptr)
	{
	}

	~FPolygonBVHNode()
	{
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

	FBox GetBoundingBox() const 
	{
		return bIsLeaf ? Triangle.GetBoundingBox() : BoundingBox;
	}

	// 禁止拷贝构造和赋值
	FPolygonBVHNode(const FPolygonBVHNode&) = delete;
	FPolygonBVHNode& operator=(const FPolygonBVHNode&) = delete;
};

/// \brief BVH树构建器类，负责从三角形数据构建BVH树
class UTILITYRENDERER_API FPolygonBVHBuilder
{
public:
	FPolygonBVHBuilder(const TArray<FTriangle>& InTriangles, const FBVHBuildConfig& InBuildConfig);
	~FPolygonBVHBuilder();

	/// \brief 构建BVH树
	void Build();

	/// \brief 检查BVH树是否已构建
	bool IsBuilt() const { return Root != nullptr; }

	/// \brief 获取统计信息
	void GetStats(FBVHStats& OutStats) const;

private:
	/// \brief 递归构建BVH树
	FPolygonBVHNode* BuildRecursive_Middle(const TArray<FTriangle>& InTriangles, int32 Depth);

	/// \brief 递归统计BVH树信息
	void GetStatsRecursive(const FPolygonBVHNode* Node, int32 CurrentDepth, FBVHStats& OutStats) const;

private:
	friend class FPolygonGPUConverter;

	FPolygonBVHNode* Root;			///< BVH树的根节点
	TArray<FTriangle> AllTriangles;	///< 所有三角形
	FBVHBuildConfig BuildConfig;	///< BVH构建配置

	// --------------------------------------------------------------------
	// 用于调试的参数
	// --------------------------------------------------------------------
	double BuildTimeMs;				///< 构建耗时（毫秒）
};

// =====================================================================
// 对应的GPU数据结构
// =====================================================================

/// \brief GPU BVH节点
struct FGPUPolygonBVHNode
{
	FVector3f MinExtent;	///< 包围盒最小值						(12字节)
	int32 LeftChild;		///< 左子节点索引（-1表示无效）			(4字节)

	FVector3f MaxExtent;	///< 包围盒最大值						(12字节)
	int32 RightChild;		///< 右子节点索引（-1表示无效）			(4字节)

	int32 TriangleIndex;	///< 三角形起始索引（仅叶子节点有效）	(4字节)
	float IsLeaf;			///< 是否为叶子节点						(4字节)
	float Padding1;			///< 填充1								(4字节)
	float Padding2;			///< 填充2								(4字节)

	FGPUPolygonBVHNode()
		: LeftChild(-1), RightChild(-1), TriangleIndex(-1)
	{
	}
};

/// \brief GPU 三角形
struct FGPUTriangle
{
	FVector3f Vertex1;		///< 顶点1			(12字节)
	float Padding1;			///< 填充1			(4字节)

	FVector3f Vertex2;		///< 顶点2			(12字节)
	float Padding2;			///< 填充2			(4字节)

	FVector3f Vertex3;		///< 顶点3			(12字节)
	int32 PolygonIndex;		///< 所属多边形索引	(4字节)
};

/// \brief 多边形 GPU数据
struct FGPUPolygonData
{
	TArray<FGPUPolygonBVHNode> Nodes;			///< BVH节点数组
	TArray<FGPUTriangle> Triangles;				///< 三角形数据
	int32 RootNodeIndex;						///< 根节点索引

	FGPUPolygonData() : RootNodeIndex(-1) {}

	// 清空数据
	void Reset()
	{
		Nodes.Empty();
		Triangles.Empty();
		RootNodeIndex = -1;
	}

	// 检查数据是否有效
	bool IsValid() const
	{
		return RootNodeIndex >= 0 && Nodes.Num() > 0;
	}
};

/// \brief 提供BVH数据到GPU格式的转换器
class UTILITYRENDERER_API FPolygonGPUConverter
{
public:
	/// \brief 转换为GPU数据格式
	static bool ConvertToGPUData(const FPolygonBVHBuilder& Builder, FGPUPolygonData& OutGPUData);

private:
	/// \brief 递归构建GPU节点
	static int32 CollectNodesRecursive(const FPolygonBVHNode* BVHNodes, FGPUPolygonData& OutGPUData);

	/// \brief 收集顶点数据
	static void CollectTrianglesRecursive(const FPolygonBVHNode* BVHNode, FGPUPolygonData& OutGPUData);

	/// \brief 分配索引
	static void AssignTriangleIndices(FGPUPolygonData& OutGPUData);
};