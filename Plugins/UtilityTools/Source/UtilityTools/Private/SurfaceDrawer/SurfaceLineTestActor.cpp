#include "SurfaceDrawer/SurfaceLineTestActor.h"

#include "SurfaceDrawer/BVHConfig.h"

#include "Components/InputComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerInput.h"


DEFINE_LOG_CATEGORY_STATIC(LogSurfaceLineTestActor, Log, All);

ASurfaceLineTestActor::ASurfaceLineTestActor()
{
	PrimaryActorTick.bCanEverTick = true;

	// 创建根组件
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("RootComponent"));

	// 创建SurfaceLine组件
	SurfaceLineComponent = CreateDefaultSubobject<USurfaceLineComponent>(TEXT("SurfaceLineComponent"));
}

void ASurfaceLineTestActor::BeginPlay()
{
	Super::BeginPlay();

	APlayerController* PC = GetWorld()->GetFirstPlayerController();
	if (PC)
	{
		EnableInput(PC);
		PC->SetShowMouseCursor(true);

		InputComponent->BindKey(EKeys::LeftMouseButton, IE_Pressed, this, &ASurfaceLineTestActor::OnLeftMouseButtonPressed);
		InputComponent->BindKey(EKeys::MiddleMouseButton, IE_Pressed, this, &ASurfaceLineTestActor::OnMiddleMouseButtonPressed);
	}
}

void ASurfaceLineTestActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if(!SurfaceLineComponent) return;

	SurfaceLineComponent->SetProperties(Width, Opacity, Color);
	SurfaceLineComponent->bUsePixelUnit = bUsePixelUnit;
}

void ASurfaceLineTestActor::SetCustomPolygons(const TArray<FPolygon>& Polygons)
{
	if (!SurfaceLineComponent) return;

	SurfaceLineComponent->SetPolygons(Polygons);
}

void ASurfaceLineTestActor::OnLeftMouseButtonPressed()
{
	APlayerController* PC = GetWorld()->GetFirstPlayerController();
	if (PC)
	{
		FHitResult HitResult;
		PC->GetHitResultUnderCursor(ECollisionChannel::ECC_Visibility, false, HitResult);
		Positions.Add(HitResult.Location);

		TArray<FPolygon> NewPolygons;
		FPolygon Polygon(Positions);
		NewPolygons.Add(Polygon);

		if (!SurfaceLineComponent) return;

		SurfaceLineComponent->SetPolygons(NewPolygons);
		SurfaceLineComponent->MarkRenderStateDirty();
	}
}

void ASurfaceLineTestActor::OnMiddleMouseButtonPressed()
{
	Positions.Empty();

	if (!SurfaceLineComponent) return;
	SurfaceLineComponent->SetPolygons(TArray<FPolygon>());
	SurfaceLineComponent->MarkRenderStateDirty();
}

