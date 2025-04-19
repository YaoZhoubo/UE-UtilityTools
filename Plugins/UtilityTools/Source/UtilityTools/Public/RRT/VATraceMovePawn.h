#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "Engine/DataTable.h"
#include "VATraceMovePawn.generated.h"

DECLARE_LOG_CATEGORY_CLASS(VATraceMovePawnLog, Log, All);

class USpringArmComponent;
class UCameraComponent;
class UCapsuleComponent;
class UStaticMeshComponent;
class USplineComponent;
class UFloatingPawnMovement;
class UCesiumGlobeAnchorComponent;
class USplineComponent;
class URapidlyRandomTreeComponent;

// 用作DataTable的结构
USTRUCT(BlueprintType)
struct FTraceMovePoint : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	float Longitude = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	float Latitude = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	float Height = 0.0f;
};

UCLASS(HideCategories = (Tick, Replication, Rendering, Collision, Actor,
	Input, HLOD, Physics, WorldPartition, Cooking, DataLayers, Networking))
class UTILITYTOOLS_API AVATraceMovePawn : public APawn
{
	GENERATED_BODY()

public:	
	UPROPERTY(EditAnywhere)
	TObjectPtr<UDataTable> TraceMovePointsDataTable;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FName> TargetPath;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	uint32 bMove : 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FVector MoveDirection;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float CurrentSpeed;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	AController* CurrentController;

public:
	AVATraceMovePawn();

	virtual void Tick(float DeltaTime) override;

	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

	UFUNCTION(BlueprintCallable, CallInEditor, Category = VATraceMovePawn)
	void ReverseTraceMovePoints();

	UFUNCTION(BlueprintCallable, CallInEditor, Category = VATraceMovePawn)
	void UpdateTraceMoveSplinePoints();

	/**
	 *	该函数会遍历TraceMovePoints中的点，并对每两个点进行RRT计算，
	 *  若存在新路径，则用新路径更新TraceMovePoints，并更新样条曲线
	 */
	UFUNCTION(BlueprintCallable, CallInEditor, Category = VATraceMovePawn)
	void CheckTraceCollisionAndCorrectTrace();

protected:
	virtual void BeginPlay() override;

private:
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USpringArmComponent> CameraBoom;

	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UCameraComponent> FollowCamera;

	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UCapsuleComponent> CapsuleComponent;

	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStaticMeshComponent> StaticMeshComponent;

	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UFloatingPawnMovement> FloatingPawnMovement;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
	TObjectPtr<URapidlyRandomTreeComponent> RapidlyRandomTreeComponent;

	/* 路径控制 */
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USplineComponent> SplineComponent;

	// 用于控制飞机飞行的方向
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
	float DistanceAlongSpline;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
	FVector Destination;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
	FVector DistanceFromDestination;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
	TArray<FVector> TraceMovePoints;
	/* 路径控制 */

private:
	void InitializeSplineComponent();
};
