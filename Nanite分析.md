# Nanite分析
## NaniteBuilder(.h/.cpp)
Build函数是构建 Nanite 数据的核心函数。它负责将输入的网格数据转换为 Nanite 格式的资源。

    bool FBuilderModule::Build(
        FResources& Resources,                          // 输出：最终的Nanite资源
        FInputMeshData& InputMeshData,                  // 输入：原始网格数据
        FOutputMeshData* OutFallbackMeshData,           // 输出：传统LOD网格（可选）
        FOutputMeshData* OutRayTracingFallbackMeshData, // 输出：光线追踪回落网格
        const FRayTracingFallbackBuildSettings* RayTracingFallbackBuildSettings,
        const FMeshNaniteSettings& Settings,            // 构建设置
        FInputAssemblyData* InputAssemblyData           // 装配数据（复杂模型）
    )
1. 构建流程
    * 预处理和条件判断

            // 判断是否需要进行网格简化
            const bool bFallbackIsReduced = ...; // 各种简化条件判断
            const bool bRayTracingFallbackIsReduced = ...;
            const bool bCanFreeInputMeshData = bFallbackIsReduced && bRayTracingFallbackIsReduced;
    * 构建中间资源

            FIntermediateResources Intermediate;
            if (!BuildIntermediateResources(Intermediate, InputMeshData, InputAssemblyData, Settings, bCanFreeInputMeshData))
            {
                return false;
            }
        在BuildIntermediateResources中：
        * 执行网格预处理（细分、位移映射）
        * 创建 Cluster DAG（有向无环图）数据结构
        * 进行网格简化（trimming）
        * 准备中间表示数据