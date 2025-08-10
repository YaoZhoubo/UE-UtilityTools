#pragma once

#include "CoreMinimal.h"
#include "Runtime/Engine/Classes/Engine/TextureRenderTarget2D.h"

struct  FSumCalculateCSParameters
{
	const TArray<float>& GetResultArray() const { return ResultArray; }
	TArray<float> ResultArray;


	FRDGBufferRef OutputBuffer;
	UTextureRenderTarget2D* InputTexture;
	FIntPoint CachedRenderTargetSize;
	float Value1;
	float Value2;

	FIntPoint GetRenderTargetSize() const
	{
		return CachedRenderTargetSize;
	}

	FSumCalculateCSParameters() {}
	FSumCalculateCSParameters(UTextureRenderTarget2D* IORenderTarget)
		: InputTexture(IORenderTarget)
	{
		CachedRenderTargetSize = InputTexture ? FIntPoint(InputTexture->SizeX, InputTexture->SizeY) : FIntPoint::ZeroValue;
	}
};


class MYSHADERS_API FSumCalculateCSManager
{
public:
	//Get the instance
	static FSumCalculateCSManager* Get()
	{
		if (!instance)
			instance = new FSumCalculateCSManager();
		return instance;
	};

	void BeginRendering();
	void Tick();
	bool TickDelegate(float DeltaTime);
	void EndRendering();

	void UpdateParameters(FSumCalculateCSParameters& DrawParameters);

	// 获取最终总和结果
	float GetTotalSum() const { return TotalSum; }

	// 检查结果是否可用
	bool IsResultReady() const { return bResultReady; }

	// 获取每个组的结果数组
	const TArray<float>& GetGroupSumsArray() const { return GroupSumsArray; }

private:
	FSumCalculateCSManager() = default;

	static FSumCalculateCSManager* instance;

	FDelegateHandle OnPostResolvedSceneColorHandle;
	// 添加Tick委托句柄
	FTSTicker::FDelegateHandle TickDelegateHandle;

	FSumCalculateCSParameters cachedParams;

	volatile bool bCachedParamsAreValid;

	// 添加以下私有成员
	TUniquePtr<FRHIGPUBufferReadback> GPUBufferReadback;
	float TotalSum = 0.0f;
	bool bResultReady = false;
	int32 ExpectedGroupCount = 0;

	// 添加用于存储结果数组的变量
	TArray<float> GroupSumsArray;

	// 用于线程安全的标志
	FThreadSafeBool bReadbackPending = false;

	// 添加处理结果的函数
	void ProcessResult_RenderThread();

public:
	void Execute_RenderThread(FRDGBuilder& GraphBuilder, const FSceneTextures& SceneTextures);
};
