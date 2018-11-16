import QtQuick 2.6
import QtQuick.Window 2.0

Item
{
    id: icon

    property bool pauseState: true
    property color color: "white"

    Canvas
    {
        id: canvas

        anchors.centerIn: parent
        width: Math.min(parent.width, parent.height) * Screen.devicePixelRatio
        height: width
        scale: 1.0 / Screen.devicePixelRatio

        property real animationPosition: pauseState ? 1.0 : 0.0
        Behavior on animationPosition { NumberAnimation { duration: 300; easing.type: Easing.OutQuad } }

        rotation: pauseState ? (animationPosition * 90) : (2.0 - animationPosition) * 90

        property real dist: Math.round(canvasSize.width * 0.25)
        property real tsize: canvasSize.width * Math.sqrt(3) / 2

        renderStrategy: Canvas.Cooperative

        onAnimationPositionChanged: requestPaint()

        onPaint:
        {
            var ctx = canvas.getContext('2d')
            ctx.clearRect(0, 0, canvasSize.width, canvasSize.height)

            var y0 = (canvasSize.width - tsize) / 2
            var y1 = canvasSize.height - y0 * (1.0 + animationPosition)
            y0 *= (1.0 - animationPosition)

            ctx.lineWidth = 0
            ctx.fillStyle = icon.color

            var cw = canvasSize.height * (1.0 - animationPosition)

            if (animationPosition < 0.5)
            {
                var dw =dist * (1.0 - animationPosition / 0.5)

                ctx.beginPath()
                ctx.moveTo((canvasSize.width - cw) / 2, y0)
                ctx.lineTo((canvasSize.width - dw) / 2, y0)
                ctx.lineTo((canvasSize.width - dw) / 2, y1)
                ctx.lineTo(0, y1)
                ctx.fill()

                ctx.moveTo((canvasSize.width + dw) / 2, y1)
                ctx.lineTo((canvasSize.width + dw) / 2, y0)
                ctx.lineTo((canvasSize.width + cw) / 2, y0)
                ctx.lineTo(canvasSize.width, y1)
                ctx.fill()
            }
            else
            {
                ctx.beginPath()
                ctx.moveTo(0, y1)
                ctx.lineTo((canvasSize.width - cw) / 2, y0)
                ctx.lineTo((canvasSize.width + cw) / 2, y0)
                ctx.lineTo(canvasSize.width, y1)
                ctx.fill()
            }
        }
    }
}
