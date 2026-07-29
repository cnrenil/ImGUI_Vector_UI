#version 450

layout(set = 0, binding = 0) uniform sampler2D sourceTexture;
layout(push_constant) uniform Constants { vec4 data[8]; } constants;
layout(location = 0) in vec2 inUv;
layout(location = 0) out vec4 outColor;

float lineMask(vec2 coordinate)
{
    vec2 width = max(fwidth(coordinate), vec2(0.001));
    vec2 distanceToLine = abs(fract(coordinate - 0.5) - 0.5) / width;
    return 1.0 - clamp(min(distanceToLine.x, distanceToLine.y), 0.0, 1.0);
}

void main()
{
    vec3 topColor = constants.data[0].rgb;
    vec3 bottomColor = constants.data[1].rgb;
    vec3 accent = constants.data[2].rgb;
    vec3 secondary = constants.data[3].rgb;
    float time = constants.data[4].x * constants.data[5].z;
    float gridSize = max(constants.data[4].y, 12.0);
    float gridStrength = clamp(constants.data[4].z, 0.0, 1.0);
    float vignetteStrength = clamp(constants.data[4].w, 0.0, 1.0);
    vec2 resolution = max(constants.data[5].xy, vec2(1.0));
    float intensity = max(constants.data[5].w, 0.0);

    vec2 uv = inUv;
    vec2 centered = uv * 2.0 - 1.0;
    centered.x *= resolution.x / resolution.y;
    vec3 color = mix(topColor, bottomColor, smoothstep(0.0, 1.0, uv.y));

    vec2 pixels = vec2(gl_FragCoord.x, resolution.y - gl_FragCoord.y);
    float fineGrid = lineMask(pixels / gridSize);
    float majorGrid = lineMask(pixels / (gridSize * 5.0));
    color += secondary * (fineGrid * gridStrength * 0.28 +
                          majorGrid * gridStrength * 0.68);

    float waveA = centered.y + 0.30 + sin(centered.x * 2.5 + time * 0.75) * 0.11;
    float waveB = centered.y - 0.20 + sin(centered.x * 4.1 - time * 0.58) * 0.075;
    float ridgeA = exp(-waveA * waveA * 105.0);
    float ridgeB = exp(-waveB * waveB * 155.0);
    color += accent * ridgeA * 0.58 * intensity;
    color += secondary * ridgeB * 0.48 * intensity;

    vec2 beaconPosition = vec2(sin(time * 0.42) * 0.72, cos(time * 0.31) * 0.34);
    float beacon = exp(-dot(centered - beaconPosition, centered - beaconPosition) * 11.0);
    float diagonal = exp(-pow(centered.y + centered.x * 0.34 - sin(time * 0.26) * 0.25, 2.0) * 75.0);
    color += accent * beacon * 0.52 * intensity;
    color += mix(accent, secondary, uv.x) * diagonal * 0.20 * intensity;

    float vignette = 1.0 - smoothstep(0.30, 1.48, length(centered));
    color *= mix(1.0, vignette, vignetteStrength);
    outColor = vec4(max(color, vec3(0.0)), 1.0);
}
