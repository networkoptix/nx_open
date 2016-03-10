import QtQuick 2.4

import com.networkoptix.qml 1.0

Item {
    id: indicator

    implicitWidth: dp(48)
    implicitHeight: dp(48)

    anchors.alignWhenCentered: false

    property color color: "white"
    property int progress: -1
    property real lineWidth: dp(3)

    onProgressChanged: {
        if (progress < 0)
            return

        canvas.rotation = -90
        canvas.arcStart = 0
        canvas.arcEnd = Math.PI * 2 * Math.min(100, progress) / 100
    }

    onVisibleChanged: {
        if (progress >= 0)
            return

        // Reset arc animation
        rotationAnimation.restart()
        arcAnimation.restart()
    }

    Canvas {
        id: canvas

        anchors.centerIn: parent
        anchors.alignWhenCentered: false
        width: parent.width / iconScale()
        height: parent.height / iconScale()
        scale: iconScale()

        renderStrategy: Canvas.Cooperative

        property real lineWidth: indicator.lineWidth / iconScale()
        property real radius: Math.min(width, height) / 2 - canvas.lineWidth

        property real arcStart: 0
        property real arcEnd: 0

        readonly property real longArc: Math.PI / 2 * 3
        readonly property real shortArc: Math.PI / 8

        readonly property int animationDuration: 800

        onArcStartChanged: requestPaint()
        onArcEndChanged: requestPaint()

        onPaint: {
            var ctx = canvas.getContext('2d')
            ctx.reset()

            ctx.lineWidth = canvas.lineWidth
            ctx.strokeStyle = color
            ctx.lineCap = "butt"

            ctx.arc(canvas.width / 2, canvas.height / 2, radius, arcStart, arcEnd, false)
            ctx.stroke()
        }

        Behavior on arcStart {
            enabled: progress >= 0
            NumberAnimation { duration: 200 }
        }
        Behavior on arcEnd {
            enabled: progress >= 0
            NumberAnimation { duration: 200 }
        }

        NumberAnimation on rotation {
            id: rotationAnimation

            duration: canvas.animationDuration * 3
            from: -90
            to: 270
            running: indicator.visible && progress < 0
            loops: Animation.Infinite
        }

        SequentialAnimation {
            id: arcAnimation

            running: indicator.visible && progress < 0
            loops: Animation.Infinite

            ParallelAnimation {
                NumberAnimation {
                    target: canvas
                    properties: "arcEnd"
                    from: canvas.shortArc
                    to: canvas.longArc
                    easing.type: Easing.InOutQuad
                    duration: canvas.animationDuration
                }
            }

            ParallelAnimation {
                NumberAnimation {
                    target: canvas
                    properties: "arcStart"
                    from: 0
                    to: Math.PI * 2
                    easing.type: Easing.InOutQuad
                    duration: canvas.animationDuration
                }
                NumberAnimation {
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

