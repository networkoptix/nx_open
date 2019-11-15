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

    property real x1
    property real y1
    property real x2
    property real y2

    x: (x1 + x2) / 2
    y: (y1 + y2) / 2

    signal toggled()

    onCheckedChanged: canvas.requestPaint()
    onColorChanged: canvas.requestPaint()

    Canvas
    {
        id: canvas

        y: -8
        width: 36 + padding
        height: 16

        transformOrigin: Item.Left
        rotation: Math.atan2(y2 - y1, x2 - x1) / Math.PI * 180 - 90

        onPaint:
        {
            var ctx = getContext("2d")
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
