Texture2D SourceTexture : register(t0);
SamplerState LinearClamp : register(s0);
cbuffer EffectConstants : register(b0) { float4 uData[8]; };

struct VertexOutput
{
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
};

float4 main(VertexOutput input) : SV_Target
{
    float2 pixelPosition = input.position.xy;
    float boundsMask = all(pixelPosition >= uData[2].xy) &&
                       all(pixelPosition <= uData[2].zw) ? 1.0 : 0.0;
    return float4(SourceTexture.Sample(LinearClamp, input.uv).rgb *
                  uData[1].y * boundsMask, 0.0);
}
