cbuffer LightConstants : register(b2)
{
    float3 LightPosition;
    float LightIntensity;
    float3 LightColor;
    float Padding2;
}

Texture2D DiffuseTexture : register(t0);
SamplerState DiffuseSampler : register(s0);

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
    // Sample the diffuse texture
    float3 textureColor = DiffuseTexture.Sample(DiffuseSampler, input.TexCoord).rgb;
    
    float3 normal = normalize(input.Normal);
    float3 lightDir = normalize(LightPosition - input.WorldPosition);
    float distance = length(LightPosition - input.WorldPosition);
    float attenuation = 1.0f / (1.0f + 0.1f * distance + 0.01f * distance * distance);
    float NdotL = max(0.0f, dot(normal, lightDir));

    float3 ambient = float3(0.1f, 0.1f, 0.1f);
    float3 diffuse = LightColor * LightIntensity * NdotL * attenuation;

    float3 viewDir = normalize(input.ViewDirection);
    float3 halfVector = normalize(lightDir + viewDir);
    float NdotH = max(0.0f, dot(normal, halfVector));
    float3 specular = LightColor * pow(NdotH, 32.0f) * 0.3f * attenuation;

    // Use texture color as base color
    float3 baseColor = textureColor;
    float3 finalColor = baseColor * (ambient + diffuse) + specular;

    return float4(finalColor, 1.0f);
}