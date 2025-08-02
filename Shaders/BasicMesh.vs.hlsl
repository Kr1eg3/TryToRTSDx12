cbuffer ModelConstants : register(b0)
{
    matrix ModelMatrix;
    matrix NormalMatrix;
}

cbuffer ViewConstants : register(b1)
{
    matrix ViewMatrix;
    matrix ProjectionMatrix;
    matrix ViewProjectionMatrix;
    float3 CameraPosition;
    float Padding1;
}

struct VertexInput
{
    float3 Position : POSITION;
    float3 Normal : NORMAL;
    float2 TexCoord : TEXCOORD0;
};

struct VertexOutput
{
    float4 Position : SV_POSITION;
    float3 WorldPosition : POSITION1;
    float3 Normal : NORMAL;
    float2 TexCoord : TEXCOORD0;
    float3 ViewDirection : TEXCOORD1;
};

VertexOutput VSMain(VertexInput input)
{
    VertexOutput output;

    float4 worldPosition = mul(float4(input.Position, 1.0f), ModelMatrix);
    output.WorldPosition = worldPosition.xyz;
    output.Position = mul(worldPosition, ViewProjectionMatrix);

    // Transform normal using the normal matrix (which should be inverse transpose of model matrix)
    output.Normal = normalize(mul(float4(input.Normal, 0.0f), NormalMatrix).xyz);
    output.TexCoord = input.TexCoord;
    output.ViewDirection = normalize(CameraPosition - worldPosition.xyz);

    return output;
}