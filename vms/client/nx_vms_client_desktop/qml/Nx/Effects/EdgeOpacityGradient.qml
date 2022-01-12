// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.15

/**
 * Produces linear opacity gradient at specified edges.
 */
ShaderEffect
{
    id: effect

    property int edges: Qt.LeftEdge | Qt.RightEdge | Qt.TopEdge | Qt.BottomEdge

    property real gradientWidth: 28

    property real leftGradientWidth: gradientWidth
    property real rightGradientWidth: gradientWidth
    property real topGradientWidth: gradientWidth
    property real bottomGradientWidth: gradientWidth

    readonly property vector4d thresholds: Qt.vector4d(
        (edges & Qt.LeftEdge) ? (leftGradientWidth / width) : 0.0,
        (edges & Qt.TopEdge) ? (topGradientWidth / height) : 0.0,
        (edges & Qt.RightEdge) ? (rightGradientWidth / width) : 0.0,
        (edges & Qt.BottomEdge) ? (bottomGradientWidth / height) : 0.0)

    fragmentShader: "qrc:/qml/Nx/Effects/EdgeOpacityGradient.frag.qsb"
}
