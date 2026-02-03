#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SurfaceDrawer/SurfacePolygonComponent.h"
#include "SurfacePolygonTestActor.generated.h"

UCLASS()
class UTILITYTOOLS_API ASurfacePolygonTestActor : public AActor
{
	GENERATED_BODY()
	
public:	
	ASurfacePolygonTestActor();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

	/// \brief 设置自定义三角形数据
	UFUNCTION(BlueprintCallable, Category = "SurfacePolygonTest")
	void SetCustomTriangles(const TArray<FTriangle>& InTriangles);

private:
	void OnLeftMouseButtonPressed();
	void OnMiddleMouseButtonPressed();

public:
	/// \brief SurfacePolygon组件
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SurfacePolygonTest")
	TObjectPtr<USurfacePolygonComponent> SurfacePolygonComponent;

	/// \brief 透明度
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SurfacePolygonTest")
	float Opacity;

	/// \brief 颜色
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SurfacePolygonTest")
	FLinearColor Color;

private:
	/// \brief 点击位置集合
	TArray<FVector> Positions;
};
