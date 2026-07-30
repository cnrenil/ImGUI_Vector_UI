Texture2D SourceTexture : register(t0);
SamplerState LinearClamp : register(s0);
cbuffer EffectConstants : register(b0) { float4 uData[8]; };

struct VertexOutput
{
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
};

float RoundedRectSdf(float2 pixelPosition, float2 rectMin, float2 rectMax, float radius)
{
    float2 center = (rectMin + rectMax) * 0.5;
    float2 halfSize = max((rectMax - rectMin) * 0.5, 0.0);
    radius = min(max(radius, 0.0), min(halfSize.x, halfSize.y));
    float2 q = abs(pixelPosition - center) - halfSize + radius;
    return length(max(q, 0.0)) + min(max(q.x, q.y), 0.0) - radius;
}

float4 main(VertexOutput input) : SV_Target
{
    float2 pixelPosition = input.position.xy;
    float sdf = RoundedRectSdf(pixelPosition, uData[0].xy, uData[0].zw, uData[1].x);
    float mask = 1.0 - smoothstep(-0.5, 1.0, sdf);
    clip(mask - 0.001);
    float opacity = saturate(uData[1].y) * mask;
    float4 source = SourceTexture.Sample(LinearClamp, input.uv);
    return float4(source.rgb * opacity, opacity);
}
