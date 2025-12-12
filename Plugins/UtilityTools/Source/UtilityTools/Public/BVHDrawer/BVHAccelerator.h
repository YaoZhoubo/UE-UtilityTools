// Copyright(c) 2025 YaoZhoubo. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "BVHDrawer/BVHConfig.h"
#include "Engine/TextureRenderTarget2D.h"


/// \brief 线段结构，表示多边形的边
struct FSegment
{
	FVector Start;		///< 线段起点
	FVector End;		///< 线段终点
	int32 Index;		///< 所属多边形索引

	FSegment(const FVector& InStart, const FVector& InEnd, int32 InPolyIndex)
		//: Start(InStart), End(InEnd), Index(InPolyIndex) 
	{
		Start = FVector(InStart.X, InStart.Y, 0);
		End = FVector(InEnd.X, InEnd.Y, 0);

		Index = InPolyIndex;
	}

	/// \brief 计算线段的包围盒
	FBox GetBoundingBox() const
	{
		FBox Box(ForceInit);
		Box += Start;
		Box += End;
		return Box;
	}

	/// \brief 计算点到线段的最短距离
	float DistanceToPoint(const FVector& Point) const
	{
		FVector AB = End - Start;
		FVector AP = Point - Start;

		float abSq = AB.SizeSquared(); // 线段长度的平方
		if (abSq < KINDA_SMALL_NUMBER) 
		{
			// 线段退化为点，直接返回点到点的距离
			return FVector::Dist(Point, Start);
		}

		// 计算点在线段上的投影位置
		float t = FVector::DotProduct(AP, AB) / abSq;
		t = FMath::Clamp(t, 0.0f, 1.0f);

		// 计算垂足点
		FVector ClosestPoint = Start + t * AB;

		return FVector::Distance(Point, ClosestPoint);
	}
};

/// \brief BVH节点结构
struct FBVHNode
{
	FBox BoundingBox;			///< 节点的包围盒
	bool bIsLeaf;				///< 是否为叶子节点

	TArray<FSegment> Segments;	///< 存储的线段

	FBVHNode* LeftChild;		///< 左子节点
	FBVHNode* RightChild;		///< 右子节点

	FBVHNode() : bIsLeaf(false), LeftChild(nullptr), RightChild(nullptr) {}
	~FBVHNode()
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

	// 禁止拷贝构造和赋值
	FBVHNode(const FBVHNode&) = delete;
	FBVHNode& operator=(const FBVHNode&) = delete;
};

/// \brief 负责从Polygons构建BVH树，并提供查询函数。
class FPolygonBVH
{
public:
	FPolygonBVH(const TArray<TArray<FVector>>& InPolygons, int32 InLeafSegmentLimit = 8);
	~FPolygonBVH();

	/// \brief 构建BVH树
	void Build();

	/// \brief 检查BVH树是否已构建
	bool IsBuilt() const { return Root != nullptr; }

	/// \brief 获取统计信息
	void GetStats(int32& OutNumNodes, int32& OutNumLeaves, int32& OutMaxDepth, float& OutMemoryUsageMB) const;

	/// \brief 查询点到多边形组的最短距离
	float QueryClosestDistance(const FVector& Point, float InitialMaxDistance = FLT_MAX) const;

public:
	FBVHNode* Root;					///< BVH树的根节点
	TArray<FSegment> AllSegments;	///< 所有多边形的线段
	int32 LeafSegmentLimit;			///< 叶子节点最多包含的线段数量

private:
	/// \brief 递归构建BVH树
	FBVHNode* BuildRecursive(TArray<FSegment>& Segments, int32 Depth);

	/// \brief 使用SAH策略选择最佳分割平面
	bool FindBestSplitPlane(const TArray<FSegment>& Segments, int32& OutBestAxis, float& OutBestSplitValue);

	/// \brief 递归查询函数
	float QueryClosestDistanceRecursive(const FBVHNode* Node, const FVector& Point, float CurrentMinDistance) const;

	/// \brief 递归统计BVH树信息
	void GetStatsRecursive(const FBVHNode* Node, int32 CurrentDepth, int32& OutNumNodes, int32& OutNumLeaves, int32& OutMaxDepth, float& OutMemoryUsageMB) const;
};


/// \brief GPU BVH节点结构
struct FGPUBVHNode
{
	FVector MinExtent;    ///< 包围盒最小值
	FVector MaxExtent;    ///< 包围盒最大值
	int32 LeftChild;      ///< 左子节点索引（-1表示无效）
	int32 RightChild;     ///< 右子节点索引（-1表示无效）
	int32 SegmentStart;   ///< 线段起始索引（仅叶子节点有效）
	int32 SegmentCount;   ///< 线段数量（仅叶子节点有效）

	FGPUBVHNode()
		: LeftChild(-1), RightChild(-1), SegmentStart(-1), SegmentCount(0)
	{
	}
};

/// \brief GPU线段数据
struct FGPUSegment
{
	FVector Start;        ///< 线段起点
	FVector End;          ///< 线段终点
	int32 PolygonIndex;   ///< 所属多边形索引

	FGPUSegment() : PolygonIndex(-1) {}
};

/// \brief BVH GPU数据
struct FGPUBVHData
{
	TArray<FGPUBVHNode> Nodes;    ///< BVH节点数组
	TArray<FGPUSegment> Segments; ///< 所有线段数据
	int32 RootNodeIndex;          ///< 根节点索引

	FGPUBVHData() : RootNodeIndex(-1) {}

	// 清空数据
	void Reset()
	{
		Nodes.Empty();
		Segments.Empty();
		RootNodeIndex = -1;
	}

	// 检查数据是否有效
	bool IsValid() const
	{
		return RootNodeIndex >= 0 && Nodes.Num() > 0;
	}
};

/// \brief 提供BVH数据到其他格式的转换器
class FBVHGPUConverter
{
public:
	/// \brief 转换为GPU数据格式
	static bool ConvertToGPUData(const FPolygonBVH& BVH, FGPUBVHData& OutGPUData);

	/// \brief 转换为渲染目标纹理
	static bool ConvertToRenderTargets(
		const FPolygonBVH& BVHData,
		UTextureRenderTarget2D* OutBVHDataRenderTarget,
		UTextureRenderTarget2D* OutSegmentDataRenderTarget
	);

	/// \brief 打印CPU BVH数据
	static void PrintCPUData(const FPolygonBVH& BVH);

	/// \brief 打印GPU BVH数据
	static void PrintGPUData(const FGPUBVHData& GPUData);

private:
	/// \brief 递归构建GPU节点
	static int32 CollectNodesRecursive(const FBVHNode* CPUNode, TArray<FGPUBVHNode>& OutGPUNode);

	/// \brief 收集所有线段数据
	static void CollectSegmentsRecursive(const FBVHNode* Node, TArray<FGPUSegment>& OutSegments);

	/// \brief 为叶子节点分配线段索引
	static void AssignSegmentIndices(FGPUBVHData& GPUData);

	static UTextureRenderTarget2D* CreateRenderTarget(UObject* Outer, int32 Width, int32 Height, const FString& RenderTargetName);

	/// \brief 填充BVH渲染目标
	static void FillBVHRenderTarget(UTextureRenderTarget2D* RenderTarget, const FGPUBVHData& BVHData);

	/// \brief 填充线段渲染目标
	static void FillSegmentRenderTarget(UTextureRenderTarget2D* RenderTarget, const FGPUBVHData& BVHData);

	/// \brief 递归打印CPU节点
	static void PrintCPUNodeRecursive(const FBVHNode* Node, int32 Depth = 0);

	/// \brief 递归打印GPU节点
	static void PrintGPUNodeRecursive(const FGPUBVHData& GPUData, int32 NodeIndex, int32 Depth = 0);
};