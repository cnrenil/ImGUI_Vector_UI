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

float Hash21(float2 value)
{
    value = frac(value * float2(123.34, 456.21));
    value += dot(value, value + 45.32);
    return frac(value.x * value.y);
}

float SmoothNoise21(float2 value)
{
    float2 cell = floor(value);
    float2 local = frac(value);
    float2 curve = local * local * (3.0 - 2.0 * local);
    float a = Hash21(cell);
    float b = Hash21(cell + float2(1.0, 0.0));
    float c = Hash21(cell + float2(0.0, 1.0));
    float d = Hash21(cell + float2(1.0, 1.0));
    return lerp(lerp(a, b, curve.x), lerp(c, d, curve.x), curve.y);
}

float4 main(VertexOutput input) : SV_Target
{
    float2 rectMin = uData[0].xy;
    float2 rectMax = uData[0].zw;
    float radius = uData[1].x;
    float opacity = saturate(uData[1].y);
    float3 tint = float3(uData[1].z, uData[1].w, uData[2].x);
    float tintStrength = saturate(uData[2].y);
    float3 accent = float3(uData[2].z, uData[2].w, uData[3].x);
    float refraction = uData[3].y;
    float edgeWidth = max(uData[3].z, 0.5);
    float edgeHighlight = saturate(uData[3].w);
    float thickness = saturate(uData[4].x);
    float shapePower = max(uData[4].y, 2.0);
    float lensStrength = max(uData[4].z, 0.0);
    float lensBase = max(uData[4].w * 2.718281828, 1.0001);
    float lensFalloff = max(uData[5].x, 0.1);
    float lensBias = max(uData[5].y, 0.0);
    float lensPower = max(uData[5].z, 0.1);
    float noiseStrength = uData[5].w;
    float glowStrength = saturate(uData[6].x);
    float glowSpread = max(uData[6].y, 0.5);
    float shadowStrength = saturate(uData[6].z);
    float shadowSpread = max(uData[6].w, 0.5);
    float2 texel = uData[7].xy;
    float time = uData[7].z;

    float sdf = RoundedRectSdf(input.position.xy, rectMin, rectMax, radius);
    float mask = 1.0 - smoothstep(-0.5, 1.0, sdf);
    float edge = mask * (1.0 - smoothstep(0.0, edgeWidth, abs(sdf)));
    float epsilon = 1.0;
    float2 normal = float2(
        RoundedRectSdf(input.position.xy + float2(epsilon, 0.0), rectMin, rectMax, radius) -
            RoundedRectSdf(input.position.xy - float2(epsilon, 0.0), rectMin, rectMax, radius),
        RoundedRectSdf(input.position.xy + float2(0.0, epsilon), rectMin, rectMax, radius) -
            RoundedRectSdf(input.position.xy - float2(0.0, epsilon), rectMin, rectMax, radius));
    normal = normalize(normal + 0.0001);

    float2 center = (rectMin + rectMax) * 0.5;
    float2 halfSize = max((rectMax - rectMin) * 0.5, 1.0);
    float lensRadius = max(min(halfSize.x, halfSize.y), 1.0);
    float edgeDistance = saturate(max(-sdf, 0.0) / lensRadius);
    float interiorDistance = pow(edgeDistance, 2.0 / shapePower);
    float lensCompression = saturate(
        lensStrength * pow(lensBase, -lensFalloff * interiorDistance - lensBias));
    lensCompression = min(lensCompression, 0.98);
    float lensScale = pow(max(1.0 - lensCompression, 0.02), lensPower);
    float2 lensOffset = ((center + (input.position.xy - center) * lensScale) -
                         input.position.xy) * texel;

    float2 noisePosition = input.position.xy * 0.045 + float2(time * 0.35, -time * 0.23);
    float2 surfaceNoise = float2(SmoothNoise21(noisePosition),
                                 SmoothNoise21(noisePosition + 19.19)) * 2.0 - 1.0;
    float rimRefraction = refraction * edge * (0.35 + thickness * 0.65);
    float2 sampleUv = input.uv + lensOffset + texel *
        (normal * rimRefraction + surfaceNoise * noiseStrength * (0.25 + lensCompression * 0.75));
    sampleUv = clamp(sampleUv, texel * 0.5, 1.0 - texel * 0.5);
    float3 backdrop = SourceTexture.Sample(LinearClamp, sampleUv).rgb;
    float3 material = lerp(backdrop, tint, tintStrength * (0.7 + thickness * 0.3));
    float lightFacing = saturate(dot(normal, normalize(float2(-0.45, -0.90))));
    float3 rimColor = lerp(accent, 1.0, 0.55);
    material += rimColor * edge * (0.22 + lightFacing * 0.78) * edgeHighlight;

    float outside = max(sdf, 0.0);
    float shadow = exp(-outside / shadowSpread) * shadowStrength * (1.0 - mask);
    float glow = exp(-outside / glowSpread) * glowStrength * (1.0 - mask);
    float materialAlpha = mask * opacity;
    float alpha = saturate(materialAlpha + shadow + glow);
    clip(alpha - 0.001);
    float3 color = material * materialAlpha + accent * glow;
    return float4(color, alpha);
}
