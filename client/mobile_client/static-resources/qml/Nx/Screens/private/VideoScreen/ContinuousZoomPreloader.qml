import QtQuick 2.6
import QtQuick.Window 2.0

import "../VideoScreen/Ptz/joystick_utils.js" as VectorUtils

Item
{
    id: control

    property color color: "white"
    property real spacing: 8
    property real sectionFOV: Math.PI / 2
    property real sectionRotation: Math.PI / 2

    implicitHeight: 96
    implicitWidth: 96

    Rectangle
    {
        id: dot

        anchors.centerIn: parent

        width: control.spacing * 2
        height: control.spacing * 2

        radius: control.spacing
        visible: true
    }

    Repeater
    {
        model: 2

        delegate:
            Canvas
            {
                id: section

                property int sectionIndex: index
                property real offset: 0
                property real innerRadius: dot.radius + control.spacing * (index * 2 + 1)
                property real outerRadius: innerRadius + control.spacing

                width: parent.width * Screen.devicePixelRatio
                height: parent.height * Screen.devicePixelRatio
                scale: 1 / Screen.devicePixelRatio

                anchors.centerIn: parent
                onOffsetChanged: section.requestPaint()

                onPaint:
                {
                    var context = getContext("2d")
                    context.reset()
                    context.scale(Screen.devicePixelRatio, Screen.devicePixelRatio)

                    context.fillStyle = Qt.rgba(255, 255, 255, 255)
                    drawSection(offset + Math.PI * 3 / 2)
                    drawSection(offset + Math.PI / 2)
                }

                function drawSection(targetAngle)
                {
                    var angleDiff = control.sectionFOV / 2
                    var centerAspect = 1 / (2 * Screen.devicePixelRatio)
                    var center = Qt.vector2d(
                        section.width * centerAspect,
                        section.height * centerAspect)
                    var sectionStartPoint = center.plus(VectorUtils.getRadialVector(
                        section.innerRadius, targetAngle - angleDiff))

                    context.beginPath();
                    context.moveTo(sectionStartPoint.x, sectionStartPoint.y)
                    context.arc(center.x, center.y, section.innerRadius,
                        targetAngle - angleDiff, targetAngle + angleDiff, false)

                    context.arc(center.x, center.y, section.outerRadius,
                        targetAngle + angleDiff, targetAngle - angleDiff, true)

                    context.fill()
                }

                SequentialAnimation
                {
                    id: animation

                    readonly property int initialSign: section.sectionIndex %2 ? 1 : -1

                    // Can't use Animation.Infinite since from/to fields of number
                    // animation stick on initial values
                    loops: 1
                    running: true

                    PauseAnimation
                    {
                        duration: 100
                    }

                    NumberAnimation
                    {
                        id: numberAnimation

                        target: section
                        property: "offset"
                        duration: 1000
                        easing.type: Easing.InOutQuad
                        from: animation.initialSign * control.sectionRotation / 2
                        to: -animation.initialSign * control.sectionRotation / 2
                    }

                    onStopped:
                    {
                        numberAnimation.from = -numberAnimation.from
                        numberAnimation.to = -numberAnimation.to
                        start()
                    }
                }
            }
    }
}
