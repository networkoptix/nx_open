#version 440

layout(location = 0) in vec2 cellCoords;

layout(location = 0) out vec4 fragColor;

layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
    vec4 color;
    vec2 cellSize;
    vec2 lineSizeInCellCoords;
};

void main()
{
    vec2 thisCell = floor(cellCoords);
    vec2 prevCell = floor(cellCoords - lineSizeInCellCoords);

    if (thisCell == prevCell)
        discard;

    fragColor = color * qt_Opacity;
}
