#include "SumCalculateShader.h"

#include "GlobalShader.h"
#include "ShaderParameterStruct.h"
#include "RenderGraphUtils.h"
#include "RenderTargetPool.h"

#include "Modules/ModuleManager.h"
#include "Math/MathFwd.h"
#include "RenderGraphFwd.h"
#include "RenderGraphUtils.h"

// 头文件中声明
FBufferRHIRef GroupSumsBuffer;

class FSumCalculateCS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FSumCalculateCS);
	SHADER_USE_PARAMETER_STRUCT(FSumCalculateCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, InputTexture)
		SHADER_PARAMETER(FIntPoint, CachedRenderTargetSize)
		SHADER_PARAMETER(float, Value1)
		SHADER_PARAMETER(float, Value2)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<float>, GroupSums)
	END_SHADER_PARAMETER_STRUCT()

public:
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}

	static inline void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
	}

};

IMPLEMENT_GLOBAL_SHADER(FSumCalculateCS, "/MyShaders/SumCalculateCS.usf", "MainComputeShader", SF_Compute);

FSumCalculateCSManager* FSumCalculateCSManager::instance = nullptr;

void FSumCalculateCSManager::ProcessResult_RenderThread()
{
	check(IsInRenderingThread());

	if (!bReadbackPending || !GPUBufferReadback || !GPUBufferReadback->IsReady())
		return;

	// 锁定缓冲区
	uint32 NumBytes = ExpectedGroupCount * sizeof(float);
	const float* Data = (const float*)GPUBufferReadback->Lock(NumBytes);

	if (Data)
	{
		// 保存原始结果数组
		GroupSumsArray.SetNumUninitialized(ExpectedGroupCount);
		FMemory::Memcpy(GroupSumsArray.GetData(), Data, NumBytes);

		// 计算总和
		TotalSum = 0.0f;
		for (int32 i = 0; i < ExpectedGroupCount; i++)
		{
			TotalSum += Data[i];
		}

		// 标记结果可用
		bResultReady = true;

		// 解锁
		GPUBufferReadback->Unlock();
	}

	bReadbackPending = false;
}

void FSumCalculateCSManager::BeginRendering()
{
	if (OnPostResolvedSceneColorHandle.IsValid())
	{
		return;
	}
	bCachedParamsAreValid = false;
	const FName RendererModuleName("Renderer");
	IRendererModule* RendererModule = FModuleManager::GetModulePtr<IRendererModule>(RendererModuleName);
	if (RendererModule)
	{
		OnPostResolvedSceneColorHandle = RendererModule->GetResolvedSceneColorCallbacks().AddRaw(this, &FSumCalculateCSManager::Execute_RenderThread);
	}

	// 添加Tick委托
	if (!TickDelegateHandle.IsValid())
	{
		TickDelegateHandle = FTSTicker::GetCoreTicker().AddTicker(
			FTickerDelegate::CreateRaw(this, &FSumCalculateCSManager::TickDelegate)
		);
	}
}

// 添加每帧检查的函数
void FSumCalculateCSManager::Tick()
{
	if (bReadbackPending && GPUBufferReadback && GPUBufferReadback->IsReady())
	{
		// 在渲染线程处理结果
		ENQUEUE_RENDER_COMMAND(ProcessSumResult)(
			[this](FRHICommandListImmediate& RHICmdList)
			{
				this->ProcessResult_RenderThread();
			}
			);
	}
}

bool FSumCalculateCSManager::TickDelegate(float DeltaTime)
{
	Tick();
	return true; // 继续执行
}

void FSumCalculateCSManager::EndRendering()
{
	if (!OnPostResolvedSceneColorHandle.IsValid())
	{
		return;
	}

	const FName RendererModuleName("Renderer");
	IRendererModule* RendererModule = FModuleManager::GetModulePtr<IRendererModule>(RendererModuleName);
	if (RendererModule)
	{
		RendererModule->GetResolvedSceneColorCallbacks().Remove(OnPostResolvedSceneColorHandle);
	}

	OnPostResolvedSceneColorHandle.Reset();

	// 移除Tick委托
	if (TickDelegateHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(TickDelegateHandle);
		TickDelegateHandle.Reset();
	}
}

void FSumCalculateCSManager::UpdateParameters(FSumCalculateCSParameters& params)
{
	cachedParams = params;
	bCachedParamsAreValid = true;
}

void FSumCalculateCSManager::Execute_RenderThread(FRDGBuilder& GraphBuilder, const FSceneTextures& SceneTextures)
{
	//如果没有缓存的参数可用，则跳过
	//如果cachedParams中未提供渲染目标，则跳过
	if (!(bCachedParamsAreValid && cachedParams.InputTexture))
	{
		return;
	}

	bCachedParamsAreValid = false;

	check(IsInRenderingThread());

	// 创建输出缓冲区
	int32 GroupCount = FMath::DivideAndRoundUp(cachedParams.CachedRenderTargetSize.X * cachedParams.CachedRenderTargetSize.Y, 1024);
	ExpectedGroupCount = GroupCount;

	cachedParams.OutputBuffer = GraphBuilder.CreateBuffer(
		FRDGBufferDesc::CreateStructuredDesc(sizeof(float), GroupCount),
		TEXT("GroupSumsBuffer"));
	// 创建UAV
	FRDGBufferUAVRef OutputUAV = GraphBuilder.CreateUAV(cachedParams.OutputBuffer);

	// 获取InputTexture
	const FTextureRHIRef SourceTexture = cachedParams.InputTexture->GetRenderTargetResource()->GetTexture2DRHI();
	if (!SourceTexture)
	{
		return;
	}
	// 注册到RDG
	const FRDGTextureRef InputTextureRDG = GraphBuilder.RegisterExternalTexture(
		CreateRenderTarget(SourceTexture, TEXT("InputTextureRDG"))
	);

	//用客户端提供的缓存数据填充着色器参数结构
	FSumCalculateCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FSumCalculateCS::FParameters>();
	PassParameters->GroupSums = OutputUAV;
	PassParameters->InputTexture = InputTextureRDG;
	PassParameters->CachedRenderTargetSize = cachedParams.CachedRenderTargetSize;
	PassParameters->Value1 = cachedParams.Value1;
	PassParameters->Value2 = cachedParams.Value2;

	//从全局着色器映射中获取我们着色器类型的引用
	TShaderMapRef<FSumCalculateCS> SumCalculateCS(GetGlobalShaderMap(GMaxRHIFeatureLevel));

	// 调度计算着色器
	FComputeShaderUtils::AddPass(
		GraphBuilder,
		RDG_EVENT_NAME("SumCalculateCS"),
		SumCalculateCS,
		PassParameters,
		FIntVector(
			FMath::DivideAndRoundUp(
				cachedParams.CachedRenderTargetSize.X * cachedParams.CachedRenderTargetSize.Y, 
				1024),
			1,
			1
		)
	);

	// 添加缓冲区读回操作（关键新增部分）
	if (!GPUBufferReadback) {
		GPUBufferReadback = MakeUnique<FRHIGPUBufferReadback>(TEXT("SumCalculateCSReadback"));
	}
	AddEnqueueCopyPass(
		GraphBuilder,
		GPUBufferReadback.Get(),
		cachedParams.OutputBuffer,
		0
	);
	bReadbackPending = true;
	bResultReady = false; // 重置结果标志
}