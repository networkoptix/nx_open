import QtQuick 2.2

Item {
    id: icon

    property bool pauseState: true
    property color color: "black"

    property real padding: 4

    Canvas {
        id: canvas

        anchors.centerIn: parent
        width: parent.width - padding * 2
        height: parent.height - padding * 2
        antialiasing: true

        property real animationPosition: pauseState ? 0.0 : 1.0
        Behavior on animationPosition { NumberAnimation { duration: 300; easing: Easing.InExpo } }

        rotation: animationPosition * 90

        property real dist: Math.round(canvasSize.width * 0.25)
        property real tsize: canvasSize.width * Math.sqrt(3) / 2

        canvasSize.width: Math.min(width, height)
        canvasSize.height: canvasSize.width

        renderStrategy: Canvas.Cooperative

        onAnimationPositionChanged: requestPaint()

        onPaint: {
            var ctx = canvas.getContext('2d')
            ctx.clearRect(0, 0, canvasSize.width, canvasSize.height)

            var y0 = Math.floor((canvasSize.width - tsize) / 2)
            var y1 = Math.floor(canvasSize.height - y0)

            ctx.lineWidth = 0
            ctx.fillStyle = icon.color

            var cw = Math.floor(canvasSize.height * (1.0 - animationPosition))

            if (animationPosition < 0.5) {
                var dw = Math.floor(dist * (1.0 - animationPosition / 0.5))

                ctx.beginPath()
                ctx.moveTo((canvasSize.width - cw) / 2, y0)
                ctx.lineTo((canvasSize.width - dw) / 2, y0)
                ctx.lineTo((canvasSize.width - dw) / 2, y1)
                ctx.lineTo(0, y1)
                ctx.fill()
                ctx.closePath()

                ctx.moveTo((canvasSize.width + dw) / 2, y1)
                ctx.lineTo((canvasSize.width + dw) / 2, y0)
                ctx.lineTo((canvasSize.width + cw) / 2, y0)
                ctx.lineTo(canvasSize.width, y1)
                ctx.fill()
                ctx.closePath()
            } else {
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
