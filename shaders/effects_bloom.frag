#version 450

layout(set = 0, binding = 0) uniform sampler2D sourceTexture;
layout(push_constant) uniform Constants { vec4 data[8]; } constants;
layout(location = 0) in vec2 inUv;
layout(location = 0) out vec4 outColor;

void main()
{
    vec4 color = texture(sourceTexture, inUv);
    float high = max(color.r, max(color.g, color.b));
    float low = min(color.r, min(color.g, color.b));
    float brightness = smoothstep(constants.data[1].z - 0.1,
                                  constants.data[1].z + 0.05, high);
    float saturation = smoothstep(constants.data[1].w - 0.05,
                                  constants.data[1].w + 0.05, high - low);
    vec2 point = gl_FragCoord.xy;
    float bounds = all(greaterThanEqual(point, constants.data[2].xy)) &&
                   all(lessThanEqual(point, constants.data[2].zw)) ? 1.0 : 0.0;
    outColor = vec4(color.rgb * brightness * saturation * bounds * constants.data[1].y, 0.0);
}
