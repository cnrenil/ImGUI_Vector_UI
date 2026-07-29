#version 450

layout(set = 0, binding = 0) uniform sampler2D sourceTexture;
layout(push_constant) uniform Constants { vec4 data[8]; } constants;
layout(location = 0) in vec2 inUv;
layout(location = 0) out vec4 outColor;

void main()
{
    vec2 point = gl_FragCoord.xy;
    float bounds = all(greaterThanEqual(point, constants.data[2].xy)) &&
                   all(lessThanEqual(point, constants.data[2].zw)) ? 1.0 : 0.0;
    vec3 color = texture(sourceTexture, inUv).rgb;
    outColor = vec4(color * constants.data[1].y * bounds, 0.0);
}
