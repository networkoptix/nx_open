#version 440

layout(location = 0) in vec2 cellCoords;

layout(location = 0) out vec4 fragColor;

layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
    vec2 cellSize;
    vec2 cellCounts;
    vec4 fillColor;
    vec4 lineColor;
    vec2 lineSizeInCellCoords;
    int maskTextureValid;
};

layout(binding = 1) uniform sampler2D maskTextureProvider;

bool isCellDiscarded(vec2 cell)
{
    if (maskTextureValid != 0)
        return texture(maskTextureProvider, (cell + vec2(0.5)) / cellCounts).a < 0.5;

    return true;
}

bool isPixelDiscarded(vec2 cellCoords)
{
    vec2 cell = floor(cellCoords); //< Cell with this fragment.
    vec2 local = cellCoords - cell - vec2(0.5); //< Local coords relative to cell center.
    vec2 next = cell + sign(local); //< Closest to this fragment diagonal cell.

    float radius = length(local); //< Distance from cell center.

    bool discardedNextX = isCellDiscarded(vec2(next.x, cell.y));
    bool discardedNextY = isCellDiscarded(vec2(cell.x, next.y));

    if (isCellDiscarded(cell))
        return (radius < 0.5 || discardedNextX || discardedNextY);

    return radius >= 0.5 && discardedNextX && discardedNextY
        && isCellDiscarded(vec2(next.x, next.y));
}

void main()
{
    if (isPixelDiscarded(cellCoords))
        discard;

    bool border = any(lessThan(cellCoords, lineSizeInCellCoords))
        || any(greaterThanEqual(cellCoords, cellCounts - lineSizeInCellCoords))
        || isPixelDiscarded(cellCoords + vec2(lineSizeInCellCoords.x, 0.0))
        || isPixelDiscarded(cellCoords - vec2(lineSizeInCellCoords.x, 0.0))
        || isPixelDiscarded(cellCoords + vec2(0.0, lineSizeInCellCoords.y))
        || isPixelDiscarded(cellCoords - vec2(0.0, lineSizeInCellCoords.y));

    if (border)
        fragColor = lineColor;
    else
        fragColor = fillColor * qt_Opacity;
}
