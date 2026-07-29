#version 450

layout(location = 0) out vec2 outUv;

void main()
{
    vec2 position = vec2(gl_VertexIndex == 2 ? 3.0 : -1.0,
                         gl_VertexIndex == 1 ? 3.0 : -1.0);
    gl_Position = vec4(position, 0.0, 1.0);
    outUv = (position + 1.0) * 0.5;
}
