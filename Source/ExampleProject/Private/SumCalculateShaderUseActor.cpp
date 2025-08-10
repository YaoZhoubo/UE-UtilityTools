// Copyright (c) 2025 YaoZhoubo. All rights reserved.


#include "SumCalculateShaderUseActor.h"

#include "Components/SceneCaptureComponent2D.h"
#include "SumCalculateShader.h"


ASumCalculateShaderUseActor::ASumCalculateShaderUseActor()
{
	PrimaryActorTick.bCanEverTick = true;

	// 创建或获取场景捕捉组件
	CaptureComponent = CreateDefaultSubobject<USceneCaptureComponent2D>("CaptureComponent");
	CaptureComponent->SetupAttachment(RootComponent);

	// 配置捕捉组件
	CaptureComponent->CaptureSource = SCS_SceneColorSceneDepth; // 或 SCS_FinalColorHDR
	CaptureComponent->bCaptureEveryFrame = false;
	CaptureComponent->bCaptureOnMovement = false;
}

void ASumCalculateShaderUseActor::BeginPlay()
{
	Super::BeginPlay();
	FSumCalculateCSManager::Get()->BeginRendering();

	// 创建纹理时
	InputTexture = NewObject<UTextureRenderTarget2D>();
	InputTexture->RenderTargetFormat = RTF_RGBA8;
	InputTexture->InitAutoFormat(1024, 1024);
	InputTexture->bGPUSharedFlag = true;
	InputTexture->bSupportsUAV = true;
	InputTexture->bCanCreateUAV = true;
	InputTexture->UpdateResource();
	CaptureComponent->TextureTarget = InputTexture;
}

void ASumCalculateShaderUseActor::BeginDestroy()
{
	FSumCalculateCSManager::Get()->EndRendering();
	Super::BeginDestroy();
}

void ASumCalculateShaderUseActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	FSumCalculateCSManager* Manager = FSumCalculateCSManager::Get();

	if (Manager && Manager->IsResultReady())
	{
		// 获取结果
		float TotalSum = Manager->GetTotalSum();
		ResultArray = Manager->GetGroupSumsArray();

		// 使用结果（示例：打印到屏幕）
		GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Green,
			FString::Printf(TEXT("Total Sum: %f"), TotalSum));
	}
}

void ASumCalculateShaderUseActor::UpdateParams()
{
	CaptureComponent->CaptureScene();

	FSumCalculateCSParameters parameters(InputTexture);
	parameters.Value1 = Value1;
	parameters.Value2 = Value2;

	FSumCalculateCSManager::Get()->UpdateParameters(parameters);
}
