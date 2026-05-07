struct VS_INPUT
{
    float4 posL : POSITION;
    float2 uv : TEXCOORD;
};

struct PS_INPUT
{
    float4 posH : SV_POSITION;
    float2 uv : TEXCOORD;
};

PS_INPUT main(VS_INPUT input_vertex)
{
    PS_INPUT output_vertex;
    output_vertex.posH = input_vertex.posL;
    output_vertex.uv = input_vertex.uv;
    return output_vertex;
}
