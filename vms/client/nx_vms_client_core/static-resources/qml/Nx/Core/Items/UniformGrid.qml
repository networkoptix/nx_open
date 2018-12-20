import QtQuick 2.6

ShaderEffect
{
    property int cellCountX: 10
    property int cellCountY: 10
    property int thickness: 1
    property color color: "white"

    // Implementation.

    readonly property real lineSize: Math.max(1.0, thickness)

    readonly property vector2d cellSize: Qt.vector2d(
        (width - lineSize) / Math.max(cellCountX, 1),
        (height - lineSize) / Math.max(cellCountY, 1))

    readonly property vector2d lineSizeInCellCoords: Qt.vector2d(
        lineSize / cellSize.x,
        lineSize / cellSize.y)

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

        uniform vec4 color;
        uniform vec2 lineSizeInCellCoords;
        uniform float qt_Opacity;

        varying vec2 cellCoords;

        void main()
        {
            vec2 thisCell = floor(cellCoords);
            vec2 prevCell = floor(cellCoords - lineSizeInCellCoords);

            if (thisCell == prevCell)
                discard;

            gl_FragColor = color * qt_Opacity;
        }"
}
