#version 440

layout(location = 0) in vec4 qt_Vertex;
layout(location = 1) in vec2 qt_MultiTexCoord0;

layout(location = 0) out vec2 cellCoords;

layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
    vec4 color;
    vec2 cellSize;
    vec2 lineSizeInCellCoords;
    vec2 cellCounts;
    int maskTextureValid;
};

void main()
{
    gl_Position = qt_Matrix * qt_Vertex;
    cellCoords = qt_Vertex.xy / cellSize;
}
