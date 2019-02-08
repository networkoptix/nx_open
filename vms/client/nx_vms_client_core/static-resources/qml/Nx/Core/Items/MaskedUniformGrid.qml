import QtQuick 2.6

UniformGrid
{
    // Texture provider item. Texture should have resolution cellCountX by cellCountY.
    // Each grid cell is drawn if and only if corresponding texel alpha is >= 0.5
    property Item maskTextureProvider: null

    // Implementation.

    readonly property bool maskTextureValid: maskTextureProvider
    readonly property vector2d cellCounts: Qt.vector2d(cellCountX, cellCountY)

    fragmentShader: "
        #version " + shaderVersion + "
        " + precisionString + "

        uniform vec4 color;
        uniform vec2 cellCounts;
        uniform vec2 lineSizeInCellCoords;
        uniform sampler2D maskTextureProvider;
        uniform bool maskTextureValid;
        uniform float qt_Opacity;

        varying vec2 cellCoords;

        bool isDiscarded(vec2 cell)
        {
            return maskTextureValid
                ? texture2D(maskTextureProvider, (cell + vec2(0.5)) / cellCounts).a < 0.5
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

            gl_FragColor = color * qt_Opacity;
        }"
}
