#version 440

layout(location = 0) in vec4 position;
layout(location = 0) out vec2 vTexCoord;

layout(std140, binding = 0) uniform buf
{
    mat4 mvp;
};

void main()
{
    vTexCoord = position.xy;
    gl_Position = mvp * position;
}
