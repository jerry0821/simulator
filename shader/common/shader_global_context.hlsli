#ifndef SHADER_GLOBAL_CONTEXT_HLSLI
#define SHADER_GLOBAL_CONTEXT_HLSLI

// Shared naming contract for frame/object data. Existing shaders still bind
// legacy constant buffers, but new shaders should map into these shapes.
struct ShaderFrameContext
{
    float4x4 view;
    float4x4 projection;
    float4x4 viewProjection;
    float4x4 invView;
    float4x4 invProjection;
    float4x4 invViewProjection;
    float4 dirLightDirection;
    float4 dirLightColor;
    float4 cameraPosition;
    float4 cameraVector;
    float2 screenResolution;
    float2 invScreenResolution;
    float time;
    float deltaTime;
    float2 padding;
};

struct ShaderObjectContext
{
    float4x4 model;
    float4x4 invTransModel;
    uint objectID;
    float3 padding;
};

struct ShaderMaterialContext
{
    float3 baseColor;
    float metallic;
    float roughness;
    float specular;
    float ambientOcclusion;
    float alpha;
};

#endif // SHADER_GLOBAL_CONTEXT_HLSLI
