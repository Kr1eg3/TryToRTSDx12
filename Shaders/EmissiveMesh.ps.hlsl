cbuffer MaterialConstants : register(b3)
{
    float3 BaseColor;
    float Metallic;
    float Roughness;
    float3 Padding3;
}

struct VertexOutput
{
    float4 Position : SV_POSITION;
    float3 WorldPosition : POSITION1;
    float3 Normal : NORMAL;
    float2 TexCoord : TEXCOORD0;
    float3 ViewDirection : TEXCOORD1;
};

float4 PSMain(VertexOutput input) : SV_TARGET
{
    // Emissive material - just return the base color without any lighting calculations
    // This makes the object glow with its own light
    return float4(BaseColor, 1.0f);
}