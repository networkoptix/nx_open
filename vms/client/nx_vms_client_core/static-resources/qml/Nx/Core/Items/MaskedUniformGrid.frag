#version 440

layout(location = 0) in vec2 cellCoords;

layout(location = 0) out vec4 fragColor;

layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
    vec4 color;
    vec2 cellSize;
    vec2 lineSizeInCellCoords;
    vec2 cellCounts;
    int maskTextureValid;
};

layout(binding = 1) uniform sampler2D maskTextureProvider;

bool isDiscarded(vec2 cell)
{
    return maskTextureValid != 0
        ? texture(maskTextureProvider, (cell + vec2(0.5)) / cellCounts).a < 0.5
        : true;
}

void main()
{
    vec2 thisCell = floor(cellCoords);
    vec2 prevCell = floor(cellCoords - lineSizeInCellCoords);

    bvec4 discarded = bvec4(
        isDiscarded(thisCell),
        isDiscarded(vec2(thisCell.x, prevCell.y)),
        isDiscarded(prevCell),
        isDiscarded(vec2(prevCell.x, thisCell.y)));

    if (thisCell == prevCell || all(discarded))
        discard;

    fragColor = color * qt_Opacity;
}
