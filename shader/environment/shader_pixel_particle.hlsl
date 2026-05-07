Texture2D g_Texture : register(t0);
SamplerState g_Sampler : register(s0);

struct PS_IN
{
    float4 pos : SV_POSITION;
    float4 color : COLOR0;
    float2 uv : TEXCOORD0;
};

float4 main(PS_IN vi) : SV_TARGET
{
    float4 texColor = g_Texture.Sample(g_Sampler, vi.uv);
    // Treat dark pixels as transparent for particle sprites.
    float mask = max(texColor.r, max(texColor.g, texColor.b));
    clip(mask - 0.05f);

    float4 finalColor = vi.color;
    finalColor.rgb *= texColor.rgb;
    finalColor.a *= mask;

    return finalColor;
}
