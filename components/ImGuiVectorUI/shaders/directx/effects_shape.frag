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
    float2 rectMin = uData[0].xy;
    float2 rectMax = uData[0].zw;
    float radius = uData[1].x;
    float strength = saturate(uData[1].y);
    float sigma = max(uData[1].z, 0.5);
    float3 color = uData[2].rgb;
    float2 offset = uData[3].xy;
    float sdf = RoundedRectSdf(input.position.xy, rectMin + offset, rectMax + offset, radius);
    float outside = max(sdf, 0.0);
    float outsideMask = smoothstep(-0.5, 0.5, sdf);
    float alpha = exp(-outside * outside / (2.0 * sigma * sigma)) *
                  outsideMask * strength * uData[2].a;
    clip(alpha - 0.001);
    return float4(color * alpha, alpha);
}
