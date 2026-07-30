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
    float2 texel = uData[0].xy;
    float2 direction = uData[0].zw;
    float sigma = max(uData[1].x, 0.5);
    float4 sum = 0.0;
    float weightSum = 0.0;
    [unroll] for (int i = -16; i <= 16; ++i)
    {
        float distance = (float)i;
        float weight = exp(-0.5 * distance * distance / (sigma * sigma));
        sum += SourceTexture.Sample(LinearClamp, input.uv + direction * texel * distance) * weight;
        weightSum += weight;
    }
    return sum / max(weightSum, 0.0001);
}
