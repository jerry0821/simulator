Texture2D diffuseTexture : register(t0);
SamplerState samplerState : register(s0);

struct PS_IN
{
    float4 posH : SV_POSITION;
    float2 uv : TEXCOORD0;
};

float4 main(PS_IN input) : SV_TARGET
{
    float4 color = diffuseTexture.Sample(samplerState, input.uv);
    clip(color.a - 0.04f);
    return float4(0.0f, 0.0f, 0.0f, 1.0f);
}
