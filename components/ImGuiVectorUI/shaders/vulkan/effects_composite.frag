#version 450

layout(set = 0, binding = 0) uniform sampler2D sourceTexture;
layout(push_constant) uniform Constants { vec4 data[8]; } constants;
layout(location = 0) in vec2 inUv;
layout(location = 0) out vec4 outColor;

void main()
{
    float itemMode = constants.data[7].w;
    if (itemMode > 0.5)
    {
        vec2 sampleUv = inUv - constants.data[1].xy;
        float inside = all(greaterThanEqual(sampleUv, vec2(0.0))) &&
                       all(lessThanEqual(sampleUv, vec2(1.0))) ? 1.0 : 0.0;
        vec4 sampled = texture(sourceTexture, clamp(sampleUv, vec2(0.0), vec2(1.0))) * inside;
        float opacity = clamp(constants.data[1].w, 0.0, 1.0);
        if (constants.data[2].x > 1.5)
            outColor = sampled * opacity;
        else
        {
            float alpha = sampled.a * clamp(constants.data[1].z, 0.0, 1.0) *
                          opacity * constants.data[0].a;
            outColor = vec4(constants.data[0].rgb * alpha, alpha);
        }
        return;
    }

    vec2 point = gl_FragCoord.xy;
    float bounds = all(greaterThanEqual(point, constants.data[2].xy)) &&
                   all(lessThanEqual(point, constants.data[2].zw)) ? 1.0 : 0.0;
    vec3 color = texture(sourceTexture, inUv).rgb;
    outColor = vec4(color * constants.data[1].y * bounds, 0.0);
}
