// Fill out your copyright notice in the Description page of Project Settings.

#include "RRT/RapidlyRandomTreeComponent.h"

void URapidlyRandomTreeComponent::ClearRandomPoints()
{
	RandomPoints.Empty();
}

void URapidlyRandomTreeComponent::ClearNewPositions()
{
	NewPositions.Empty();
}

void URapidlyRandomTreeComponent::ClearResultPoints()
{
	ResultPoints.Empty();
}

void URapidlyRandomTreeComponent::ClearTreeNodes()
{
	TreeNodes.Empty();
}

// Sets default values for this component's properties
URapidlyRandomTreeComponent::URapidlyRandomTreeComponent()
{
	PrimaryComponentTick.bCanEverTick = true;

	ExploreSpaceScale = 1.f;
	Step = 10.f;
	GoalBias = 0.1f;
	MaxIterations = 1000;

	bShowExploreSpace = true;
	ExploreSpaceBoxColor = FColor::Green;
	bExploreSpaceBoxPersistent = false;
	ExploreSpaceBoxLifeTime = 0.f;

	bShowTreeNodes = false;
	TreeNodesRadius = 32.f;
	TreeNodesSegments = 8;
	TreeNodesColor = FColor::Black;
	bTreeNodesPersistent = false;
	TreeNodesLifeTime = 0.f;

	bShowRandomPoints = false;
	RandomPointsRadius = 32.f;
	RandomPointsSegments = 8;
	RandomPointsColor = FColor::Blue;
	bRandomPointsPersistent = false;
	RandomPointsLifeTime = 0.f;

	bShowNewPositions = false;
	NewPositionsRadius = 32.f;
	NewPositionsSegments = 8;
	NewPositionsColor = FColor::Cyan;
	bNewPositionsPersistent = false;
	NewPositionsLifeTime = 0.f;

	bShowResultPoints = true;
	ResultPointsRadius = 32.f;
	ResultPointsSegments = 8;
	ResultPointsColor = FColor::Green;
	bResultPointsPersistent = false;
	ResultPointsLifeTime = 0.f;
}

void URapidlyRandomTreeComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	SetExploreSpaceBox();

	UWorld* World = GetWorld();
	if (!World) return;

	// debug
	if (bShowExploreSpace)
	{
		DrawDebugBox(World, ExploreSpaceBox.GetCenter(), ExploreSpaceBox.GetExtent(), ExploreSpaceBoxColor, bExploreSpaceBoxPersistent, ExploreSpaceBoxLifeTime);
	}
	if (bShowTreeNodes)
	{
		for (const FRRTNode& Node : TreeNodes)
		{
			DrawDebugSphere(World, Node.Position, TreeNodesRadius, TreeNodesSegments, TreeNodesColor, bTreeNodesPersistent, TreeNodesLifeTime);

			FRRTNode CurrentNode = Node;
			while (CurrentNode.ParentNodeIndex != -1)
			{
				DrawDebugLine(World, CurrentNode.Position, TreeNodes[CurrentNode.ParentNodeIndex].Position, TreeNodesColor, bTreeNodesPersistent, TreeNodesLifeTime);
				CurrentNode = TreeNodes[CurrentNode.ParentNodeIndex];
			}
		}
	}
	if (bShowRandomPoints)
	{
		for (const FVector& Point : RandomPoints)
		{
			DrawDebugSphere(World, Point, RandomPointsRadius, RandomPointsSegments, RandomPointsColor, bRandomPointsPersistent, RandomPointsLifeTime);
		}
	}
	if (bShowNewPositions)
	{
		for (const FVector& Point : NewPositions)
		{
			DrawDebugSphere(World, Point, NewPositionsRadius, NewPositionsSegments, NewPositionsColor, bNewPositionsPersistent, NewPositionsLifeTime);
		}
	}
	if (bShowResultPoints)
	{
		for (int32 i = 0; i < ResultPoints.Num(); ++i)
		{
			DrawDebugSphere(World, ResultPoints[i], ResultPointsRadius, ResultPointsSegments, ResultPointsColor, bResultPointsPersistent, ResultPointsLifeTime);
			if (i < ResultPoints.Num() - 1)
			{
				DrawDebugLine(World, ResultPoints[i], ResultPoints[i + 1], ResultPointsColor, bResultPointsPersistent, ResultPointsLifeTime);
			}
		}
	}
}

void URapidlyRandomTreeComponent::BeginDestroy()
{
	Super::BeginDestroy();

	ClearRandomPoints();
	ClearNewPositions();
	ClearResultPoints();
	ClearTreeNodes();
}

bool URapidlyRandomTreeComponent::RunRRT(const FVector& InStartPosition, const FVector& InEndPosition, TArray<FVector>& OutResultPoints)
{
	if (Step <= 0)
	{
		OutResultPoints.Empty();
		return false;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		OutResultPoints.Empty();
		return false;
	}

	// For debug
	StartPosition = InStartPosition;
	EndPosition = InEndPosition;
	ClearRandomPoints();
	ClearNewPositions();
	ClearResultPoints();
	ClearTreeNodes();

	// 初始化树，将起点作为根节点
	FRRTNode RootNode;
	RootNode.Position = StartPosition;
	RootNode.ParentNodeIndex = -1;
	TreeNodes.Add(RootNode);

	OutResultPoints.Empty();

	// 设置搜索范围
	SetExploreSpaceBox();

	for (int32 Iter = 0; Iter < MaxIterations; ++Iter)
	{
		// 在搜索空间中随机采样（带目标偏向）
		FVector RandomPoint;
		if (FMath::FRand() < GoalBias)
		{
			RandomPoint = EndPosition;
		}
		else
		{
			RandomPoint = FMath::RandPointInBox(ExploreSpaceBox);
		}

		// For debug
		RandomPoints.Add(RandomPoint);

		// 在TreeNodes中找到离随机位置最近的节点
		int32 NearestIndex = 0;
		float MinDistance = FVector::Distance(TreeNodes[0].Position, RandomPoint);
		for (int32 i = 1; i < TreeNodes.Num(); ++i)
		{
			float DistanceFromRandomPointToIndexNode = FVector::Distance(TreeNodes[i].Position, RandomPoint);
			if (DistanceFromRandomPointToIndexNode < MinDistance)
			{
				MinDistance = DistanceFromRandomPointToIndexNode;
				NearestIndex = i;
			}
		}

		// 扩展新节点
		FVector DirectionFromNearestNodeToRandomPoint = (RandomPoint - TreeNodes[NearestIndex].Position).GetSafeNormal();
		float Distance = FVector::Distance(StartPosition, EndPosition);
		FVector NewPosition = TreeNodes[NearestIndex].Position + DirectionFromNearestNodeToRandomPoint * (Distance / Step);

		// For debug
		NewPositions.Add(NewPosition);

		// 碰撞检测
		FHitResult HitResult;
		bool bHit = World->LineTraceSingleByChannel(HitResult, TreeNodes[NearestIndex].Position, NewPosition, ECC_Visibility);
		if (!bHit)
		{
			FRRTNode NewNode;
			NewNode.Position = NewPosition;
			NewNode.ParentNodeIndex = NearestIndex;
			TreeNodes.Add(NewNode);

			// 符合要求，得到最终路径
			if (FVector::Distance(NewPosition, EndPosition) <= (Distance / Step) && !World->LineTraceSingleByChannel(HitResult, EndPosition, NewPosition, ECC_Visibility))
			{
				int32 CurrentIndex = TreeNodes.Num() - 1;
				while (CurrentIndex != -1)
				{
					OutResultPoints.Insert(TreeNodes[CurrentIndex].Position, 0);
					CurrentIndex = TreeNodes[CurrentIndex].ParentNodeIndex;
				}

				// For debug
				ResultPoints = OutResultPoints;

				return true;
			}
		}
	}

	OutResultPoints.Empty();
	return false;
}

bool URapidlyRandomTreeComponent::RunRRTStar(const FVector& InStartPosition, const FVector& InEndPosition, TArray<FVector>& OutResultPoints)
{
	if (Step <= 0)
	{
		OutResultPoints.Empty();
		return false;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		OutResultPoints.Empty();
		return false;
	}

	// For debug
	StartPosition = InStartPosition;
	EndPosition = InEndPosition;

	// Clear
	ClearRandomPoints();
	ClearNewPositions();
	ClearResultPoints();
	ClearTreeNodes();

	// 初始化树，将起点作为根节点
	FRRTNode RootNode;
	RootNode.Position = StartPosition;
	RootNode.ParentNodeIndex = -1;
	TreeNodes.Add(RootNode);

	OutResultPoints.Empty();

	// 设置搜索范围
	SetExploreSpaceBox();

	for (int32 Iter = 0; Iter < MaxIterations; ++Iter)
	{
		// 在搜索空间中随机采样（带目标偏向）
		FVector RandomPoint;
		if (FMath::FRand() < GoalBias)
		{
			RandomPoint = EndPosition;
		}
		else
		{
			RandomPoint = FMath::RandPointInBox(ExploreSpaceBox);
		}

		// For debug
		RandomPoints.Add(RandomPoint);

		// 在TreeNodes中找到离随机位置最近的节点
		int32 NearestIndex = 0;
		float MinDistance = FVector::Distance(TreeNodes[0].Position, RandomPoint);
		for (int32 i = 1; i < TreeNodes.Num(); ++i)
		{
			float DistanceFromRandomPointToIndexNode = FVector::Distance(TreeNodes[i].Position, RandomPoint);
			if (DistanceFromRandomPointToIndexNode < MinDistance)
			{
				MinDistance = DistanceFromRandomPointToIndexNode;
				NearestIndex = i;
			}
		}

		// 扩展新节点
		FVector DirectionFromNearestNodeToRandomPoint = (RandomPoint - TreeNodes[NearestIndex].Position).GetSafeNormal();
		float Distance = FVector::Distance(StartPosition, EndPosition);
		FVector NewPosition = TreeNodes[NearestIndex].Position + DirectionFromNearestNodeToRandomPoint * (Distance / Step);

		// For debug
		NewPositions.Add(NewPosition);

		// 搜索邻域节点（半径内所有节点）
		float NeighborRadius = Distance / Step * NeighborExp;
		// 保存所有处于邻域范围内的节点索引
		TArray<int32> NeighborIndices;
		for (int32 i = 0; i < TreeNodes.Num(); ++i)
		{
			float NeighborDistance = FVector::Distance(TreeNodes[i].Position, NewPosition);
			if (NeighborDistance <= NeighborRadius)
			{
				
				NeighborIndices.Add(i);
			}
		}
		// 在邻域范围内的节点中寻找最优父节点（成本最低）
		int32 BestParentIndex = NearestIndex; // 最优父节点默认为最近节点
		float MinCost = TreeNodes[NearestIndex].Cost + FVector::Distance(TreeNodes[NearestIndex].Position, NewPosition);
		for (int32 Index : NeighborIndices)
		{
			float Cost = TreeNodes[Index].Cost + FVector::Distance(TreeNodes[Index].Position, NewPosition);
			if (Cost < MinCost)
			{
				// 检查路径是否无碰撞
				FHitResult HitResult;
				bool bHit = World->LineTraceSingleByChannel(HitResult, TreeNodes[Index].Position, NewPosition, ECC_Visibility);
				if (!bHit)
				{
					MinCost = Cost;
					BestParentIndex = Index;
				}
			}
		}
		// 创建新节点（与RRT不同的是，RRTStar在这里会更新Cost）
		FRRTNode NewNode;
		NewNode.Position = NewPosition;
		NewNode.ParentNodeIndex = BestParentIndex;
		NewNode.Cost = MinCost;
		// 碰撞检测
		FHitResult NewNodeHitResult;
		if (!World->LineTraceSingleByChannel(NewNodeHitResult, TreeNodes[BestParentIndex].Position, NewPosition, ECC_Visibility))
		{
			TreeNodes.Add(NewNode);
		}

		// 重布线邻域节点
		for (int32 Index : NeighborIndices)
		{
			float NewCost = NewNode.Cost + FVector::Distance(NewNode.Position, TreeNodes[Index].Position);
			if (NewCost < TreeNodes[Index].Cost)
			{
				// 检查路径是否无碰撞
				FHitResult HitResult;
				bool bHit = World->LineTraceSingleByChannel(HitResult, NewNode.Position, TreeNodes[Index].Position, ECC_Visibility);
				if (!bHit)
				{
					TreeNodes[Index].ParentNodeIndex = TreeNodes.Num() - 1; // 新节点作为父节点
					TreeNodes[Index].Cost = NewCost;
				}
			}
		}
		FHitResult HitResult;
		// 符合要求，得到最终路径
		if (FVector::Distance(NewPosition, EndPosition) <= (Distance / Step) && !World->LineTraceSingleByChannel(HitResult, EndPosition, NewPosition, ECC_Visibility))
		{
			int32 CurrentIndex = TreeNodes.Num() - 1;
			while (CurrentIndex != -1)
			{
				OutResultPoints.Insert(TreeNodes[CurrentIndex].Position, 0);
				CurrentIndex = TreeNodes[CurrentIndex].ParentNodeIndex;
			}

			// For debug
			ResultPoints = OutResultPoints;

			return true;
		}
		
	}

	OutResultPoints.Empty();
	return false;
}

bool URapidlyRandomTreeComponent::RunRRTStarSingle(const FVector& InStartPosition, const FVector& InEndPosition, TArray<FVector>& OutResultPoints)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		OutResultPoints.Empty();
		return false;
	}

	// For debug
	StartPosition = InStartPosition;
	EndPosition = InEndPosition;

	// 初始化树，将起点作为根节点
	if (TreeNodes.IsEmpty())
	{
		FRRTNode RootNode;
		RootNode.Position = StartPosition;
		RootNode.ParentNodeIndex = -1;
		TreeNodes.Add(RootNode);
	}

	OutResultPoints.Empty();

	// 设置搜索范围
	SetExploreSpaceBox();

	// 在搜索空间中随机采样（带目标偏向）
	FVector RandomPoint;
	if (FMath::FRand() < GoalBias)
	{
		RandomPoint = EndPosition;
	}
	else
	{
		RandomPoint = FMath::RandPointInBox(ExploreSpaceBox);
	}

	// For debug
	RandomPoints.Add(RandomPoint);

	// 在TreeNodes中找到离随机位置最近的节点
	int32 NearestIndex = 0;
	float MinDistance = FVector::Distance(TreeNodes[0].Position, RandomPoint);
	for (int32 i = 1; i < TreeNodes.Num(); ++i)
	{
		float DistanceFromRandomPointToIndexNode = FVector::Distance(TreeNodes[i].Position, RandomPoint);
		if (DistanceFromRandomPointToIndexNode < MinDistance)
		{
			MinDistance = DistanceFromRandomPointToIndexNode;
			NearestIndex = i;
		}
	}

	// 扩展新节点
	FVector DirectionFromNearestNodeToRandomPoint = (RandomPoint - TreeNodes[NearestIndex].Position).GetSafeNormal();
	float Distance = FVector::Distance(StartPosition, EndPosition);
	FVector NewPosition = TreeNodes[NearestIndex].Position + DirectionFromNearestNodeToRandomPoint * (Distance / Step);

	// For debug
	NewPositions.Add(NewPosition);

	// 搜索邻域节点（半径内所有节点）
	float NeighborRadius = Distance / Step * NeighborExp;
	// 保存所有处于邻域范围内的节点索引
	TArray<int32> NeighborIndices;
	for (int32 i = 0; i < TreeNodes.Num(); ++i)
	{
		float NeighborDistance = FVector::Distance(TreeNodes[i].Position, NewPosition);
		if (NeighborDistance <= NeighborRadius)
		{

			NeighborIndices.Add(i);
		}
	}
	// 在邻域范围内的节点中寻找最优父节点（成本最低）
	int32 BestParentIndex = NearestIndex; // 最优父节点默认为最近节点
	float MinCost = TreeNodes[NearestIndex].Cost + FVector::Distance(TreeNodes[NearestIndex].Position, NewPosition);
	for (int32 Index : NeighborIndices)
	{
		float Cost = TreeNodes[Index].Cost + FVector::Distance(TreeNodes[Index].Position, NewPosition);
		if (Cost < MinCost)
		{
			// 检查路径是否无碰撞
			FHitResult HitResult;
			bool bHit = World->LineTraceSingleByChannel(HitResult, TreeNodes[Index].Position, NewPosition, ECC_Visibility);
			if (!bHit)
			{
				MinCost = Cost;
				BestParentIndex = Index;
			}
		}
	}
	// 创建新节点
	FRRTNode NewNode;
	NewNode.Position = NewPosition;
	NewNode.ParentNodeIndex = BestParentIndex;
	NewNode.Cost = MinCost;
	// 碰撞检测
	FHitResult NewNodeHitResult;
	if (!World->LineTraceSingleByChannel(NewNodeHitResult, TreeNodes[BestParentIndex].Position, NewPosition, ECC_Visibility))
	{
		TreeNodes.Add(NewNode);
	}

	// 重布线邻域节点
	for (int32 Index : NeighborIndices)
	{
		float NewCost = NewNode.Cost + FVector::Distance(NewNode.Position, TreeNodes[Index].Position);
		if (NewCost < TreeNodes[Index].Cost)
		{
			// 检查路径是否无碰撞
			FHitResult HitResult;
			bool bHit = World->LineTraceSingleByChannel(HitResult, NewNode.Position, TreeNodes[Index].Position, ECC_Visibility);
			if (!bHit)
			{
				TreeNodes[Index].ParentNodeIndex = TreeNodes.Num() - 1; // 新节点作为父节点
				TreeNodes[Index].Cost = NewCost;
			}
		}
	}
	FHitResult HitResult;
	// 符合要求，得到最终路径
	if (FVector::Distance(NewPosition, EndPosition) <= (Distance / Step) && !World->LineTraceSingleByChannel(HitResult, EndPosition, NewPosition, ECC_Visibility))
	{
		int32 CurrentIndex = TreeNodes.Num() - 1;
		while (CurrentIndex != -1)
		{
			OutResultPoints.Insert(TreeNodes[CurrentIndex].Position, 0);
			CurrentIndex = TreeNodes[CurrentIndex].ParentNodeIndex;
		}

		// For debug
		ResultPoints = OutResultPoints;

		return true;
	}

	return false;
}

void URapidlyRandomTreeComponent::BeginPlay()
{
	Super::BeginPlay();

	ClearRandomPoints();
	ClearNewPositions();
	ClearResultPoints();
	ClearTreeNodes();
}

void URapidlyRandomTreeComponent::SetExploreSpaceBox()
{
	const float Distance = FVector::Distance(StartPosition, EndPosition);
	const FVector CenterPosition = (StartPosition + EndPosition) * 0.5f;

	ExploreSpaceBox.Min = CenterPosition - FVector(Distance) * ExploreSpaceScale;
	ExploreSpaceBox.Max = CenterPosition + FVector(Distance) * ExploreSpaceScale;
}

