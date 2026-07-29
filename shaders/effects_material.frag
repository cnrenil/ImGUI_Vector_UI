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

void main()
{
    vec2 rectMin = constants.data[0].xy;
    vec2 rectMax = constants.data[0].zw;
    vec2 point = gl_FragCoord.xy;
    float radius = constants.data[1].x;
    float opacity = clamp(constants.data[1].y, 0.0, 1.0);
    float sdf = roundedRectSdf(point, rectMin, rectMax, radius);
    float mask = 1.0 - smoothstep(-0.5, 1.0, sdf);
    if (mask <= 0.001)
        discard;

    if (constants.data[7].w > 0.5)
    {
        vec4 sampled = texture(sourceTexture, inUv);
        outColor = vec4(sampled.rgb, 1.0);
        return;
    }

    vec3 tint = vec3(constants.data[1].z, constants.data[1].w, constants.data[2].x);
    float tintStrength = clamp(constants.data[2].y, 0.0, 1.0);
    vec3 accent = vec3(constants.data[2].z, constants.data[2].w, constants.data[3].x);
    float edgeWidth = max(constants.data[3].z, 0.5);
    float edge = mask * (1.0 - smoothstep(0.0, edgeWidth, abs(sdf)));
    vec2 normal = vec2(
        roundedRectSdf(point + vec2(1.0, 0.0), rectMin, rectMax, radius) -
            roundedRectSdf(point - vec2(1.0, 0.0), rectMin, rectMax, radius),
        roundedRectSdf(point + vec2(0.0, 1.0), rectMin, rectMax, radius) -
            roundedRectSdf(point - vec2(0.0, 1.0), rectMin, rectMax, radius));
    normal = normalize(normal + vec2(0.0001));
    vec2 texel = constants.data[7].xy;
    vec2 sampleUv = clamp(inUv + texel * normal * constants.data[3].y * edge,
                          texel * 0.5, vec2(1.0) - texel * 0.5);
    vec3 material = mix(texture(sourceTexture, sampleUv).rgb, tint, tintStrength);
    material += accent * edge * clamp(constants.data[3].w, 0.0, 1.0);

    float outside = max(sdf, 0.0);
    float glow = exp(-outside / max(constants.data[6].y, 0.5)) *
                 clamp(constants.data[6].x, 0.0, 1.0) * (1.0 - mask);
    float shadow = exp(-outside / max(constants.data[6].w, 0.5)) *
                   clamp(constants.data[6].z, 0.0, 1.0) * (1.0 - mask);
    float materialAlpha = mask * opacity;
    float alpha = clamp(materialAlpha + glow + shadow, 0.0, 1.0);
    outColor = vec4(material * materialAlpha + accent * glow, alpha);
}
