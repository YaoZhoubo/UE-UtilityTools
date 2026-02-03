#include "SurfaceDrawer/SurfacePolygonTestActor.h"

#include "SurfaceDrawer/BVHConfig.h"

#include "Components/InputComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerInput.h"

DEFINE_LOG_CATEGORY_STATIC(LogSurfacePolygonTestActor, Log, All);

class FTimeLogScope
{
public:
	FTimeLogScope(const TCHAR* StaticLabel) : Label(StaticLabel), StartTime(FPlatformTime::Cycles()) {}
	~FTimeLogScope()
	{
		const uint32 EndTime = FPlatformTime::Cycles();
		UE_LOG(LogSurfacePolygonTestActor, Log, TEXT("%s 耗时 [%.2fs]"), Label, FPlatformTime::ToMilliseconds(EndTime - StartTime) / 1000.0f);
	}

private:
	const TCHAR* Label;
	const uint32 StartTime;
};

#define TEST_TIME_LOG_SCOPE(Name) FTimeLogScope TimeLogScope_ ## Name(TEXT(#Name))

ASurfacePolygonTestActor::ASurfacePolygonTestActor()
	: Opacity(1.f)
	, Color(FLinearColor::Red)
{
	PrimaryActorTick.bCanEverTick = true;

	// 创建根组件
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("RootComponent"));

	// 创建SurfaceLine组件
	SurfacePolygonComponent = CreateDefaultSubobject<USurfacePolygonComponent>(TEXT("SurfacePolygonComponent"));
}

void ASurfacePolygonTestActor::BeginPlay()
{
	Super::BeginPlay();

	APlayerController* PC = GetWorld()->GetFirstPlayerController();
	if (PC)
	{
		EnableInput(PC);
		PC->SetShowMouseCursor(true);

		InputComponent->BindKey(EKeys::LeftMouseButton, IE_Pressed, this, &ASurfacePolygonTestActor::OnLeftMouseButtonPressed);
		InputComponent->BindKey(EKeys::MiddleMouseButton, IE_Pressed, this, &ASurfacePolygonTestActor::OnMiddleMouseButtonPressed);
	}	
}

void ASurfacePolygonTestActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	SurfacePolygonComponent->SetProperties(Opacity, Color);
}

void ASurfacePolygonTestActor::SetCustomTriangles(const TArray<FTriangle>& InTriangles)
{
	if (SurfacePolygonComponent)
	{
		SurfacePolygonComponent->SetTriangles(InTriangles);
	}
}

void ASurfacePolygonTestActor::OnLeftMouseButtonPressed()
{
	APlayerController* PC = GetWorld()->GetFirstPlayerController();
	if (PC)
	{
		FHitResult HitResult;
		PC->GetHitResultUnderCursor(ECollisionChannel::ECC_Visibility, false, HitResult);
		Positions.Add(HitResult.Location);

		FBox Box(ForceInit);
		for (auto Position : Positions)
		{
			Box += Position;
		}
		TArray<FTriangle> NewTriangles;
		for(int32 i = 0; i < Positions.Num(); i++)
		{
			FTriangle Triangle;
			Triangle.Vertex1 = Positions[i];
			Triangle.Vertex2 = Positions[(i + 1) % Positions.Num()];
			Triangle.Vertex3 = Box.GetCenter();
			NewTriangles.Add(Triangle);
		}
		SurfacePolygonComponent->SetTriangles(NewTriangles);
		SurfacePolygonComponent->MarkRenderStateDirty();
	}
}

void ASurfacePolygonTestActor::OnMiddleMouseButtonPressed()
{
	Positions.Empty();
	SurfacePolygonComponent->SetTriangles(TArray<FTriangle>());
	SurfacePolygonComponent->MarkRenderStateDirty();
}