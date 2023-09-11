// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

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

    readonly property int maskTextureValid: !!maskTextureProvider
    readonly property vector2d cellCounts: Qt.vector2d(cellCountX, cellCountY)

    readonly property real lineSize: Math.max(1.0, thickness) - 0.01

    readonly property vector2d lineSizeInCellCoords: Qt.vector2d(
        lineSize / cellSize.x,
        lineSize / cellSize.y)

    readonly property vector2d cellSize: Qt.vector2d(
        (width - lineSize) / Math.max(cellCountX, 1),
        (height - lineSize) / Math.max(cellCountY, 1))

    vertexShader: "qrc:/qml/Nx/Core/Items/RoundedMask.vert.qsb"
    fragmentShader: "qrc:/qml/Nx/Core/Items/RoundedMask.frag.qsb"
}
