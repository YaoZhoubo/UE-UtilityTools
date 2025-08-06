#include "MySimpleComputeShader.h"

#include "GlobalShader.h"
#include "ShaderParameterStruct.h"
#include "RenderGraphUtils.h"
#include "RenderTargetPool.h"


#include "Modules/ModuleManager.h"
#include "Math/MathFwd.h"
#include "RenderGraphFwd.h"

#define NUM_THREADS_PER_GROUP_DIMENSION 32

/// <summary>
/// Internal class thet holds the parameters and connects the HLSL Shader to the engine
/// </summary>
class FWhiteNoiseCS : public FGlobalShader
{
public:
	//Declare this class as a global shader
	DECLARE_GLOBAL_SHADER(FWhiteNoiseCS);
	//Tells the engine that this shader uses a structure for its parameters
	SHADER_USE_PARAMETER_STRUCT(FWhiteNoiseCS, FGlobalShader);
	/// <summary>
	/// DECLARATION OF THE PARAMETER STRUCTURE
	/// The parameters must match the parameters in the HLSL code
	/// For each parameter, provide the C++ type, and the name (Same name used in HLSL code)
	/// </summary>
	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float4>, OutputTexture)
		SHADER_PARAMETER(FVector2f, Dimensions)
		SHADER_PARAMETER(uint32, TimeStamp)
	END_SHADER_PARAMETER_STRUCT()

public:
	//Called by the engine to determine which permutations to compile for this shader
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}

	//Modifies the compilations environment of the shader
	static inline void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);

		//We're using it here to add some preprocessor defines. That way we don't have to change both C++ and HLSL code when we change the value for NUM_THREADS_PER_GROUP_DIMENSION
		OutEnvironment.SetDefine(TEXT("THREADGROUPSIZE_X"), NUM_THREADS_PER_GROUP_DIMENSION);
		OutEnvironment.SetDefine(TEXT("THREADGROUPSIZE_Y"), NUM_THREADS_PER_GROUP_DIMENSION);
		OutEnvironment.SetDefine(TEXT("THREADGROUPSIZE_Z"), 1);
	}

};

// This will tell the engine to create the shader and where the shader entry point is.
//                        ShaderType              ShaderPath             Shader function name    Type
IMPLEMENT_GLOBAL_SHADER(FWhiteNoiseCS, "/MyShaders/WhiteNoiseCS.usf", "MainComputeShader", SF_Compute);


//Static members
FWhiteNoiseCSManager* FWhiteNoiseCSManager::instance = nullptr;

//Begin the execution of the compute shader each frame
void FWhiteNoiseCSManager::BeginRendering()
{
	//If the handle is already initalized and valid, no need to do anything
	if (OnPostResolvedSceneColorHandle.IsValid())
	{
		return;
	}
	bCachedParamsAreValid = false;
	//Get the Renderer Module and add our entry to the callbacks so it can be executed each frame after the scene rendering is done
	const FName RendererModuleName("Renderer");
	IRendererModule* RendererModule = FModuleManager::GetModulePtr<IRendererModule>(RendererModuleName);
	if (RendererModule)
	{
		OnPostResolvedSceneColorHandle = RendererModule->GetResolvedSceneColorCallbacks().AddRaw(this, &FWhiteNoiseCSManager::Execute_RenderThread);
	}
}

//Stop the compute shader execution
void FWhiteNoiseCSManager::EndRendering()
{
	//If the handle is not valid then there's no cleanup to do
	if (!OnPostResolvedSceneColorHandle.IsValid())
	{
		return;
	}

	//Get the Renderer Module and remove our entry from the ResolvedSceneColorCallbacks
	const FName RendererModuleName("Renderer");
	IRendererModule* RendererModule = FModuleManager::GetModulePtr<IRendererModule>(RendererModuleName);
	if (RendererModule)
	{
		RendererModule->GetResolvedSceneColorCallbacks().Remove(OnPostResolvedSceneColorHandle);
	}

	OnPostResolvedSceneColorHandle.Reset();
}

//Update the parameters by a providing an instance of the Parameters structure used by the shader manager
void FWhiteNoiseCSManager::UpdateParameters(FWhiteNoiseCSParameters& params)
{
	cachedParams = params;
	bCachedParamsAreValid = true;
}


///<总结>
/// 创建着色器类型参数结构的实例，并使用缓存的着色器管理器参数结构对其进行填充
/// 从全局着色器映射中获取对着色器类型的引用
/// 使用参数结构实例来调度着色器
///</总结>
void FWhiteNoiseCSManager::Execute_RenderThread(FRDGBuilder& GraphBuilder, const FSceneTextures& SceneTextures)
{
	//如果没有缓存的参数可用，则跳过
	//如果cachedParams中未提供渲染目标，则跳过
	if (!(bCachedParamsAreValid && cachedParams.RenderTarget))
	{
		return;
	}

	//Render Thread Assertion
	check(IsInRenderingThread());


	//如果渲染目标无效，则通过提供描述符从渲染目标池中获取一个元素
	if (!ComputeShaderOutput.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("Not Valid"));
		FPooledRenderTargetDesc ComputeShaderOutputDesc(
			FPooledRenderTargetDesc::Create2DDesc(
				cachedParams.GetRenderTargetSize(), 
				cachedParams.RenderTarget->GetFormat(),
				FClearValueBinding::None, 
				TexCreate_None, 
				TexCreate_ShaderResource | TexCreate_UAV, 
				false
			)
		);
		ComputeShaderOutputDesc.DebugName = TEXT("WhiteNoiseCS_Output_RenderTarget");
		GRenderTargetPool.FindFreeElement(GraphBuilder.RHICmdList, ComputeShaderOutputDesc, ComputeShaderOutput, TEXT("WhiteNoiseCS_Output_RenderTarget"));
	}

	// 将ComputeShaderOutput注册到RDG
	FRDGTextureRef OutputTextureRDG = GraphBuilder.RegisterExternalTexture(ComputeShaderOutput, TEXT("WhiteNoiseCS_OutputTexture"));
	// 创建UAV
	FRDGTextureUAVDesc OutputTextureUAVDesc(OutputTextureRDG);
	FRDGTextureUAVRef OutputTextureUAV = GraphBuilder.CreateUAV(OutputTextureUAVDesc);

	//用客户端提供的缓存数据填充着色器参数结构
	FWhiteNoiseCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FWhiteNoiseCS::FParameters>();
	PassParameters->OutputTexture = OutputTextureUAV;
	PassParameters->Dimensions = FVector2f(cachedParams.GetRenderTargetSize().X, cachedParams.GetRenderTargetSize().Y);
	PassParameters->TimeStamp = cachedParams.TimeStamp;

	//从全局着色器映射中获取我们着色器类型的引用
	TShaderMapRef<FWhiteNoiseCS> whiteNoiseCS(GetGlobalShaderMap(GMaxRHIFeatureLevel));

	// 调度计算着色器 - 使用RDG版
	FComputeShaderUtils::AddPass(
		GraphBuilder,
		RDG_EVENT_NAME("WhiteNoiseCS"),
		whiteNoiseCS,
		PassParameters,
		FIntVector(
			FMath::DivideAndRoundUp(cachedParams.GetRenderTargetSize().X, NUM_THREADS_PER_GROUP_DIMENSION),
			FMath::DivideAndRoundUp(cachedParams.GetRenderTargetSize().Y, NUM_THREADS_PER_GROUP_DIMENSION),
			1
		)
	);

	// 将结果复制到客户端提供的渲染目标
	FTextureRenderTargetResource* RenderTargetResource = cachedParams.RenderTarget->GetRenderTargetResource();
	FRDGTextureRef DestTextureRDG = GraphBuilder.RegisterExternalTexture(
		CreateRenderTarget(RenderTargetResource->GetRenderTargetTexture(), TEXT("DestTexture"))
	);

	// 添加复制传递
	AddCopyTexturePass(
		GraphBuilder,
		OutputTextureRDG,
		DestTextureRDG,
		FRHICopyTextureInfo()
	);

}