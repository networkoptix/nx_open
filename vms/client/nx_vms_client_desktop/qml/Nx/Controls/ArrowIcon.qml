// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.11

Canvas
{
    id: icon

    property color color: "black"
    property real lineWidth: 1.2

    width: 10
    height: 10

    onColorChanged: requestPaint()
    onLineWidthChanged: requestPaint()
    onVisibleChanged:
        requestPaint() //< Helps to repaint the arrow when QQuickWidget visibility is changed.

    onPaint:
    {
        let ctx = getContext("2d")
        ctx.reset()
        ctx.strokeStyle = icon.color
        ctx.lineJoin = "miter"
        ctx.lineWidth = icon.lineWidth
        ctx.moveTo(1, 3)
        ctx.lineTo(5, 7)
        ctx.lineTo(9, 3)
        ctx.stroke()
    }
}
