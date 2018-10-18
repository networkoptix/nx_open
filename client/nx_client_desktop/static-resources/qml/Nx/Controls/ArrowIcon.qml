import QtQuick 2.11

Canvas
{
    id: icon

    property color color: "black"

    width: 10
    height: 10

    onPaint:
    {
        var ctx = getContext("2d")
        ctx.strokeStyle = icon.color
        ctx.lineJoin = "miter"
        ctx.lineWidth = 1.2
        ctx.moveTo(1, 3)
        ctx.lineTo(5, 7)
        ctx.lineTo(9, 3)
        ctx.stroke()
    }
}
