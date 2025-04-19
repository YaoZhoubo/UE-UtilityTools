#include "RRT/VATraceMovePawn.h"

#include "RRT/RapidlyRandomTreeComponent.h"

#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetArrayLibrary.h"
#include "Components/CapsuleComponent.h"
#include "Components/Splinecomponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/FloatingPawnMovement.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/Actor.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "DrawDebugHelpers.h"

AVATraceMovePawn::AVATraceMovePawn()
	: DistanceAlongSpline(500.0f)
{
	PrimaryActorTick.bCanEverTick = true;

	CapsuleComponent = CreateDefaultSubobject<UCapsuleComponent>("CapsuleComponent");
	CapsuleComponent->InitCapsuleSize(34.0f, 88.0f);
	CapsuleComponent->SetCollisionProfileName(UCollisionProfile::Pawn_ProfileName);
	CapsuleComponent->CanCharacterStepUpOn = ECB_No;
	CapsuleComponent->SetShouldUpdatePhysicsVolume(true);
	CapsuleComponent->SetCanEverAffectNavigation(false);
	CapsuleComponent->bDynamicObstacle = true;
	SetRootComponent(CapsuleComponent);

	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->bUsePawnControlRotation = true;
	CameraBoom->TargetArmLength = 2000.f;

	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->bUsePawnControlRotation = false; 

	StaticMeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("StaticMeshComponent"));
	StaticMeshComponent->SetupAttachment(RootComponent);

	FloatingPawnMovement = CreateDefaultSubobject<UFloatingPawnMovement>("FloatingPawnMovement");
	FloatingPawnMovement->MaxSpeed = 11111.11f;
	FloatingPawnMovement->Acceleration = 8000.f;

	SplineComponent = CreateDefaultSubobject<USplineComponent>("SplineComponent");
	SplineComponent->ReparamStepsPerSegment = 20;
	RapidlyRandomTreeComponent = CreateDefaultSubobject<URapidlyRandomTreeComponent>("RapidlyRandomTreeComponent");
}

void AVATraceMovePawn::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	DistanceFromDestination = Destination - GetActorLocation();
	if (DistanceFromDestination.Length() < 10.f)
	{
		bMove = false;
	}

	if (bMove)
	{
		float InputKeyClosestToWorldLocation = SplineComponent->FindInputKeyClosestToWorldLocation(GetActorLocation());
		FVector TargetPosition = SplineComponent->GetLocationAtSplineInputKey(InputKeyClosestToWorldLocation + 0.1f, ESplineCoordinateSpace::World);

		MoveDirection = TargetPosition - GetActorLocation();
		MoveDirection.Normalize();

		SetActorRotation(MoveDirection.ToOrientationQuat());
		FloatingPawnMovement->AddInputVector(MoveDirection);
	}
	CurrentSpeed = FloatingPawnMovement->Velocity.Length() * 0.036f;

	CurrentController = GetController();
}

void AVATraceMovePawn::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
}

void AVATraceMovePawn::ReverseTraceMovePoints()
{
	Algo::Reverse(TraceMovePoints);
}

void AVATraceMovePawn::UpdateTraceMoveSplinePoints()
{
	SplineComponent->SetSplineWorldPoints(TraceMovePoints);
	for (int8 i = 0; i < SplineComponent->GetNumberOfSplinePoints(); i++)
	{
		SplineComponent->SetSplinePointType(i, ESplinePointType::Curve);
	}

	// 设置终点
	if (!TraceMovePoints.IsEmpty())
	{
		Destination = TraceMovePoints.Top();
	}
}

void AVATraceMovePawn::CheckTraceCollisionAndCorrectTrace()
{
	UWorld* World = GetWorld();
	if (!World) return;

	TArray<FVector> CorrectTracePoints;
	for (int32 i = 0; i < TraceMovePoints.Num() - 1; ++i)
	{
		// 碰撞检测
		FHitResult HitResult;
		bool bHit = World->LineTraceSingleByChannel(HitResult, TraceMovePoints[i], TraceMovePoints[i + 1], ECC_Visibility);
		if (!bHit)
		{
			CorrectTracePoints.Add(TraceMovePoints[i]);
		}
		else
		{
			TArray<FVector> SubCorrectTracePoints;
			RapidlyRandomTreeComponent->RunRRT(TraceMovePoints[i], TraceMovePoints[i + 1], SubCorrectTracePoints);

			CorrectTracePoints.Append(SubCorrectTracePoints);
		}
	}
	CorrectTracePoints.Add(TraceMovePoints[TraceMovePoints.Num() - 1]);

	// 若有新路径则替换原来的路径，并更新样条曲线
	if (CorrectTracePoints.Num() > TraceMovePoints.Num())
	{
		TraceMovePoints = CorrectTracePoints;
		UpdateTraceMoveSplinePoints();
	}
}

void AVATraceMovePawn::BeginPlay()
{
	Super::BeginPlay();

	InitializeSplineComponent();
}

void AVATraceMovePawn::InitializeSplineComponent()
{
	AActor* SplineActor = GetWorld()->SpawnActor<AActor>(AActor::StaticClass());
	SplineComponent->AttachToComponent(SplineActor->GetRootComponent(), FAttachmentTransformRules::KeepWorldTransform);
	SplineComponent->ClearSplinePoints();
	for (const FName& TargetAreaName : TargetPath)
	{
		if (const FTraceMovePoint* TraceMovePoint = TraceMovePointsDataTable->FindRow<FTraceMovePoint>(TargetAreaName, TEXT("")))
		{
			FVector Position = FVector(TraceMovePoint->Longitude, TraceMovePoint->Latitude, TraceMovePoint->Height);

			TraceMovePoints.Add(Position);
		}
	}

	UpdateTraceMoveSplinePoints();

	SetActorLocation(SplineComponent->GetLocationAtSplinePoint(0, ESplineCoordinateSpace::World));
	SetActorRotation(SplineComponent->GetDirectionAtSplinePoint(0, ESplineCoordinateSpace::World).ToOrientationQuat());
}