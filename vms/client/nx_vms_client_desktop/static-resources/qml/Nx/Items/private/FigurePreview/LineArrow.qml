import QtQuick 2.0

Item
{
    id: arrow

    property color color: "white"
    property real padding: 2

    property real x1
    property real y1
    property real x2
    property real y2

    x: (x1 + x2) / 2
    y: (y1 + y2) / 2

    onPaddingChanged: canvas.requestPaint()
    onColorChanged: canvas.requestPaint()

    Canvas
    {
        id: canvas

        y: -3.5
        width: 7 + arrow.padding
        height: 7

        transformOrigin: Item.Left
        rotation: Math.atan2(y2 - y1, x2 - x1) / Math.PI * 180 - 90

        onPaint:
        {
            var ctx = getContext("2d")
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
}
