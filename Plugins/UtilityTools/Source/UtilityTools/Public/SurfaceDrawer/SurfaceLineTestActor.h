#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SurfaceDrawer/SurfaceLineComponent.h"
#include "SurfaceLineTestActor.generated.h"

/**
 * @brief SurfaceLineTestActor类
 *
 * 该类用于测试USurfaceLineComponent，
 */
UCLASS()
class UTILITYTOOLS_API ASurfaceLineTestActor : public AActor
{
	GENERATED_BODY()
	
public:	
	ASurfaceLineTestActor();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

	/// \brief 设置自定义多边形
	UFUNCTION(BlueprintCallable, Category = "SurfaceLineTest")
	void SetCustomPolygons(const TArray<FPolygon>& Polygons);

private:
	void OnLeftMouseButtonPressed();
	void OnMiddleMouseButtonPressed();

public:
	/// \brief SurfaceLine组件
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SurfaceLineTest")
	TObjectPtr<USurfaceLineComponent> SurfaceLineComponent;

	/// \brief 线宽
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SurfaceLineTest")
	float Width = 5.f;

	/// \brief 透明度
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SurfaceLineTest")
	float Opacity = 1.f;

	/// \brief 颜色
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SurfaceLineTest")
	FLinearColor Color = FLinearColor::Green;

	/// \brief 使用像素单位
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SurfaceLineTest")
	uint8 bUsePixelUnit : 1;

private:
	TArray<FVector> Positions;
};
