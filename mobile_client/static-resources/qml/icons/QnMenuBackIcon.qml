import QtQuick 2.4

Item {
    id: icon

    property color color: "white"

    property bool menuSate: true

    property real animationProgress: 0.0

    width: dp(24)
    height: dp(24)

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

            ctx.lineWidth = dp(2)
            ctx.strokeStyle = color
            ctx.lineCap = "square"

            ctx.moveTo(linearCombine(animationPosition, dp(3), dp(4)), dp(12))
            ctx.lineTo(linearCombine(animationPosition, dp(21), dp(18)), dp(12))
            ctx.stroke()

            ctx.moveTo(linearCombine(animationPosition, dp(3), dp(12)), linearCombine(animationPosition, dp(6), dp(4.5)))
            ctx.lineTo(linearCombine(animationPosition, dp(21), dp(19.5)), linearCombine(animationPosition, dp(6), dp(12)))
            ctx.stroke()

            ctx.moveTo(linearCombine(animationPosition, dp(3), dp(12)), linearCombine(animationPosition, dp(18), dp(19.5)))
            ctx.lineTo(linearCombine(animationPosition, dp(21), dp(19.5)), linearCombine(animationPosition, dp(18), dp(12)))
            ctx.stroke()
        }

        function linearCombine(p, v1, v2) {
            return v1 * (1.0 - p) + v2 * p
        }
    }
}
