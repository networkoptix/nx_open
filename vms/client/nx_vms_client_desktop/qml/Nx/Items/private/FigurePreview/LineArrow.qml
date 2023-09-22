// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.0
import Qt5Compat.GraphicalEffects

Item
{
    id: arrow

    property color color: "white"
    property real padding: 2

    onPaddingChanged: canvas.requestPaint()
    onColorChanged: canvas.requestPaint()

    transformOrigin: Item.Left

    Canvas
    {
        id: canvas

        y: -3.5
        width: 7 + arrow.padding
        height: 7

        visible: false

        onPaint:
        {
            let ctx = getContext("2d")
            ctx.reset()
            ctx.fillStyle = arrow.color
            ctx.translate(arrow.padding, 0)

            ctx.moveTo(0, 0)
            ctx.lineTo(7, 3.5)
            ctx.lineTo(0, 7)
            ctx.lineTo(0, 0)

            ctx.fill()
        }
    }

    DropShadow
    {
        anchors.fill: canvas
        source: canvas
        color: "black"
        radius: 3
    }
}
