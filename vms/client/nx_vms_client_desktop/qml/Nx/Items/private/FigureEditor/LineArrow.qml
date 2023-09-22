// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.0

Item
{
    id: arrow

    property color color: "white"
    property color defaultColor: "#303030"
    property color outlineColor: "white"
    property real padding: 8
    property bool checked: false
    property alias hovered: mouseArea.containsMouse

    signal toggled()

    onCheckedChanged: canvas.requestPaint()
    onColorChanged: canvas.requestPaint()

    transformOrigin: Item.Left

    Canvas
    {
        id: canvas

        y: -8
        width: 36 + padding
        height: 16

        onPaint:
        {
            let ctx = getContext("2d")
            ctx.reset()
            ctx.fillStyle = arrow.checked ? arrow.color : arrow.defaultColor
            ctx.strokeStyle = arrow.outlineColor
            ctx.translate(arrow.padding, 0)

            ctx.moveTo(0.5, 6.5)
            ctx.lineTo(22.5, 6.5)
            ctx.lineTo(22.5, 1.5)
            ctx.lineTo(33.5, 8)
            ctx.lineTo(22.5, 14.5)
            ctx.lineTo(22.5, 9.5)
            ctx.lineTo(0.5, 9.5)
            ctx.lineTo(0.5, 6.5)

            ctx.fill()
            ctx.stroke()
        }

        MouseArea
        {
            id: mouseArea

            anchors.fill: parent
            anchors.leftMargin: padding

            hoverEnabled: true

            onClicked:
            {
                arrow.checked = !arrow.checked
                toggled()
            }
        }
    }
}
