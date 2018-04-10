import QtQuick 2.6

ShaderEffect
{
    property int cellCountX: 10
    property int cellCountY: 10
    property int lineWidth: 1
    property color lineColor: "white"

    // Implementation.

    readonly property real lineThickness: Math.max(1.0, lineWidth)
    readonly property vector2d cellSize: Qt.vector2d(
        (width - lineThickness) / Math.max(cellCountX, 1),
        (height - lineThickness) / Math.max(cellCountY, 1))

    vertexShader: "
        #version 120

        uniform mat4 qt_Matrix;
        attribute vec4 qt_Vertex;

        varying vec2 coordinates;

        void main()
        {
            gl_Position = qt_Matrix * qt_Vertex;
            coordinates = qt_Vertex.xy;
        }"

    fragmentShader: "
        #version 120

        uniform vec2 cellSize;
        uniform vec4 lineColor;
        uniform float lineThickness;
        uniform float qt_Opacity;

        varying vec2 coordinates;

        void main()
        {
            vec2 closestLines = floor(coordinates / cellSize) * cellSize;
            //closestLines += (vec2(1.0) - step(0.001, abs(coordinates - closestLines))) * 0.001;
            if (all(greaterThan(coordinates - closestLines, vec2(lineThickness))))
                discard;

            gl_FragColor = lineColor * qt_Opacity;
        }"
}
