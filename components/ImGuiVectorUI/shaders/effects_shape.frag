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
    vec2 offset = constants.data[3].xy;
    float sdf = roundedRectSdf(gl_FragCoord.xy, constants.data[0].xy + offset,
                               constants.data[0].zw + offset, constants.data[1].x);
    float sigma = max(constants.data[1].z, 0.5);
    float alpha = exp(-max(sdf, 0.0) * max(sdf, 0.0) / (2.0 * sigma * sigma)) *
                  clamp(constants.data[1].y, 0.0, 1.0);
    if (alpha <= 0.001)
        discard;
    outColor = vec4(constants.data[2].rgb * alpha, alpha);
}
