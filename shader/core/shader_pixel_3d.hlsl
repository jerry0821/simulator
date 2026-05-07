/*==============================================================================

   3D描画用ピクセルシェーダー [shader_pixel_3d.hlsl]
														 Author : Youhei Sato
														 Date   : 2025/05/15
--------------------------------------------------------------------------------

==============================================================================*/

cbuffer PS_CONSTANT_BUFFER : register(b0)
{
    float4 diffuse_color;
};

cbuffer PS_CONSTANT_BUFFER : register(b1)
{
    float4 ambient_color;
};

cbuffer PS_CONSTANT_BUFFER : register(b2)
{
    float4 directional_world_vector;
    float4 directional_color;
}

cbuffer PS_CONSTANT_BUFFER : register(b3)
{
    float3 eye_posW;
    float  specular_power;
    float4 specular_color;
};

//cbuffer PS_CONSTANT_BUFFER : register(b4)
//{
//    float3 pointlight_posW; // 光源のワールド座標
//    float pointlight_range; // 光源の届く最大範囲
//    float4 pointlight_color;
//};

struct PointLight
{
    float3 posW;
    float range;
    float4 color;
};

cbuffer PS_CONSTANT_BUFFER : register(b4)
{
    PointLight pointlight[4];
    int pointlight_count;
    float3 pointlight_dummy;
};

struct PS_INPUT
{
    float4 posH : SV_POSITION;
    float4 posW : POSITION0;
    float4 normalW : NORMAL0;
    float4 color : COLOR0;
    float2 uv : TEXCOORD0;
};

Texture2D tex; // テクスチャ
SamplerState samp; // テクスチャさんプラ

float4 main(PS_INPUT ps_in) : SV_TARGET
{
    float3 material_color = tex.Sample(samp, ps_in.uv).rgb * ps_in.color.rgb * diffuse_color.rgb;
    
    // 並行光源(diffuse light)
    float4 normalW = normalize(ps_in.normalW);
    float3 lightVec = normalize(-directional_world_vector.xyz);
    //float dl = max(0, dot(lightVec, normalW.xyz)); // 通常の拡散光計算
    float dl = (dot(lightVec, normalW.xyz) + 1.0f) * 0.5f; // 半分の明るさを加算(環境光の代わり)
    float3 diffuse = material_color * directional_color.rgb * dl;
    
    // 環境光(ambient light)
    float3 ambient = ambient_color.rgb * material_color;
    
    // スペキュラ計算
    float3 toEye = normalize(eye_posW - ps_in.posW.xyz);
    float3 r = reflect(-lightVec, normalW.xyz).xyz;
    float t = pow(max(dot(r, toEye), 0.0f), specular_power); //反射大小
    float3 specular = specular_color.rgb * t;
    
    float alpha =  tex.Sample(samp, ps_in.uv).a * ps_in.color.a * diffuse_color.a;
    float3 final_color = ambient + diffuse + specular;
    
    // リムライト計算
    float rim =1.0f - max(dot(normalW.xyz, toEye), 0.0f); // rim light 輪廓光
    rim = pow(rim, 2.0f); // 強調
    //final_color += float3(rim, rim, rim);
    
    // ポイントライト計算
    for (int i = 0; i < pointlight_count;i++)
    {
        // 面と光源の距離
        float3 lightToPixel = ps_in.posW.xyz - pointlight[i].posW;
        float D = length(lightToPixel);
        // 距離減衰
        float A = pow(max(1.0f - 1.0f * D / pointlight[i].range, 0.0f), 2.0f);
        
        // 点と面の角度 
        float dl = max(0.0f, dot(-D, normalW.xyz));
        // float dl = (dot(lightVec, normalW.xyz) + 1.0f) * 0.5f; // 半分の明るさを加算(環境光の代わり)
                
        final_color += material_color * pointlight[i].color.rgb * A * dl;
        
        // 点光源のスペキュラ計算
        float3 r_point = reflect(normalize(lightToPixel), normalW.xyz).xyz;
        float t_point = pow(max(dot(r_point, toEye), 0.0f), specular_power); //反射大小
        
        final_color += pointlight[i].color.rgb * t_point;
    }
    return float4(final_color, alpha);
}

