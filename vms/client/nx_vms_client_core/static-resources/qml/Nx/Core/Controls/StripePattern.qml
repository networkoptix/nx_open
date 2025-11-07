// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick

// Configurable horizontal, vertical or oblique stripe filled rectangle.
Item
{
    id: item

    property alias color: shader.color
    property alias backgroundColor: shader.backgroundColor

    property real stripeWidth: 4
    property real gapWidth: 4

    // Stripe angle.
    property real angleDegrees: 30

    // Pattern offset, in logical units.
    property real horizontalOffset: 0
    property real verticalOffset: 0

    ShaderEffect
    {
        id: shader

        anchors.fill: item

        property color color: "green"
        property color backgroundColor: "transparent"

        property real stripeWidth: Math.max(item.stripeWidth, 1)
        property real gapWidth: Math.max(item.gapWidth, 1)

        property size itemSize: Qt.size(width, height)

        property vector2d uvOffset: Qt.vector2d(item.horizontalOffset, item.verticalOffset)

        // A normal perpendicular to the stripes.
        property vector2d normal:
        {
            const angle = item.angleDegrees * (Math.PI / 180.0)
            return Qt.vector2d(-Math.sin(angle), Math.cos(angle));
        }

        fragmentShader: "qrc:/qml/Nx/Core/Controls/StripePattern.frag.qsb"
    }
}
