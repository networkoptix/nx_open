import QtQuick 2.4

Item {
    id: icon

    property color color: "white"

    property bool menuSate: true

    property real animationProgress: 0.0

    width: dps(24)
    height: dps(24)
    scale: iconScale()

    Canvas {
        id: canvas

        anchors.fill: parent

        renderStrategy: Canvas.Cooperative

        property real animationPosition: Math.max(Math.min(1.0, animationProgress), 0.0)
        onAnimationPositionChanged: requestPaint()

        rotation: menuSate ? (animationPosition * 180) : (2.0 - animationPosition) * 180

        onPaint: {
            var ctx = canvas.getContext('2d')
            ctx.reset()

            ctx.lineWidth = dps(2)
            ctx.strokeStyle = color
            ctx.lineCap = "square"

            ctx.moveTo(linearCombine(animationPosition, dps(3), dps(4)), dps(12))
            ctx.lineTo(linearCombine(animationPosition, dps(21), dps(18)), dps(12))
            ctx.stroke()

            ctx.moveTo(linearCombine(animationPosition, dps(3), dps(12)), linearCombine(animationPosition, dps(6), dps(4.5)))
            ctx.lineTo(linearCombine(animationPosition, dps(21), dps(19.5)), linearCombine(animationPosition, dps(6), dps(12)))
            ctx.stroke()

            ctx.moveTo(linearCombine(animationPosition, dps(3), dps(12)), linearCombine(animationPosition, dps(18), dps(19.5)))
            ctx.lineTo(linearCombine(animationPosition, dps(21), dps(19.5)), linearCombine(animationPosition, dps(18), dps(12)))
            ctx.stroke()
        }

        function linearCombine(p, v1, v2) {
            return v1 * (1.0 - p) + v2 * p
        }
    }
}
