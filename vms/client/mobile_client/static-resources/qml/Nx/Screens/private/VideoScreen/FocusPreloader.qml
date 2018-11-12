import QtQuick 2.6
import QtQuick.Window 2.0
import Nx 1.0

import "Ptz/joystick_utils.js" as VectorUtils

BasePreloader
{
    id: control

    property color color: ColorTheme.transparent(ColorTheme.windowText, 0.9)

    property real thickness: 6
    property real sectionFov: Math.PI / 2
    property real sectionRotationDegrees: 90

    implicitHeight: 96
    implicitWidth: 96

    contentItem: Item
    {
        Rectangle
        {
            id: dot

            anchors.centerIn: parent

            width: control.thickness * 2
            height: control.thickness * 2

            radius: control.thickness
            color: control.color
            visible: true
        }

        Repeater
        {
            model: 2

            delegate: Canvas
            {
                id: section

                property real innerRadius: dot.radius + control.thickness * (index * 2 + 1)
                property real outerRadius: innerRadius + control.thickness

                anchors.centerIn: parent
                width: parent.width * Screen.devicePixelRatio
                height: parent.height * Screen.devicePixelRatio
                scale: 1 / Screen.devicePixelRatio

                onRotationChanged: section.requestPaint()

                onPaint:
                {
                    var context = getContext("2d")
                    context.reset()
                    context.scale(Screen.devicePixelRatio, Screen.devicePixelRatio)

                    context.fillStyle = control.color
                    drawSection(Math.PI * 3 / 2)
                    drawSection(Math.PI / 2)
                }

                function drawSection(targetAngle)
                {
                    var angleDiff = control.sectionFov / 2
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

                    readonly property int initialSign: index % 2 ? 1 : -1

                    running: true

                    PauseAnimation
                    {
                        duration: 100
                    }

                    NumberAnimation
                    {
                        id: numberAnimation

                        target: section
                        property: "rotation"
                        duration: 1000
                        easing.type: Easing.InOutQuad
                        from: animation.initialSign * control.sectionRotationDegrees / 2
                        to: -animation.initialSign * control.sectionRotationDegrees / 2
                    }

                    // Can't use Animation.Infinite since from/to fields of number
                    // animation stick on initial values.
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
}
