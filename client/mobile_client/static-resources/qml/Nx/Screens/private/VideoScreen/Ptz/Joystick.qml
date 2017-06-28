import QtQuick 2.6
import QtQuick.Window 2.0

import Nx 1.0

import "joystick_utils.js" as JoystickUtils

Rectangle
{
    id: control

    property vector2d direction:
    {
        var radians = -customRotation * Math.PI / 180
        var cosAngle = Math.cos(radians)
        var sinAngle = Math.sin(radians)
        var x = d.movementVector.x * cosAngle - d.movementVector.y * sinAngle
        var y = d.movementVector.x * sinAngle + d.movementVector.y * cosAngle
        return Qt.vector2d(x, -y)
    }
    property int joystickType: JoystickUtils.Type.Any
    property real customRotation: 0

    implicitWidth: 136
    implicitHeight: implicitWidth
    radius: width / 2
    color: ColorTheme.transparent(ColorTheme.base8, 0.8)

    Canvas
    {
        id: customCanvas

        property bool drawButtonBorders:
            joystickType != JoystickUtils.Type.EightWayPtz
            && joystickType != JoystickUtils.Type.FreeWayPtz

        anchors.centerIn: parent
        visible: drawButtonBorders || mouseArea.pressed
        width: parent.width * Screen.devicePixelRatio
        height: parent.height * Screen.devicePixelRatio
        scale: 1.0 / Screen.devicePixelRatio

        onPaint:
        {
            var context = getContext("2d")
            context.reset()
            context.scale(Screen.devicePixelRatio, Screen.devicePixelRatio)

            if (mouseArea.pressed)
                drawGradient(context)

            if (drawButtonBorders)
                drawBorders(context)
        }

        function drawGradient(context)
        {
            if (d.dragging)
                return

            var gradientStartPoint = drawButtonBorders
                ? d.centerPoint
                : d.centerPoint.minus(d.radialVector)

            var startColor = ColorTheme.transparent(ColorTheme.contrast1, 0.0)
            var finishColor = ColorTheme.transparent(ColorTheme.contrast1, 0.2)
            context.fillStyle = createGradient(context, gradientStartPoint, d.radialPosition,
                startColor, finishColor)

            var angle = JoystickUtils.getAngle(d.radialVector)
            var angleOffset = customCanvas.drawButtonBorders
                && control.joystickType != JoystickUtils.Type.FreeWayPtz
                ? d.currentSectionData.step / 2
                : Math.PI

            var startAngle = angle - angleOffset
            var finishAngle = angle + angleOffset
            drawSegment(context, d.centerPoint, control.radius, startAngle, finishAngle)
        }

        function createGradient(context, startPoint, finishPoint, startColor, endColor)
        {
            var gradient = context.createLinearGradient(
                startPoint.x, startPoint.y, finishPoint.x, finishPoint.y)
            gradient.addColorStop(0, startColor)
            gradient.addColorStop(1, endColor)
            return gradient
        }

        function drawSegment(context, center, radius, startAngle, finishAngle)
        {
            context.beginPath()
            context.moveTo(center.x, center.y)
            context.arc(center.x, center.y, radius, startAngle, finishAngle)
            context.lineTo(center.x, center.y)
            context.fill()
        }

        function drawLineWithHole(context, from, to, holeLength)
        {
            var vector = to.minus(from)
            var vectorLength = vector.length()
            var offset = vector.times((vectorLength - holeLength) / (2 * vectorLength))
            var intermediateTo = from.plus(offset)
            var intermediateFrom = to.minus(offset)

            context.beginPath()
            context.moveTo(from.x, from.y)
            context.lineTo(intermediateTo.x, intermediateTo.y)
            context.moveTo(intermediateFrom.x, intermediateFrom.y)
            context.lineTo(to.x, to.y)
            context.stroke()
        }

        function drawBorders(context)
        {
            var type = control.joystickType
            var center = d.centerPoint
            var radius = control.radius
            var holeLength = markerShadow.width

            switch (type)
            {
                case JoystickUtils.Type.TwoWayHorizontal:
                    var verticalOffset = Qt.vector2d(0, radius)
                    var topPoint = center.minus(verticalOffset)
                    var bottomPoint = center.plus(verticalOffset)
                    drawLineWithHole(context, topPoint, bottomPoint, holeLength)
                    return

                case JoystickUtils.Type.TwoWayVertical:
                    var horizontalOffset = Qt.vector2d(radius, 0)
                    var leftPoint = center.minus(horizontalOffset)
                    var rightPoint = center.plus(horizontalOffset)
                    drawLineWithHole(context, leftPoint, rightPoint, holeLength)
                    return

                case JoystickUtils.Type.FourWayPtz:
                    var radial = JoystickUtils.getRadialVector(radius, -Math.PI / 4)
                    var topRight = center.plus(radial)
                    var bottomLeft = center.minus(radial)
                    drawLineWithHole(context, topRight, bottomLeft, holeLength)

                    radial = JoystickUtils.getRadialVector(radius, Math.PI / 4)
                    var bottomRight = center.plus(radial)
                    var topLeft = center.minus(radial)
                    drawLineWithHole(context, bottomRight, topLeft, holeLength)
                    return

                case JoystickUtils.Type.EightWayPtz: //< Fallthrough
                case JoystickUtils.Type.FreeWayPtz: //< Fallthrough
                default:
                    return //< We don't need to draw delimiter lines
            }
        }
    }

    Item
    {
        id: marker

        x: d.markerCenterPosition.x - width / 2
        y: d.markerCenterPosition.y - height / 2
        z: 1
        width: 32
        height: width

        Rectangle
        {
            id: markerShadow

            width: 40
            height: width
            anchors.centerIn: parent
            radius: width / 2
            visible: d.dragging


            color: ColorTheme.transparent(ColorTheme.contrast1, 0.2)
        }

        Image
        {
            id: circleMarker

            anchors.centerIn: parent
            source: lp("/images/ptz/ptz_circle.png")
        }
    }

    Image
    {
        id: pointerImage

        property vector2d position: d.radialVector.times(
            1 + 0.15 * (d.dragging ? d.movementVector.length() : 1)).plus(d.centerPoint)

        source: lp("/images/ptz/ptz_arrow.png")

        x: position.x - width / 2
        y: position.y - height / 2
        scale: d.dragging ? d.movementVector.length() : 1
        visible: mouseArea.pressed
        rotation: JoystickUtils.getAngle(d.radialVector) * 180 / Math.PI
    }

    Rectangle
    {
        id: centerMarker

        width: 4
        height: 4
        radius: 2

        anchors.centerIn: parent
        visible: d.dragging
    }

    MouseArea
    {
        id: mouseArea

        anchors.fill: parent

        onPressed:
        {
            var mousePos = Qt.vector2d(mouseX, mouseY) //< Use explicit value to avoid dependencies
            if (JoystickUtils.pointInCircle(mousePos, d.centerPoint, d.markerRadius))
                d.dragging = true

            handleMouseMove()
        }

        onReleased:
        {
            d.dragging = false
            d.markerDragStarted = false
            customCanvas.requestPaint()
        }

        onMouseXChanged: handleMouseMove()
        onMouseYChanged: handleMouseMove()

        function handleMouseMove()
        {
            var mousePos = Qt.vector2d(mouseX, mouseY) //< Use explicit value to avoid dependencies
            if (d.centerPoint.minus(mousePos).length() > d.markerStartDragEps)
                d.markerDragStarted = true

            customCanvas.requestPaint()
        }
    }

    Repeater
    {
        model: d.currentSectionData ? d.currentSectionData.buttons.length : 0

        delegate: Image
        {
            property real offset: marker.height / 2 + 16 + 16

            property int buttonDirection:
                d.currentSectionData
                && d.currentSectionData.buttons.length > index
                ? d.currentSectionData.buttons[index]
                : JoystickUtils.Direction.Unknown

            property bool horizontal:
                buttonDirection == JoystickUtils.Direction.Left
                || buttonDirection == JoystickUtils.Direction.Right

            property vector2d position: d.centerPoint.plus(horizontal
                ? Qt.vector2d(buttonDirection == JoystickUtils.Direction.Left ? -offset : offset, 0)
                : Qt.vector2d(0, buttonDirection == JoystickUtils.Direction.Top ? -offset : offset))

            source: lp("/images/ptz/ptz_arrow_dimmed.png")

            x: position.x - width / 2
            y: position.y - height / 2
            rotation: JoystickUtils.ButtonRotations[buttonDirection]
            enabled: !d.dragging
            opacity: enabled ? 1 : 0.2
        }
    }

    QtObject
    {
        id: d

        property real markerRadius: marker.width / 2
        property real markerStartDragEps: markerRadius
        property real markerMaxDistance: control.radius - markerRadius

        property bool dragging: false

        property vector2d mouseVector: mousePos.minus(centerPoint)
        property vector2d mousePos: mouseArea.pressed && markerDragStarted
            ? Qt.vector2d(mouseArea.mouseX, mouseArea.mouseY)
            : centerPoint
        property bool markerDragStarted: false

        property vector2d centerPoint: Qt.vector2d(control.radius, control.radius)

        property vector2d radialVector: radialPosition.minus(centerPoint)
        property vector2d radialPosition:
        {
            if (!mouseArea.pressed)
                return centerPoint

            if (control.joystickType == JoystickUtils.Type.FreeWayPtz)
                return JoystickUtils.directionToPosition(mouseVector, control.radius, true)

            if (!currentSectionData || currentSectionIndex == -1)
                return centerPoint

            var directionAngle = JoystickUtils.getAngleWithOffset(
                currentSectionData.startAngle, currentSectionData.step, currentSectionIndex + 0.5)

            var vector = JoystickUtils.getRadialVector(
                control.radius, directionAngle)

            return centerPoint.plus(vector)
        }

        readonly property real kMaxSinglePressSpeedFactor: 0.8
        property vector2d movementVector:
        {
            if (!mouseArea.pressed)
                return Qt.vector2d(0, 0)

            var cosAlpha = JoystickUtils.getCosBetweenVectors(mouseVector, radialVector)
            var factor = dragging
                ? cosAlpha * mouseVector.length() / control.radius
                : kMaxSinglePressSpeedFactor

            var coeff = factor / control.radius

            var result = radialVector.times(coeff)
            return result.length() > 1 ? result.normalized() : result
        }

        property vector2d markerCenterPosition: dragging
            ? movementVector.times(markerMaxDistance).plus(centerPoint)
            : centerPoint

        property var currentSectionData: JoystickUtils.getSectorData(control.joystickType)

        property int currentSectionIndex:
        {
            if (control.joystickType == JoystickUtils.Type.FreeWayPtz
                || !currentSectionData
                || mousePos == centerPoint)
            {
                return -1
            }

            var sectorsCount = currentSectionData.sectorsCount
            for (var i = 0; i !== sectorsCount; ++i)
            {
                var startAngle = JoystickUtils.getAngleWithOffset(
                    currentSectionData.startAngle, currentSectionData.step, i)
                var finishAngle = JoystickUtils.getAngleWithOffset(
                    currentSectionData.startAngle, currentSectionData.step, i + 1)

                if (JoystickUtils.angleInSector(d.mousePos, d.centerPoint, startAngle, finishAngle))
                    return i
            }

            throw "Can't find sector"
        }
    }
}

