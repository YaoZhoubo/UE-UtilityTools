// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "RapidlyRandomTreeComponent.generated.h"

// 用于RRT的树结构
USTRUCT(BlueprintType)
struct FRRTNode
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "RRT")
	FVector Position = FVector::ZeroVector;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "RRT")
	int32 ParentNodeIndex = -1;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "RRT")
	int32 Cost = 0;
};

/**
 * URapidlyRandomTreeComponent 定义了与RRT路径规划相关的功能，并提供RRT计算过程中相关的Debug信息
 * 用户应该只需要考虑使用以下功能:
 *    - RunRRT()
 *    - RunRRTStar()
 */
UCLASS( ClassGroup=(RapidlyRandomTree), meta=(BlueprintSpawnableComponent),
	HideCategories = (Tags, AssetUserData, Replication, Activation, Collision, Cooking, Navigation))
class UTILITYTOOLS_API URapidlyRandomTreeComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	/** 搜索空间缩放，搜索空间是根据起点与终点的直线距离来定义的立方体，ExploreSpaceScale为1时，搜索空间的边长就是起点与终点的直线距离 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RRT|Base", meta = (ClampMin = 0, ClampMax = 2))
	float ExploreSpaceScale;

	/** 起点与终点的直线距离除以Step，作为每一步新结点走的距离 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RRT|Base")
	float Step;

	/** 目标偏向，进行随机采样时有一定概率直接将终点作为采样点，值的范围限定在0 - 0.5，*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RRT|Base", meta = (ClampMin = 0, ClampMax = 0.5))
	float GoalBias;

	/** RRT查找的最大次数，若在最大次数内无法找到符合要求的路径， RunRRT将返回false */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RRT|Base", meta = (ClampMin = 0, ClampMax = 5000))
	int32 MaxIterations;

	/** 搜索邻域节点时，将根据该参数来定义邻域范围 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RRT|Base", meta = (ClampMin = 1, ClampMax = 5))
	float NeighborExp;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "RRT|Debug")
	FVector StartPosition;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "RRT|Debug")
	FVector EndPosition;

	//~ Start ExploreSpaceBox Debug
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RRT|Debug|ExploreSpace")
	uint8 bShowExploreSpace : 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RRT|Debug|ExploreSpace")
	FBox ExploreSpaceBox;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RRT|Debug|ExploreSpace")
	FColor ExploreSpaceBoxColor;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "RRT|Debug|ExploreSpace")
	uint8 bExploreSpaceBoxPersistent : 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RRT|Debug|ExploreSpace")
	float ExploreSpaceBoxLifeTime;
	//~ End ExploreSpaceBox Debug

	//~ Start TreeNodes Debug
	/** 所有符合要求（通视）的NewPosition都会添加到该树 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "RRT|Debug|TreeNodes")
	TArray<FRRTNode> TreeNodes;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RRT|Debug|TreeNodes")
	uint8 bShowTreeNodes : 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RRT|Debug|TreeNodes")
	float TreeNodesRadius;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RRT|Debug|TreeNodes")
	int32 TreeNodesSegments;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RRT|Debug|TreeNodes")
	FColor TreeNodesColor;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "RRT|Debug|TreeNodes")
	uint8 bTreeNodesPersistent : 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RRT|Debug|TreeNodes")
	float TreeNodesLifeTime;
	//~ End TreeNodes Debug

	//~ Start RandomPoints Debug
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "RRT|Debug|RandomPoints")
	TArray<FVector> RandomPoints;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RRT|Debug|RandomPoints")
	uint8 bShowRandomPoints : 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RRT|Debug|RandomPoints")
	float RandomPointsRadius;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RRT|Debug|RandomPoints")
	int32 RandomPointsSegments;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RRT|Debug|RandomPoints")
	FColor RandomPointsColor;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "RRT|Debug|RandomPoints")
	uint8 bRandomPointsPersistent : 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RRT|Debug|RandomPoints")
	float RandomPointsLifeTime;
	//~ End RandomPoints Debug

	//~ Start NewPositions Debug
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "RRT|Debug|NewPositions")
	TArray<FVector> NewPositions;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RRT|Debug|NewPositions")
	uint8 bShowNewPositions : 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RRT|Debug|NewPositions")
	float NewPositionsRadius;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RRT|Debug|NewPositions")
	int32 NewPositionsSegments;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RRT|Debug|NewPositions")
	FColor NewPositionsColor;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "RRT|Debug|NewPositions")
	uint8 bNewPositionsPersistent : 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RRT|Debug|NewPositions")
	float NewPositionsLifeTime;
	//~ End RandomPoints Debug

	//~ Start ResultPoints Debug
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "RRT|Debug|ResultPoints")
	TArray<FVector> ResultPoints;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RRT|Debug|ResultPoints")
	uint8 bShowResultPoints : 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RRT|Debug|ResultPoints")
	float ResultPointsRadius;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RRT|Debug|ResultPoints")
	int32 ResultPointsSegments;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RRT|Debug|ResultPoints")
	FColor ResultPointsColor;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "RRT|Debug|ResultPoints")
	uint8 bResultPointsPersistent : 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RRT|Debug|ResultPoints")
	float ResultPointsLifeTime;
	//~ End ResultPoints Debug

public:	
	URapidlyRandomTreeComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void BeginDestroy() override;

	/**
	 *	RRT算法将根据传入的起点和终点，基于Category=RRT|Base中的关键参数，进行路径规划，计算出无碰撞的路径，并将结果数组存放到OutResultPoints中
	 *  得到的路径不是最优路径，仅无碰撞
	 *
	 *	@param		InStartPosition		起点
	 *	@param		InEndPosition		终点
	 *	@param		OutResultPoints		用于存放路径的数组
	 * 
	 *  @return 成功找到路径则返回true，否则为false
	 */
	UFUNCTION(BlueprintCallable, Category = "RRTFunction")
	bool RunRRT(const FVector& InStartPosition, const FVector& InEndPosition, TArray<FVector>& OutResultPoints);
	
	/**
	 *	RRTStar算法在RRT的基础上进行优化，获得渐进最优特性
	 *  得到的路径是渐进最优路径
	 *
	 *	@param		InStartPosition		起点
	 *	@param		InEndPosition		终点
	 *	@param		OutResultPoints		用于存放路径的数组
	 *
	 *  @return 成功找到路径则返回true，否则为false
	 */
	UFUNCTION(BlueprintCallable, Category = "RRTFunction")
	bool RunRRTStar(const FVector& InStartPosition, const FVector& InEndPosition, TArray<FVector>& OutResultPoints);

	UFUNCTION(BlueprintCallable, Category = "RRTFunction")
	bool RunRRTStarSingle(const FVector& InStartPosition, const FVector& InEndPosition, TArray<FVector>& OutResultPoints);

	UFUNCTION(BlueprintCallable, CallInEditor, Category = "RRTFunction")
	void ClearRandomPoints();

	UFUNCTION(BlueprintCallable, CallInEditor, Category = "RRTFunction")
	void ClearNewPositions();

	UFUNCTION(BlueprintCallable, CallInEditor, Category = "RRTFunction")
	void ClearResultPoints();

	UFUNCTION(BlueprintCallable, CallInEditor, Category = "RRTFunction")
	void ClearTreeNodes();

protected:
	virtual void BeginPlay() override;

private:
	void SetExploreSpaceBox();
};
