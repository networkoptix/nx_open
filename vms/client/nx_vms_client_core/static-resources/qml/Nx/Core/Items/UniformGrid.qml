// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.6
import nx.client.core.graphics 1.0

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

    vertexShader: "qrc:/qml/Nx/Core/Items/UniformGrid.vert.qsb"
    fragmentShader: "qrc:/qml/Nx/Core/Items/UniformGrid.frag.qsb"
}
