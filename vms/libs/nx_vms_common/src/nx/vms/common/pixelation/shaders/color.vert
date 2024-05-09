#version 440

layout(location = 0) in vec4 position;
layout(location = 0) out vec4 vColor;

layout(std140, binding = 0) uniform buf
{
    mat4 mvp;
    vec4 color;
};

void main()
{
    vColor = color;
    gl_Position = mvp * position;
}
