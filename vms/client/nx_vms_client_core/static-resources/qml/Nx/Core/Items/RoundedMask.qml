import QtQuick 2.6

ShaderEffect
{
    property int cellCountX: 10
    property int cellCountY: 10
    property color lineColor: "black"
    property color fillColor: "white"
    property int thickness: 1

    // Texture provider item. Texture should have resolution cellCountX by cellCountY.
    // Each grid cell is drawn if and only if corresponding texel alpha is >= 0.5
    property Item maskTextureProvider: null

    // Implementation.

    readonly property bool maskTextureValid: maskTextureProvider
    readonly property vector2d cellCounts: Qt.vector2d(cellCountX, cellCountY)

    readonly property real lineSize: Math.max(1.0, thickness) - 0.01

    readonly property vector2d lineSizeInCellCoords: Qt.vector2d(
        lineSize / cellSize.x,
        lineSize / cellSize.y)

    readonly property vector2d cellSize: Qt.vector2d(
        (width - lineSize) / Math.max(cellCountX, 1),
        (height - lineSize) / Math.max(cellCountY, 1))

    readonly property string shaderVersion:
        OpenGLInfo.renderableType == OpenGLInfo.OpenGLES ? 100 : 120

    readonly property string precisionString:
        OpenGLInfo.renderableType == OpenGLInfo.OpenGLES ? "precision highp float;" : ""

    vertexShader: "
        #version " + shaderVersion + "
        " + precisionString + "

        uniform vec2 cellSize;
        uniform mat4 qt_Matrix;

        attribute vec4 qt_Vertex;

        varying vec2 cellCoords;

        void main()
        {
            gl_Position = qt_Matrix * qt_Vertex;
            cellCoords = qt_Vertex.xy / cellSize;
        }"

    fragmentShader: "
        #version " + shaderVersion + "
        " + precisionString + "

        uniform vec4 fillColor;
        uniform vec4 lineColor;
        uniform vec2 cellCounts;
        uniform vec2 lineSizeInCellCoords;
        uniform sampler2D maskTextureProvider;
        uniform bool maskTextureValid;
        uniform float qt_Opacity;

        varying vec2 cellCoords;

        bool isCellDiscarded(vec2 cell)
        {
            if (maskTextureValid)
                return texture2D(maskTextureProvider, (cell + vec2(0.5)) / cellCounts).a < 0.5;

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
                gl_FragColor = lineColor;
            else
                gl_FragColor = fillColor * qt_Opacity;
        }"
}
