cbuffer EffectConstants : register(b0) { float4 uData[8]; };

struct VertexOutput
{
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
};

float Hash21(float2 value)
{
    value = frac(value * float2(123.34, 456.21));
    value += dot(value, value + 45.32);
    return frac(value.x * value.y);
}

float LineMask(float2 coordinate)
{
    float2 width = max(fwidth(coordinate), 0.001);
    float2 distanceToLine = abs(frac(coordinate - 0.5) - 0.5) / width;
    return 1.0 - saturate(min(distanceToLine.x, distanceToLine.y));
}

float4 main(VertexOutput input) : SV_Target
{
    float3 topColor = uData[0].rgb;
    float3 bottomColor = uData[1].rgb;
    float3 accent = uData[2].rgb;
    float3 secondary = uData[3].rgb;
    float time = uData[4].x * uData[5].z;
    float gridSize = max(uData[4].y, 12.0);
    float gridStrength = saturate(uData[4].z);
    float vignetteStrength = saturate(uData[4].w);
    float2 resolution = max(uData[5].xy, 1.0);
    float intensity = max(uData[5].w, 0.0);

    float2 uv = input.uv;
    float2 centered = uv * 2.0 - 1.0;
    centered.x *= resolution.x / resolution.y;
    float3 color = lerp(topColor, bottomColor, smoothstep(0.0, 1.0, uv.y));

    float fineGrid = LineMask(input.position.xy / gridSize);
    float majorGrid = LineMask(input.position.xy / (gridSize * 5.0));
    color += secondary * (fineGrid * gridStrength * 0.28 + majorGrid * gridStrength * 0.68);

    float waveA = centered.y + 0.30 + sin(centered.x * 2.5 + time * 0.75) * 0.11;
    float waveB = centered.y - 0.20 + sin(centered.x * 4.1 - time * 0.58) * 0.075;
    color += accent * exp(-waveA * waveA * 105.0) * 0.58 * intensity;
    color += secondary * exp(-waveB * waveB * 155.0) * 0.48 * intensity;

    float2 beaconPosition = float2(sin(time * 0.42) * 0.72, cos(time * 0.31) * 0.34);
    float beacon = exp(-dot(centered - beaconPosition, centered - beaconPosition) * 11.0);
    float diagonal = exp(-pow(centered.y + centered.x * 0.34 - sin(time * 0.26) * 0.25, 2.0) * 75.0);
    color += accent * beacon * 0.52 * intensity;
    color += lerp(accent, secondary, uv.x) * diagonal * 0.20 * intensity;
    float vignette = 1.0 - smoothstep(0.30, 1.48, length(centered));
    color *= lerp(1.0, vignette, vignetteStrength);
    color += (Hash21(input.position.xy + floor(time * 12.0)) - 0.5) / 320.0;
    return float4(max(color, 0.0), 1.0);
}
