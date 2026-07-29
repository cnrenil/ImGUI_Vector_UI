#version 450

layout(set = 0, binding = 0) uniform sampler2D sourceTexture;
layout(push_constant) uniform Constants { vec4 data[8]; } constants;
layout(location = 0) in vec2 inUv;
layout(location = 0) out vec4 outColor;

float roundedRectSdf(vec2 point, vec2 rectMin, vec2 rectMax, float radius)
{
    vec2 center = (rectMin + rectMax) * 0.5;
    vec2 halfSize = max((rectMax - rectMin) * 0.5, vec2(0.0));
    radius = min(max(radius, 0.0), min(halfSize.x, halfSize.y));
    vec2 q = abs(point - center) - halfSize + radius;
    return length(max(q, vec2(0.0))) + min(max(q.x, q.y), 0.0) - radius;
}

float hash21(vec2 point)
{
    point = fract(point * vec2(123.34, 345.45));
    point += dot(point, point + 34.345);
    return fract(point.x * point.y);
}

float smoothNoise21(vec2 point)
{
    vec2 cell = floor(point);
    vec2 local = fract(point);
    vec2 curve = local * local * (3.0 - 2.0 * local);
    float a = hash21(cell);
    float b = hash21(cell + vec2(1.0, 0.0));
    float c = hash21(cell + vec2(0.0, 1.0));
    float d = hash21(cell + vec2(1.0, 1.0));
    return mix(mix(a, b, curve.x), mix(c, d, curve.x), curve.y);
}

void main()
{
    vec2 rectMin = constants.data[0].xy;
    vec2 rectMax = constants.data[0].zw;
    vec2 point = gl_FragCoord.xy;
    float radius = constants.data[1].x;
    float opacity = clamp(constants.data[1].y, 0.0, 1.0);
    float sdf = roundedRectSdf(point, rectMin, rectMax, radius);
    float mask = 1.0 - smoothstep(-0.5, 1.0, sdf);

    if (constants.data[7].w > 0.5)
    {
        vec4 sampled = texture(sourceTexture, inUv);
        float alpha = mask * opacity;
        if (alpha <= 0.001)
            discard;
        outColor = vec4(sampled.rgb * alpha, alpha);
        return;
    }

    vec3 tint = vec3(constants.data[1].z, constants.data[1].w, constants.data[2].x);
    float tintStrength = clamp(constants.data[2].y, 0.0, 1.0);
    vec3 accent = vec3(constants.data[2].z, constants.data[2].w, constants.data[3].x);
    float refraction = max(constants.data[3].y, 0.0);
    float edgeWidth = max(constants.data[3].z, 0.5);
    float edge = mask * (1.0 - smoothstep(0.0, edgeWidth, abs(sdf)));
    vec2 normal = vec2(
        roundedRectSdf(point + vec2(1.0, 0.0), rectMin, rectMax, radius) -
            roundedRectSdf(point - vec2(1.0, 0.0), rectMin, rectMax, radius),
        roundedRectSdf(point + vec2(0.0, 1.0), rectMin, rectMax, radius) -
            roundedRectSdf(point - vec2(0.0, 1.0), rectMin, rectMax, radius));
    normal = normalize(normal + vec2(0.0001));
    vec2 texel = constants.data[7].xy;
    float thickness = clamp(constants.data[4].x, 0.0, 1.0);
    float shapePower = max(constants.data[4].y, 2.0);
    float lensStrength = max(constants.data[4].z, 0.0);
    float lensBase = max(constants.data[4].w * 2.718281828, 1.0001);
    float lensFalloff = max(constants.data[5].x, 0.1);
    float lensBias = max(constants.data[5].y, 0.0);
    float lensPower = max(constants.data[5].z, 0.1);
    float noiseStrength = max(constants.data[5].w, 0.0);
    float time = constants.data[7].z;

    vec2 center = (rectMin + rectMax) * 0.5;
    vec2 halfSize = max((rectMax - rectMin) * 0.5, vec2(1.0));
    float lensRadius = max(min(halfSize.x, halfSize.y), 1.0);
    float edgeDistance = clamp(max(-sdf, 0.0) / lensRadius, 0.0, 1.0);
    float interiorDistance = pow(edgeDistance, 2.0 / shapePower);
    float lensCompression = clamp(
        lensStrength * pow(lensBase, -lensFalloff * interiorDistance - lensBias),
        0.0, 0.98);
    float lensScale = pow(max(1.0 - lensCompression, 0.02), lensPower);
    vec2 lensOffset = ((center + (point - center) * lensScale) - point) * texel;

    vec2 noisePosition = point * 0.045 + vec2(time * 0.35, -time * 0.23);
    vec2 surfaceNoise = vec2(smoothNoise21(noisePosition),
                             smoothNoise21(noisePosition + 19.19)) * 2.0 - 1.0;
    float rimRefraction = refraction * edge * (0.35 + thickness * 0.65);
    vec2 sampleUv = inUv + lensOffset + texel *
        (normal * rimRefraction + surfaceNoise * noiseStrength * (0.25 + lensCompression * 0.75));
    sampleUv = clamp(sampleUv, texel * 0.5, vec2(1.0) - texel * 0.5);
    vec3 material = mix(texture(sourceTexture, sampleUv).rgb, tint,
                        tintStrength * (0.7 + thickness * 0.3));
    float lightFacing = clamp(dot(normal, normalize(vec2(-0.45, -0.90))), 0.0, 1.0);
    vec3 rimColor = mix(accent, vec3(1.0), 0.55);
    material += rimColor * edge * (0.22 + lightFacing * 0.78) *
                clamp(constants.data[3].w, 0.0, 1.0);

    float outside = max(sdf, 0.0);
    float glow = exp(-outside / max(constants.data[6].y, 0.5)) *
                 clamp(constants.data[6].x, 0.0, 1.0) * (1.0 - mask);
    float shadow = exp(-outside / max(constants.data[6].w, 0.5)) *
                   clamp(constants.data[6].z, 0.0, 1.0) * (1.0 - mask);
    float materialAlpha = mask * opacity;
    float alpha = clamp(materialAlpha + glow + shadow, 0.0, 1.0);
    if (alpha <= 0.001)
        discard;
    outColor = vec4(material * materialAlpha + accent * glow, alpha);
}
