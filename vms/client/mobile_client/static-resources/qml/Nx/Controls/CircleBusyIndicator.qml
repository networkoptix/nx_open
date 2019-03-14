import QtQuick 2.6
import QtQuick.Window 2.0
import QtQuick.Controls 2.4

BusyIndicator
{
    id: control

    property color color: "white"
    property int progress: -1
    property real lineWidth: 3

    implicitWidth: 48
    implicitHeight: 48

    anchors.alignWhenCentered: false

    onProgressChanged:
    {
        if (progress < 0)
            return

        canvas.rotation = -90
        canvas.arcStart = 0
        canvas.arcEnd = Math.PI * 2 * Math.min(100, progress) / 100
    }

    onVisibleChanged:
    {
        if (progress >= 0)
            return

        // Reset arc animation
        rotationAnimation.restart()
        arcAnimation.restart()
    }

    contentItem: Item
    {
        anchors.fill: parent

        Canvas
        {
            id: canvas

            anchors.centerIn: parent

            width: parent.width * Screen.devicePixelRatio
            height: parent.height * Screen.devicePixelRatio

            renderStrategy: Canvas.Cooperative
            scale: 1.0 / Screen.devicePixelRatio

            property real lineWidth: control.lineWidth * Screen.devicePixelRatio
            property real radius: Math.min(width, height) / 2 - lineWidth

            property real arcStart: 0
            property real arcEnd: 0

            readonly property real longArc: Math.PI / 2 * 3
            readonly property real shortArc: Math.PI / 8

            readonly property int animationDuration: 800

            onArcStartChanged: requestPaint()
            onArcEndChanged: requestPaint()

            onPaint:
            {
                var ctx = canvas.getContext('2d')
                ctx.reset()

                ctx.lineWidth = canvas.lineWidth
                ctx.strokeStyle = color
                ctx.lineCap = "butt"

                ctx.arc(canvas.width / 2, canvas.height / 2, radius, arcStart, arcEnd, false)
                ctx.stroke()
            }

            Behavior on arcStart
            {
                enabled: progress >= 0
                NumberAnimation { duration: 200 }
            }
            Behavior on arcEnd
            {
                enabled: progress >= 0
                NumberAnimation { duration: 200 }
            }

            NumberAnimation on rotation
            {
                id: rotationAnimation

                duration: canvas.animationDuration * 3
                from: -90
                to: 270
                running: control.visible && progress < 0
                loops: Animation.Infinite
            }

            SequentialAnimation
            {
                id: arcAnimation

                running: control.visible && progress < 0
                loops: Animation.Infinite

                ParallelAnimation
                {
                    NumberAnimation
                    {
                        target: canvas
                        properties: "arcEnd"
                        from: canvas.shortArc
                        to: canvas.longArc
                        easing.type: Easing.InOutQuad
                        duration: canvas.animationDuration
                    }
                }

                ParallelAnimation
                {
                    NumberAnimation
                    {
                        target: canvas
                        properties: "arcStart"
                        from: 0
                        to: Math.PI * 2
                        easing.type: Easing.InOutQuad
                        duration: canvas.animationDuration
                    }
                    NumberAnimation
                    {
                        target: canvas
                        properties: "arcEnd"
                        from: canvas.longArc
                        to: 2 * Math.PI + canvas.shortArc
                        easing.type: Easing.InOutQuad
                        duration: canvas.animationDuration
                    }
                }
            }
        }
    }
}

