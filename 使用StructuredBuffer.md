# 使用StructuredBuffer
StructuredBuffer是一种包含结构化数据的GPU缓冲区资源类型，所有元素具有相同的内存布局，可以将其类比于C++中的结构体数组
## 特性
* 支持随机访问：通过索引直接访问任意元素
* 线程安全：多线程可以同时读取，适合并行处理
* 通过RDG（Render Dependency Graph）管理，自动处理资源屏障和同步
* 高效的数据组织，提高缓存命中率，尤其在按顺序访问时
## 使用流程
1. 定义数据结构（C++端）

        // 必须与HLSL中的结构体内存布局完全匹配
        struct FMyData 
        {
            FVector4f Param1;   // 对应HLSL的float4

            int32 Param2;       // 4字节
            float Param3;       // 4字节
            float Pad[2];       // 填充保证16字节对齐
        };
2. 着色器参数声明（C++端）

        BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
            // 用于只读
            SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<FMyData>, MyReadOnlyData)
            // 用于读写
            SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<FMyData>, MyReadWriteData)
        END_SHADER_PARAMETER_STRUCT()
3. 创建并上传数据（C++端）

    数据上传必须在RenderingThread中通过RDG进行，生命周期在当前帧的Graph执行期间有效，不要尝试在帧间持有其指针。

            // 准备数据
            TArray<FMyData> MyDataArray;
            for(int32 i = 0; i < 100; ++i)
            {
                FMyData MyData;
                MyData.Param1 = FVector4f(/*...*/);
                MyData.Param2 = 1;
                MyData.Param3 = 2.0f;
                MyDataArray.Add(MyData);
            }
            
            // 创建RDG缓冲区描述
            FRDGBufferDesc Desc = FRDGBufferDesc::CreateStructuredDesc(
                sizeof(FMyData),     // 每个元素的大小
                MyDataArray.Num()    // 元素数量
            );
            
            // 创建缓冲区
            FRDGBuffer* MyDataArrayBuffer = GraphBuilder.CreateBuffer(Desc, TEXT("MyDataArrayBuffer"));
            
            // 上传数据到GPU
            GraphBuilder.QueueBufferUpload(
                MyDataArrayBuffer, 
                MyDataArray.GetData(), 
                MyDataArray.Num() * sizeof(FMyData)
            );
            
            // 创建SRV并绑定到着色器参数
            PassParameters->MyReadOnlyData = GraphBuilder.CreateSRV(MyDataArrayBuffer);
4. HLSL着色器声明

        // HLSL中的对应结构体
        struct FMyData
        {
            float4 Param1;  // 对应HLSL的float4

            int Param2;     // 4字节
            float Param3;   // 4字节
            float Pad[2];   // 填充保证16字节对齐
        };

        // 声明为SRV（只读）
        StructuredBuffer<FMyData> MyReadOnlyData;
        // 声明为UAV（读写）
        RWStructuredBuffer<FMyData> MyReadWriteData;
5. HLSL中使用

        void MainPixelShader(in float4 SV_Position : SV_Position, out float4 OutColor : SV_Target0)
        {
            // 通过索引访问数据
            uint index = 0;
            float4 Param1 = MyReadOnlyData[index].Param1;
            int Param2 = MyReadOnlyData[index].Param2;
            float Param3 = MyReadOnlyData[index].Param3;
        }
## 注意事项
1. 内存对齐问题

    不同编译器例如MSVC、Clang/LLVM的对齐规则可能有细微差别，所以显式填充是最可靠、最易移植的方法。

        // 错误
        struct FBadAlignment
        {
           FVector4f Param1;  // 16字节
           float Param2;      // 4字节 → 总共20字节，UE可能按16字节对齐为32字节，导致创建缓冲区时不正确
        };

        // 正确
        struct FMyData 
        {
            FVector4f Param1;   // 16字节
            float Param2;       // 4字节
            float Opacity[3];   // 填充 → 总共32字节，16字节对齐
        };
2. 数据竞争
        
    当使用RWStructuredBuffer（UAV）时，如果多个线程写入同一个索引位置，会产生未定义行为，必须使用同步原语，如HLSL中的Interlocked函数（原子操作）或GroupMemoryBarrierWithGroupSync来确保写入顺序。
## 常见问题
### 数据不匹配
HLSL中读取的数据与C++上传的数据不一致
* 解决：检查结构体内存布局、字节对齐、数据上传大小
### 性能瓶颈
着色器执行缓慢
* 解决：优化数据布局、采用池化缓冲区
### 越界访问
图形驱动崩溃或渲染异常
* 解决：添加边界检查、验证索引有效性