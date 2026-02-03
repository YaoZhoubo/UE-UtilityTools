#pragma once

#include "CoreMinimal.h"
#include "BVHConfig.generated.h"


UENUM(BlueprintType)
enum class EBVHBuildStrategy : uint8
{
	SAH         UMETA(DisplayName = "Surface Area Heuristic"),
	Middle      UMETA(DisplayName = "Middle Split")
};

USTRUCT(BlueprintType)
struct UTILITYRENDERER_API FBVHBuildConfig
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BVH", meta = (ClampMin = "0"))
	int32 MaxDataNum = 1000000;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BVH")
	EBVHBuildStrategy Strategy = EBVHBuildStrategy::SAH;

	// TODO: 将异步处理与该选项关联
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BVH")
	bool bEnableParallelBuild = false; 

	FBVHBuildConfig() = default;
};

USTRUCT(BlueprintType)
struct UTILITYRENDERER_API FBVHStats
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	int32 NumNodes = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	int32 NumLeaves = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	int32 MaxDepth = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	float BuildTimeMs = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	float MemoryUsageMB = 0.0f;

	FBVHStats() = default;
};

// 多边形结构
USTRUCT(BlueprintType)
struct UTILITYRENDERER_API FPolygon
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BVH")
	TArray<FVector> Vertices;
};

// 三角形结构
USTRUCT(BlueprintType)
struct UTILITYRENDERER_API FTriangle
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BVH")
	FVector Vertex1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BVH")
	FVector Vertex2;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BVH")
	FVector Vertex3;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BVH")
	int32 PolygonIndex;	

	FTriangle()
		: Vertex1(FVector::ZeroVector)
		, Vertex2(FVector::ZeroVector)
		, Vertex3(FVector::ZeroVector)
		, PolygonIndex(-1)
	{
	}

	FTriangle(const FVector& V0, const FVector& V1, const FVector& V2, int32 InPolygonIndex)
	{
		Vertex1 = V0;
		Vertex2 = V1;
		Vertex3 = V2;
		PolygonIndex = InPolygonIndex;
	}

	bool operator==(const FTriangle& Other) const
	{
		return Vertex1.Equals(Other.Vertex1) &&
			Vertex2.Equals(Other.Vertex2) &&
			Vertex3.Equals(Other.Vertex3) &&
			PolygonIndex == Other.PolygonIndex;
	}

	bool operator!=(const FTriangle& Other) const
	{
		return !(*this == Other);
	}

	// 计算包围盒
	FBox GetBoundingBox() const
	{
		FBox Box(ForceInit);
		Box += Vertex1;
		Box += Vertex2;
		Box += Vertex3;
		return Box;
	}
};