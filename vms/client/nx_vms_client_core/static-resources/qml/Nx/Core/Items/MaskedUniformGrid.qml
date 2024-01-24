// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick

import nx.vms.client.core

UniformGrid
{
    // Texture provider item. Texture should have resolution cellCountX by cellCountY.
    // Each grid cell is drawn if and only if corresponding texel alpha is >= 0.5
    property Item maskTextureProvider: null

    // Implementation.

    readonly property int maskTextureValid: !!maskTextureProvider
    readonly property vector2d cellCounts: Qt.vector2d(cellCountX, cellCountY)

    vertexShader: "qrc:/qml/Nx/Core/Items/MaskedUniformGrid.vert.qsb"
    fragmentShader: "qrc:/qml/Nx/Core/Items/MaskedUniformGrid.frag.qsb"
}
