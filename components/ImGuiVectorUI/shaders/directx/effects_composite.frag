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
    float itemMode = uData[7].w;
    if (itemMode > 0.5)
    {
        float2 sampleUv = input.uv - uData[1].xy;
        float inside = all(sampleUv >= 0.0) && all(sampleUv <= 1.0) ? 1.0 : 0.0;
        float4 sampled = SourceTexture.Sample(LinearClamp, saturate(sampleUv)) * inside;
        float opacity = saturate(uData[1].w);
        if (uData[2].x > 1.5)
            return sampled * opacity;

        float alpha = sampled.a * saturate(uData[1].z) * opacity * uData[0].a;
        return float4(uData[0].rgb * alpha, alpha);
    }

    float2 pixelPosition = input.position.xy;
    float boundsMask = all(pixelPosition >= uData[2].xy) &&
                       all(pixelPosition <= uData[2].zw) ? 1.0 : 0.0;
    return float4(SourceTexture.Sample(LinearClamp, input.uv).rgb *
                  uData[1].y * boundsMask, 0.0);
}
