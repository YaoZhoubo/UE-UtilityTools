

#include "Shaders/MyShader.h"

IMPLEMENT_SHADER_TYPE(, FMyShaderVS, TEXT("/UtilityTools/Shaders/MyShader.usf"), TEXT("MainVS"), SF_Vertex);
IMPLEMENT_SHADER_TYPE(, FMyShaderPS, TEXT("/UtilityTools/Shaders/MyShader.usf"), TEXT("MainPS"), SF_Pixel);