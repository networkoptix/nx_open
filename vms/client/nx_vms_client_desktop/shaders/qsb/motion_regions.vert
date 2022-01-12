#version 440

layout(location = 0) in vec4 qt_Vertex;
layout(location = 1) in vec2 qt_MultiTexCoord0;

layout(location = 0) out vec2 vTexCoord;

// uniform block: 96 bytes
layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix; // offset 0
    float qt_Opacity; // offset 64
    float fillOpacity; // offset 68
    vec2 gridStep; // offset 72
    vec4 borderColor; // offset 80
} ubuf;

void main()
{
    gl_Position = ubuf.qt_Matrix * qt_Vertex;
    vTexCoord = qt_MultiTexCoord0;
}
