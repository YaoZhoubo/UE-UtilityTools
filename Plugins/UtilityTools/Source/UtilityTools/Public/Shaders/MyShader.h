#pragma once

#include "GlobalShader.h"

class FMyShaderVS : public FGlobalShader
{
public:
	DECLARE_SHADER_TYPE(FMyShaderVS, Global);
	FMyShaderVS() {}
	FMyShaderVS(const ShaderMetaType::CompiledShaderInitializerType& Initializer) : FGlobalShader(Initializer) {}
};

class FMyShaderPS : public FGlobalShader
{
public:
	DECLARE_SHADER_TYPE(FMyShaderPS, Global);
	FMyShaderPS() {}
	FMyShaderPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer) : FGlobalShader(Initializer) {}
};