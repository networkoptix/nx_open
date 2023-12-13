#version 440

layout(location = 0) in vec4 position;
layout(location = 1) in vec2 texCoord;
layout(location = 2) in float texOpacity;

layout(location = 0) out vec2 vTexCoord;
layout(location = 1) out float vOpacity;

layout(std140, binding = 0) uniform buf
{
    mat4 mvp;
};

void main()
{
    vTexCoord = texCoord;
    vOpacity = texOpacity;
    gl_Position = mvp * position;
}
