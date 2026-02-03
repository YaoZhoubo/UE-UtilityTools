#pragma once

#include "CoreMinimal.h"


/// \brief 线段结构，表示多边形的边
struct FSegment
{
	FVector Start;		///< 线段起点
	FVector End;		///< 线段终点
	int32 PolygonIndex;	///< 所属多边形索引

	FSegment(const FVector& InStart, const FVector& InEnd, int32 InPolyIndex)
	{
		Start = FVector(InStart.X, InStart.Y, 0);
		End = FVector(InEnd.X, InEnd.Y, 0);

		PolygonIndex = InPolyIndex;
	}

	/// \brief 计算线段的包围盒
	FBox GetBoundingBox() const
	{
		FBox Box(ForceInit);
		Box += Start;
		Box += End;
		return Box;
	}
};

// \brief 线段簇
struct FSegmentCluster
{
	TArray<FSegment> Segments;	///< 包含的线段
	FBox BoundingBox;			///< 包围盒
	int32 SegmentStartIndex;	///< 线段起始索引
	int32 PolygonIndex;			///< 所属多边形索引
	int32 SegmentNumPerLOD[8];	///< 每个LOD的线段数量	(4 * 8字节)

	FSegmentCluster() = default;

	FSegmentCluster(int32 InPolygonIndex)
		: PolygonIndex(InPolygonIndex)
	{
		BoundingBox = FBox(ForceInit);
	}

	/// \brief 添加线段到簇中
	void AddSegment(const FSegment& Segment)
	{
		Segments.Add(Segment);
		BoundingBox += Segment.GetBoundingBox();
	}

	/// \brief 清空簇
	void Reset()
	{
		Segments.Empty();
		BoundingBox = FBox(ForceInit);
	}

	void GenerateLODLevel();

	/// \brief 获取簇中的线段数量
	int32 GetNumSegments() const { return Segments.Num(); }
};