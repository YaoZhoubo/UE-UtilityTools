// Copyright(c) 2025 YaoZhoubo. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "BVHConfig.generated.h"

UENUM(BlueprintType)
enum class EBVHBuildStrategy : uint8
{
	SAH         UMETA(DisplayName = "Surface Area Heuristic"),
	Median      UMETA(DisplayName = "Median Split"),
	Middle      UMETA(DisplayName = "Middle Split")
};

UENUM(BlueprintType)
enum class EBVHDebugDetailLevel : uint8
{
	None        UMETA(DisplayName = "None"),
	Basic       UMETA(DisplayName = "Basic Info"),
	Detailed    UMETA(DisplayName = "Detailed Tree"),
	Verbose     UMETA(DisplayName = "Verbose All")
};

USTRUCT(BlueprintType)
struct FBVHBuildConfig
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BVH", meta = (ClampMin = "1"))
	int32 LeafPrimitiveLimit = 8;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BVH")
	EBVHBuildStrategy Strategy = EBVHBuildStrategy::SAH;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BVH")
	int32 MaxDepth = 64;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BVH")
	bool bEnableParallelBuild = false; // 暂时不支持并行

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BVH")
	bool bValidateTree = true;

	FBVHBuildConfig() = default;
};

USTRUCT(BlueprintType)
struct FTextureConfig
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 MaxTextureSize = 2048;

	FTextureConfig() = default;
};

USTRUCT(BlueprintType)
struct FBVHStats
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

USTRUCT(BlueprintType)
struct FBVHSystemStats
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	int32 ActiveBVHCount = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	int32 TotalNodes = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	int32 TotalSegments = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	float TotalMemoryUsageMB = 0.0f;

	FBVHSystemStats() = default;
};

// 包装多边形的顶点数组
USTRUCT(BlueprintType)
struct FPolygon
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BVH")
	TArray<FVector> Vertices;
};