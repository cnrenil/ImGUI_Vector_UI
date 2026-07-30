#version 450

layout(set = 0, binding = 0) uniform sampler2D sourceTexture;
layout(push_constant) uniform Constants { vec4 data[8]; } constants;
layout(location = 0) in vec2 inUv;
layout(location = 0) out vec4 outColor;

void main()
{
    vec2 texel = constants.data[0].xy;
    vec2 direction = constants.data[0].zw;
    float sigma = max(constants.data[1].x, 0.5);
    vec4 sum = vec4(0.0);
    float weightSum = 0.0;
    for (int index = -16; index <= 16; ++index)
    {
        float distance = float(index);
        float weight = exp(-0.5 * distance * distance / (sigma * sigma));
        sum += texture(sourceTexture, inUv + direction * texel * distance) * weight;
        weightSum += weight;
    }
    outColor = sum / max(weightSum, 0.0001);
}
