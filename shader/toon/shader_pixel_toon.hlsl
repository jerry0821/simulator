Texture2D MainTex : register(t0);
SamplerState Sampler : register(s0);

cbuffer CB_PS : register(b0)
{
    float4 materialColor;
    float4 outlineColor;
    int stepCount;
    float3 padding_ps; 
};

cbuffer CB_AMBIENT : register(b1)
{
    float4 ambient_color;
};

cbuffer CB_DIRECTIONAL : register(b2)
{
    float4 directional_world_vector;
    float4 directional_color;
};

cbuffer CB_SPECULAR : register(b3)
{
    float3 eye_posW;
    float specular_power;
    float4 specular_color;
};

struct PS_INPUT
{
    float4 posH : SV_POSITION;
    float4 posW : POSITION;
    float4 normal : NORMAL;
    float4 color : COLOR;
    float2 uv : TEXCOORD;
};

float4 main(PS_INPUT input) : SV_TARGET
{
    float4 texColor = MainTex.Sample(Sampler, input.uv);
    float3 n = normalize(input.normal.xyz);
    float3 l = normalize(-directional_world_vector.xyz);

    float NdotL = dot(n, l);
    float threshold = 0.0;
    float intensity = floor((NdotL + threshold) * stepCount) / max(stepCount - 1, 1);
    intensity = clamp(intensity, 0.25, 1.0);

    //float3 v = normalize(float3(0.0, 0.5, 1.0));
    //float3 h = normalize(l + v);
    //float NdotH = max(dot(n, l), 0.0);
    
    float3 viewDir = normalize(eye_posW - input.posW.xyz); 

    float3 halfVector = normalize(l + viewDir);

    float NdotH = max(dot(n, halfVector), 0.0);

    // specåvéZ
    float specFactor = pow(NdotH, max(1.0, specular_power));
    float toonSpec = smoothstep(0.5, 0.52, specFactor);

    float3 diffuse = texColor.rgb * materialColor.rgb * directional_color.rgb * intensity;
    float3 ambient = texColor.rgb * ambient_color.rgb;
    float3 specular = specular_color.rgb * toonSpec;
    float specularIntensity = input.color.g == 0 ? 0 : pow(NdotH, input.color.g * 500);

    float3 finalColor = diffuse + ambient + specularIntensity;
    return float4(finalColor, texColor.a);
}