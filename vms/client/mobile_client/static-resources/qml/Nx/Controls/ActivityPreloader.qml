import QtQuick 2.6
import QtQuick.Window 2.0
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
            width: parent.width * Screen.devicePixelRatio
            height: parent.height * Screen.devicePixelRatio
            anchors.centerIn: parent
            scale: 1.0 / Screen.devicePixelRatio

            onPaint:
            {
                var ctx = getContext("2d")
                ctx.reset()
                ctx.scale(Screen.devicePixelRatio, Screen.devicePixelRatio)

                var centerX = control.width / 2
                var centerY = control.height / 2

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

            NumberAnimation on rotation
            {
                running: true
                loops: Animation.Infinite
                duration: (index + 1) * 1800
                from: 0
                to: 360
            }
        }
    }
}
