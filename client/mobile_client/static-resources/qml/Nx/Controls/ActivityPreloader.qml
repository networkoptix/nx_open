import QtQuick 2.6
import Nx 1.0

Item
{
    id: control

    property real radius: 17
    property color color: ColorTheme.brightText
    property real lineWidth: 2

    implicitWidth: (radius + lineWidth) * 2
    implicitHeight: implicitWidth

    Repeater
    {
        model: 2

        delegate: Canvas
        {
            id: linesControl

            anchors.fill: parent

            onPaint:
            {
                var ctx = getContext("2d")
                ctx.reset()

                var centerX = width / 2
                var centerY = height / 2

                for (var i = 0; i < 4; ++i)
                {
                    ctx.beginPath()
                    var angle = i * Math.PI / 2
                    var startAngle = angle - Math.PI / 12
                    var endAngle = angle + Math.PI / 12

                    ctx.lineCap = "round"
                    ctx.strokeStyle = control.color
                    ctx.lineWidth = control.lineWidth
                    ctx.arc(centerX, centerY, control.radius, startAngle, endAngle);
                    ctx.stroke()
                }
            }

            NumberAnimation
            {
                id: animation

                running: true
                loops: Animation.Infinite
                duration: (index + 1) * 450

                target: linesControl
                property: "rotation"
                from: 0
                to: 90
            }
        }
    }
}
