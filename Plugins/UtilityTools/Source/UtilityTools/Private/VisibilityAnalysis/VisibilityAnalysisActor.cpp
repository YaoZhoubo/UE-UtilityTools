// Fill out your copyright notice in the Description page of Project Settings.

#include "VisibilityAnalysis/VisibilityAnalysisActor.h"

#include "Components/SceneCaptureComponent2D.h"
#include "Components/DecalComponent.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Kismet/KismetMaterialLibrary.h"

/**  Materials */
#include "Materials/Material.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialExpressionWorldPosition.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionTextureObjectParameter.h"
#include "Materials/MaterialExpressionCustom.h"
#include "Materials/MaterialExpressionFunctionInput.h"
#include "Materials/MaterialExpressionFunctionOutput.h"
#include "MaterialDomain.h"
/** ----------------------------------*/

AVisibilityAnalysisActor::AVisibilityAnalysisActor()
	: Distance(1024.f)
	, FOV(60.f)
	, DepthError(2.f)
	, AspectRatio(1.77f)
	, DepthCaptureResolution(512)
	, bOpenDebugFrustum(true)
	, bOpenDebugInEdit(true)
	, FrustumNear(100.f)
	, FrustumLineThickness(0.f)
	, FrustumLineDepthPriority(0.f)
	, FrustumColor(FColor::Blue)
{
	PrimaryActorTick.bCanEverTick = true;

	DepthCapture = CreateDefaultSubobject<USceneCaptureComponent2D>(TEXT("DepthCapture"));
	RootComponent = DepthCapture;
	VisibilityDecal = CreateDefaultSubobject<UDecalComponent>(TEXT("VisibilityDecal"));
	VisibilityDecal->SetupAttachment(RootComponent);

	VisibilityDecal->DecalSize = FVector(1.0, 1.0, 1.0);
	FVector VisibilityDecalScale3D(Distance * 1.1);
	VisibilityDecal->SetRelativeTransform(FTransform(FRotator(0.0), FVector(0.0), VisibilityDecalScale3D));

	DepthCapture->ProjectionType = ECameraProjectionMode::Perspective;
	DepthCapture->FOVAngle = FOV;
	DepthCapture->CaptureSource = ESceneCaptureSource::SCS_SceneDepth;
	DepthCapture->MaxViewDistanceOverride = Distance * 2;
}

void AVisibilityAnalysisActor::OnConstruction(const FTransform& Transform)
{
#if WITH_EDITOR
	CreateMaterial();
#endif // WITH_EDITOR
	UpdateParams();
	DrawFrustum();
}

void AVisibilityAnalysisActor::Destroyed()
{
	FlushPersistentDebugLines(GetWorld());
}

void AVisibilityAnalysisActor::BeginPlay()
{
	Super::BeginPlay();

	CreateMaterial();
}

void AVisibilityAnalysisActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	UpdateParams();

	UWorld* world = GetWorld();
	if (world)
	{
		if (bOpenDebugFrustum) DrawFrustum();
		else FlushPersistentDebugLines(GetWorld());
	}
}

void AVisibilityAnalysisActor::UpdateParams(float DeltaTime)
{
	check(DepthCapture)
	{
		if (!DepthCapture->TextureTarget)
		{
			DepthCapture->TextureTarget = NewObject<UTextureRenderTarget2D>();
			DepthCapture->TextureTarget->RenderTargetFormat = ETextureRenderTargetFormat::RTF_R32f;
			DepthCapture->FOVAngle = FOV;
			DepthCapture->MaxViewDistanceOverride = Distance * 2;
			DepthCapture->TextureTarget->ResizeTarget(DepthCaptureResolution, DepthCaptureResolution / AspectRatio);
		}
		else
		{
			DepthCapture->FOVAngle = FOV;
			DepthCapture->MaxViewDistanceOverride = Distance * 2;
			DepthCapture->TextureTarget->ResizeTarget(DepthCaptureResolution, DepthCaptureResolution / AspectRatio);
		}
	}
	check(VisibilityDecal)
	{
		VisibilityDecal->DecalSize = FVector(1.0, 1.0, 1.0);
		FVector VisibilityDecalScale3D(Distance * 1.1);
		VisibilityDecal->SetRelativeTransform(FTransform(FRotator(0.0), FVector(0.0), VisibilityDecalScale3D));
		if (!VisibilityMaterial)
		{
			VisibilityMID = NULL;
		}
		else
		{
			if (!VisibilityMID)
			{
				VisibilityMID = UKismetMaterialLibrary::CreateDynamicMaterialInstance(GetWorld(), VisibilityMaterial);
				VisibilityDecal->SetDecalMaterial(VisibilityMID);
			}
			else
			{
				VisibilityMID->SetVectorParameterValue(FName("Camera Relative Position"), DepthCapture->GetComponentLocation());
				VisibilityMID->SetVectorParameterValue(FName("X Camera to World Vector"), DepthCapture->GetRightVector());
				VisibilityMID->SetVectorParameterValue(FName("Y Camera to World Vector"), DepthCapture->GetUpVector());
				VisibilityMID->SetVectorParameterValue(FName("Z Camera to World Vector"), DepthCapture->GetForwardVector());
				VisibilityMID->SetTextureParameterValue(FName("Depth Texture Sample"), DepthCapture->TextureTarget);
				VisibilityMID->SetScalarParameterValue(FName("Depth Error"), DepthError);
				VisibilityMID->SetScalarParameterValue(FName("FOV"), FOV);
				VisibilityMID->SetScalarParameterValue(FName("Aspect Ratio"), AspectRatio);
				VisibilityMID->SetScalarParameterValue(FName("Distance"), Distance);
			}
		}
	}
}

#if WITH_EDITOR
UMaterialExpression* AVisibilityAnalysisActor::CreateMaterialExpression(UMaterial* Material, TSubclassOf<UMaterialExpression> ExpressionClass)
{
	UMaterialExpression* NewExpression = nullptr;
	if (Material)
	{
		UObject* ExpressionOuter = Material;

		NewExpression = NewObject<UMaterialExpression>(ExpressionOuter, ExpressionClass.Get(), NAME_None, RF_Transactional);
		if (Material)
		{
			Material->GetEditorOnlyData()->ExpressionCollection.AddExpression(NewExpression);
			NewExpression->Material = Material;
		}
		NewExpression->MaterialExpressionEditorX = 0;
		NewExpression->MaterialExpressionEditorY = 0;

		NewExpression->UpdateMaterialExpressionGuid(true, true);

		UMaterialExpressionFunctionInput* FunctionInput = Cast<UMaterialExpressionFunctionInput>(NewExpression);
		if (FunctionInput)
		{
			FunctionInput->ConditionallyGenerateId(true);
			FunctionInput->ValidateName();
		}

		UMaterialExpressionFunctionOutput* FunctionOutput = Cast<UMaterialExpressionFunctionOutput>(NewExpression);
		if (FunctionOutput)
		{
			FunctionOutput->ConditionallyGenerateId(true);
			FunctionOutput->ValidateName();
		}

		NewExpression->UpdateParameterGuid(true, true);

		if (NewExpression->HasAParameterName())
		{
			NewExpression->ValidateParameterName(false);
		}

		if (Material)
		{
			Material->AddExpressionParameter(NewExpression, Material->EditorParameters);
		}
		NewExpression->MarkPackageDirty();
	}
	return NewExpression;
}

void AVisibilityAnalysisActor::CreateMaterial()
{
	if (VisibilityMaterial) return;

	VisibilityMaterial = NewObject<UMaterial>(this, FName("VisibilityMaterial"), RF_Transactional);

	VisibilityMaterial->SetTextureParameterValueEditorOnly("Depth Capture Texture", DepthCapture->TextureTarget);
	VisibilityMaterial->MaterialDomain = EMaterialDomain::MD_DeferredDecal;
	VisibilityMaterial->BlendMode = EBlendMode::BLEND_Translucent;

	// WorldPositionParam
	UMaterialExpressionWorldPosition* WorldPosition = Cast<UMaterialExpressionWorldPosition>(
		CreateMaterialExpression(VisibilityMaterial, UMaterialExpressionWorldPosition::StaticClass())
	);
	VisibilityMaterial->AddExpressionParameter(WorldPosition, VisibilityMaterial->EditorParameters);
	// CameraRelativePosition Param
	UMaterialExpressionVectorParameter* CameraPositionParam = Cast<UMaterialExpressionVectorParameter>(
		CreateMaterialExpression(VisibilityMaterial, UMaterialExpressionVectorParameter::StaticClass())
	);
	CameraPositionParam->SetEditableName("Camera Relative Position");
	VisibilityMaterial->AddExpressionParameter(CameraPositionParam, VisibilityMaterial->EditorParameters);
	// X Camera to World Vector Param
	UMaterialExpressionVectorParameter* XCameraToWorldVectorParam = Cast<UMaterialExpressionVectorParameter>(
		CreateMaterialExpression(VisibilityMaterial, UMaterialExpressionVectorParameter::StaticClass())
	);
	XCameraToWorldVectorParam->SetEditableName("X Camera to World Vector");
	VisibilityMaterial->AddExpressionParameter(XCameraToWorldVectorParam, VisibilityMaterial->EditorParameters);
	// Y Camera to World Vector Param
	UMaterialExpressionVectorParameter* YCameraToWorldVectorParam = Cast<UMaterialExpressionVectorParameter>(
		CreateMaterialExpression(VisibilityMaterial, UMaterialExpressionVectorParameter::StaticClass())
	);
	YCameraToWorldVectorParam->SetEditableName("Y Camera to World Vector");
	VisibilityMaterial->AddExpressionParameter(YCameraToWorldVectorParam, VisibilityMaterial->EditorParameters);
	// Z Camera to World Vector Param
	UMaterialExpressionVectorParameter* ZCameraToWorldVectorParam = Cast<UMaterialExpressionVectorParameter>(
		CreateMaterialExpression(VisibilityMaterial, UMaterialExpressionVectorParameter::StaticClass())
	);
	ZCameraToWorldVectorParam->SetEditableName("Z Camera to World Vector");
	VisibilityMaterial->AddExpressionParameter(ZCameraToWorldVectorParam, VisibilityMaterial->EditorParameters);
	// DepthErrorParam
	UMaterialExpressionScalarParameter* DepthErrorParam = Cast<UMaterialExpressionScalarParameter>(
		CreateMaterialExpression(VisibilityMaterial, UMaterialExpressionScalarParameter::StaticClass())
	);
	DepthErrorParam->SetEditableName("Depth Error");
	VisibilityMaterial->AddExpressionParameter(DepthErrorParam, VisibilityMaterial->EditorParameters);
	// FOVParam
	UMaterialExpressionScalarParameter* FOVParam = Cast<UMaterialExpressionScalarParameter>(
		CreateMaterialExpression(VisibilityMaterial, UMaterialExpressionScalarParameter::StaticClass())
	);
	FOVParam->SetEditableName("FOV");
	VisibilityMaterial->AddExpressionParameter(FOVParam, VisibilityMaterial->EditorParameters);
	// AspectRatioParam
	UMaterialExpressionScalarParameter* AspectRatioParam = Cast<UMaterialExpressionScalarParameter>(
		CreateMaterialExpression(VisibilityMaterial, UMaterialExpressionScalarParameter::StaticClass())
	);
	AspectRatioParam->SetEditableName("Aspect Ratio");
	VisibilityMaterial->AddExpressionParameter(AspectRatioParam, VisibilityMaterial->EditorParameters);
	// DepthTextureSampleParam
	UMaterialExpression* DepthTextureSampleParam = Cast<UMaterialExpressionTextureObjectParameter>(
		CreateMaterialExpression(VisibilityMaterial, UMaterialExpressionTextureObjectParameter::StaticClass())
	);
	DepthTextureSampleParam->SetEditableName("Depth Texture Sample");
	VisibilityMaterial->AddExpressionParameter(DepthTextureSampleParam, VisibilityMaterial->EditorParameters);
	// FOVParam
	UMaterialExpressionScalarParameter* DistanceParam = NewObject<UMaterialExpressionScalarParameter>(VisibilityMaterial);
	DistanceParam->SetEditableName("Distance");
	VisibilityMaterial->AddExpressionParameter(DistanceParam, VisibilityMaterial->EditorParameters);
	// Custom
	UMaterialExpressionCustom* Custom1 = Cast<UMaterialExpressionCustom>(
		CreateMaterialExpression(VisibilityMaterial, UMaterialExpressionCustom::StaticClass())
	);
	Custom1->Inputs.SetNum(10);
	Custom1->Inputs[0] = FCustomInput{ "WorldPosition" };
	Custom1->Inputs[1] = FCustomInput{ "CameraPosition" };
	Custom1->Inputs[2] = FCustomInput{ "Xdirection" };
	Custom1->Inputs[3] = FCustomInput{ "Ydirection" };
	Custom1->Inputs[4] = FCustomInput{ "Zdirection" };
	Custom1->Inputs[5] = FCustomInput{ "AspectRatio" };
	Custom1->Inputs[6] = FCustomInput{ "FOV" };
	Custom1->Inputs[7] = FCustomInput{ "DepthError" };
	Custom1->Inputs[8] = FCustomInput{ "DepthTexture" };
	Custom1->Inputs[9] = FCustomInput{ "Distance" };
	Custom1->OutputType = ECustomMaterialOutputType::CMOT_Float3;
	// connect custom
	WorldPosition->ConnectExpression(Custom1->GetInput(0), 0);
	CameraPositionParam->ConnectExpression(Custom1->GetInput(1), 0);
	XCameraToWorldVectorParam->ConnectExpression(Custom1->GetInput(2), 0);
	YCameraToWorldVectorParam->ConnectExpression(Custom1->GetInput(3), 0);
	ZCameraToWorldVectorParam->ConnectExpression(Custom1->GetInput(4), 0);
	AspectRatioParam->ConnectExpression(Custom1->GetInput(5), 0);
	FOVParam->ConnectExpression(Custom1->GetInput(6), 0);
	DepthErrorParam->ConnectExpression(Custom1->GetInput(7), 0);
	DepthTextureSampleParam->ConnectExpression(Custom1->GetInput(8), 0);
	DistanceParam->ConnectExpression(Custom1->GetInput(9), 0);
	// Code
	Custom1->Code = R"(
// Input: WorldPosition, CameraDirectiton, Xdirection, Ydirection, Zdirection, AspectRatio, FOV, DepthError, DepthTexture, Distance
float3 position = WorldPosition - CameraPosition;
float3 direction = normalize(position);
float cullingExp = 0.f;
float3 resultColor = float3(0.f, 0.f, 0.f);
float3 xzDirection = normalize(position - dot(position, Ydirection) * Ydirection);
float3 yzDirection = normalize(position - dot(position, Xdirection) * Xdirection);
float xzAngle = atan(sqrt(1 - 1 / (AspectRatio * AspectRatio + 1)) * tan(radians(FOV / 2)));
float yzAngle = atan(sqrt(1 / (AspectRatio * AspectRatio + 1)) * tan(radians(FOV / 2)));

float distanceFromXY = dot(position, Zdirection);

if (dot(xzDirection, Zdirection) > cos(xzAngle) && dot(yzDirection, Zdirection) > cos(yzAngle) && distanceFromXY < Distance) cullingExp = 1.f;

float depthOffset = pow(10, -8) * pow(distanceFromXY, 2) - pow(10, -16) * pow(distanceFromXY, 3)
+ pow(10, -24) * pow(distanceFromXY, 4) - pow(10, -32) * pow(distanceFromXY, 5);
distanceFromXY = distanceFromXY - depthOffset - DepthError;

float2 worldToCameraScreenPosition = float2(dot(position, Xdirection),  dot(position, Ydirection)) / dot(position, Zdirection);
float2 tanFov = tan(radians(FOV / 2)) * float2(1, 1 / AspectRatio);
float2 ScreenUV = worldToCameraScreenPosition / tanFov;
ScreenUV = ScreenUV * float2(0.5, -0.5) + float2(0.5, 0.5);
float depthValue = DepthTexture.Sample(DepthTextureSampler, ScreenUV).r;

if(distanceFromXY > depthValue) resultColor = float3(1, 0, 0);
else resultColor = float3(0, 1, 0);
resultColor *= cullingExp;
resultColor *= 0.2;
return resultColor;
)";
	VisibilityMaterial->GetEditorOnlyData()->EmissiveColor.Expression = Custom1;
	VisibilityMaterial->ForceRecompileForRendering();
	VisibilityMaterial->PostEditChange();
	VisibilityMaterial->MarkPackageDirty();
}
#endif // WITH_EDITOR

void AVisibilityAnalysisActor::DrawFrustum()
{
	UWorld* world = GetWorld();
	if (world)
	{
		float diagonalLenth = FrustumNear * FMath::Tan(FMath::DegreesToRadians(FOV * 0.5f));
		float yLength = FMath::Sqrt(diagonalLenth * diagonalLenth / (1 + AspectRatio * AspectRatio));
		float xLength = yLength * AspectRatio;

		FVector startPostion = GetActorLocation();

		FVector nearLeftTopPosition = GetActorLocation() + GetActorForwardVector() * FrustumNear - GetActorRightVector() * xLength + GetActorUpVector() * yLength;
		FVector nearRightTopPosition = GetActorLocation() + GetActorForwardVector() * FrustumNear + GetActorRightVector() * xLength + GetActorUpVector() * yLength;
		FVector nearRightBottomPosition = GetActorLocation() + GetActorForwardVector() * FrustumNear + GetActorRightVector() * xLength - GetActorUpVector() * yLength;
		FVector nearLeftBottomPosition = GetActorLocation() + GetActorForwardVector() * FrustumNear - GetActorRightVector() * xLength - GetActorUpVector() * yLength;

		FVector directionLeftTop = nearLeftTopPosition - startPostion;
		FVector directionRightTop = nearRightTopPosition - startPostion;
		FVector directionRightBottom = nearRightBottomPosition - startPostion;
		FVector directionLeftBottom = nearLeftBottomPosition - startPostion;
		directionLeftTop.Normalize();
		directionRightTop.Normalize();
		directionRightBottom.Normalize();
		directionLeftBottom.Normalize();

		FVector farLeftTopPosition = startPostion + directionLeftTop * (Distance / FMath::Cos(FMath::DegreesToRadians(FOV * 0.5f)));
		FVector farRightTopPosition = startPostion + directionRightTop * (Distance / FMath::Cos(FMath::DegreesToRadians(FOV * 0.5f)));
		FVector farRightBottomPosition = startPostion + directionRightBottom * (Distance / FMath::Cos(FMath::DegreesToRadians(FOV * 0.5f)));
		FVector farLeftBottomPosition = startPostion + directionLeftBottom * (Distance / FMath::Cos(FMath::DegreesToRadians(FOV * 0.5f)));

		FlushPersistentDebugLines(world);

		DrawDebugLine(world, startPostion, farLeftTopPosition, FrustumColor, bOpenDebugInEdit, -1.0f, FrustumLineDepthPriority, FrustumLineThickness);
		DrawDebugLine(world, startPostion, (farLeftTopPosition + farRightTopPosition) / 2, FrustumColor, bOpenDebugInEdit, -1.0f, FrustumLineDepthPriority, FrustumLineThickness);
		DrawDebugLine(world, startPostion, farRightTopPosition, FrustumColor, bOpenDebugInEdit, -1.0f, FrustumLineDepthPriority, FrustumLineThickness);
		DrawDebugLine(world, startPostion, (farRightTopPosition + farRightBottomPosition) / 2, FrustumColor, bOpenDebugInEdit, -1.0f, FrustumLineDepthPriority, FrustumLineThickness);
		DrawDebugLine(world, startPostion, farRightBottomPosition, FrustumColor, bOpenDebugInEdit, -1.0f, FrustumLineDepthPriority, FrustumLineThickness);
		DrawDebugLine(world, startPostion, (farRightBottomPosition + farLeftBottomPosition) / 2, FrustumColor, bOpenDebugInEdit, -1.0f, FrustumLineDepthPriority, FrustumLineThickness);
		DrawDebugLine(world, startPostion, farLeftBottomPosition, FrustumColor, bOpenDebugInEdit, -1.0f, FrustumLineDepthPriority, FrustumLineThickness);
		DrawDebugLine(world, startPostion, (farLeftBottomPosition + farLeftTopPosition) / 2, FrustumColor, bOpenDebugInEdit, -1.0f, FrustumLineDepthPriority, FrustumLineThickness);

		DrawDebugLine(world, nearLeftTopPosition, nearRightTopPosition, FrustumColor, bOpenDebugInEdit, -1.0f, FrustumLineDepthPriority, FrustumLineThickness);
		DrawDebugLine(world, nearRightTopPosition, nearRightBottomPosition, FrustumColor, bOpenDebugInEdit, -1.0f, FrustumLineDepthPriority, FrustumLineThickness);
		DrawDebugLine(world, nearRightBottomPosition, nearLeftBottomPosition, FrustumColor, bOpenDebugInEdit, -1.0f, FrustumLineDepthPriority, FrustumLineThickness);
		DrawDebugLine(world, nearLeftBottomPosition, nearLeftTopPosition, FrustumColor, bOpenDebugInEdit, -1.0f, FrustumLineDepthPriority, FrustumLineThickness);
		DrawDebugLine(world, farLeftTopPosition, farRightTopPosition, FrustumColor, bOpenDebugInEdit, -1.0f, FrustumLineDepthPriority, FrustumLineThickness);
		DrawDebugLine(world, farRightTopPosition, farRightBottomPosition, FrustumColor, bOpenDebugInEdit, -1.0f, FrustumLineDepthPriority, FrustumLineThickness);
		DrawDebugLine(world, farRightBottomPosition, farLeftBottomPosition, FrustumColor, bOpenDebugInEdit, -1.0f, FrustumLineDepthPriority, FrustumLineThickness);
		DrawDebugLine(world, farLeftBottomPosition, farLeftTopPosition, FrustumColor, bOpenDebugInEdit, -1.0f, FrustumLineDepthPriority, FrustumLineThickness);
	}
}
