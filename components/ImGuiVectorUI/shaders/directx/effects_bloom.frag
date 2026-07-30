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
    float4 source = SourceTexture.Sample(LinearClamp, input.uv);
    float brightness = max(source.r, max(source.g, source.b));
    float minimum = min(source.r, min(source.g, source.b));
    float brightnessWeight = smoothstep(uData[1].z - 0.1, uData[1].z + 0.05, brightness);
    float saturationWeight = smoothstep(uData[1].w - 0.05, uData[1].w + 0.05,
                                        brightness - minimum);
    float2 pixelPosition = input.position.xy;
    float boundsMask = all(pixelPosition >= uData[2].xy) &&
                       all(pixelPosition <= uData[2].zw) ? 1.0 : 0.0;
    return source * brightnessWeight * saturationWeight * boundsMask;
}
