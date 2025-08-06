// Copyright (c) 2025 YaoZhoubo. All rights reserved.


#include "DepthSampleActor.h"

#include "Components/SceneCaptureComponent2D.h"
#include "MySimpleComputeShader.h"

// Sets default values
ADepthSampleActor::ADepthSampleActor()
{
	PrimaryActorTick.bCanEverTick = true;

	PrimaryActorTick.bCanEverTick = true;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;

	StaticMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("StaticMesh"));

	TimeStamp = 0;

}

// Called when the game starts or when spawned
void ADepthSampleActor::BeginPlay()
{
	Super::BeginPlay();
	FWhiteNoiseCSManager::Get()->BeginRendering();


	//Assuming that the static mesh is already using the material that we're targeting, we create an instance and assign it to it
	UMaterialInstanceDynamic* MID = StaticMesh->CreateAndSetMaterialInstanceDynamic(0);
	MID->SetTextureParameterValue("InputTexture", (UTexture*)RenderTarget);
}

void ADepthSampleActor::BeginDestroy()
{
	FWhiteNoiseCSManager::Get()->EndRendering();
	Super::BeginDestroy();
}

// Called every frame
void ADepthSampleActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	//Update parameters
	FWhiteNoiseCSParameters parameters(RenderTarget);
	TimeStamp++;
	parameters.TimeStamp = TimeStamp;
	FWhiteNoiseCSManager::Get()->UpdateParameters(parameters);

}
