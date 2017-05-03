import QtQuick 2.6
import QtQuick.Window 2.0

import Nx 1.0

Rectangle
{
    id: control


    /**
     * "direction" property specifies blah blah TODO: write comment
     */
    property alias direction: d.movementVector
    property int ptzType: 0

    readonly property int kFreeWayPtz: 0
    readonly property int kEightWayPtz: 1
    readonly property int kFourWayPtz: 2
    readonly property int kTwoWayHorizontal: 3
    readonly property int kTwoWayVertical: 4

    signal buttonPressed(point direction)
    signal buttonReleased()

    implicitWidth: 136
    implicitHeight: implicitWidth
    radius: width / 2
    color: ColorTheme.transparent(ColorTheme.base8, 0.8)

    Canvas
    {
        id: drawer

        property bool drawButtonBorders: ptzType != kEightWayPtz && ptzType != kFreeWayPtz

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

            var angle = mathHelpers.getAngle(d.radialVector)
            var angleOffset = drawer.drawButtonBorders && control.ptzType != kFreeWayPtz
                ? d.currentSectionData.step / 2
                : Math.PI

            var startAngle = angle - angleOffset
            var finishAngle = angle + angleOffset
            drawSegment(context, d.centerPoint, d.controlRadius, startAngle, finishAngle)
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
            var type = control.ptzType
            var center = d.centerPoint
            var radius = d.controlRadius
            var holeLength = markerShadow.width

            switch(type)
            {
                case kTwoWayHorizontal:
                    var verticalOffset = Qt.vector2d(0, radius)
                    var topPoint = center.minus(verticalOffset)
                    var bottomPoint = center.plus(verticalOffset)
                    drawLineWithHole(context, topPoint, bottomPoint, holeLength)
                    return

                case kTwoWayVertical:
                    var horizontalOffset = Qt.vector2d(radius, 0)
                    var leftPoint = center.minus(horizontalOffset)
                    var rightPoint = center.plus(horizontalOffset)
                    drawLineWithHole(context, leftPoint, rightPoint, holeLength)
                    return

                case kFourWayPtz:
                    var radial = mathHelpers.getRadialVector(radius, -Math.PI / 4)
                    var topRight = center.plus(radial)
                    var bottomLeft = center.minus(radial)
                    drawLineWithHole(context, topRight, bottomLeft, holeLength)

                    radial = mathHelpers.getRadialVector(radius, Math.PI / 4)
                    var bottomRight = center.plus(radial)
                    var topLeft = center.minus(radial)
                    drawLineWithHole(context, bottomRight, topLeft, holeLength)
                    return

                case kEightWayPtz: //< Fallthrough
                case kFreeWayPtz: //< Fallthrough
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
        rotation: mathHelpers.getAngle(d.radialVector) * 180 / Math.PI
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
            if (mathHelpers.pointInCircle(mousePos, d.centerPoint, d.markerRadius))
                d.dragging = true

            drawer.requestPaint()
        }

        onReleased:
        {
            d.dragging = false
            drawer.requestPaint()
        }
        onMouseXChanged: { drawer.requestPaint() }
        onMouseYChanged: { drawer.requestPaint() }
    }

    Repeater
    {
        model: d.currentSectionData ? d.currentSectionData.buttons.length : 0

        delegate: Image
        {
            property int directionId: d.currentSectionData ? d.currentSectionData.buttons[index] : 0
            property bool horizontal: directionId == 3 || directionId == 4
            property real offset: marker.height / 2 + 16 + 16

            property vector2d position: d.centerPoint.plus(horizontal
                ? Qt.vector2d(directionId == 3 ? -offset : offset, 0)
                : Qt.vector2d(0, directionId == 1 ? -offset : offset))

            property color color: mouseArea.pressed ? "grey" : "white"

            source: lp("/images/ptz/ptz_arrow_dimmed.png")

            x: position.x - width / 2
            y: position.y - height / 2
            rotation: d.kButtonRotations[directionId]
            enabled: !d.dragging
            opacity: enabled ? 1 : 0.2
        }
    }

    QtObject
    {
        id: d


        property real controlRadius: control.width / 2
        property real markerRadius: marker.width / 2
        property real markerMaxDistance: controlRadius - markerRadius

        property bool dragging: false

        property vector2d mouseVector: mousePos.minus(centerPoint)
        property vector2d mousePos: mouseArea.pressed
            ? Qt.vector2d(mouseArea.mouseX, mouseArea.mouseY)
            : centerPoint

        property vector2d centerPoint: Qt.vector2d(controlRadius, controlRadius)

        property vector2d radialVector: radialPosition.minus(centerPoint)
        property vector2d radialPosition:
        {
            if (!mouseArea.pressed)
                return centerPoint


            if (control.ptzType == kFreeWayPtz)
                return mathHelpers.directionToPosition(mouseVector, controlRadius, true)

            if (!currentSectionData || currentSectionIndex == -1 || currentSectionId == -1)
                return centerPoint


            var directionAngle = mathHelpers.getAngleWithOffset(
                currentSectionData.startAngle, currentSectionData.step, currentSectionIndex + 0.5)

            var radialVector = mathHelpers.getRadialVector(
                d.controlRadius, directionAngle)

            return centerPoint.plus(radialVector)
        }

        property vector2d movementVector:
        {
            var cosAlpha = mathHelpers.getCosBetweenVectors(mouseVector, radialVector)
            var coeff = 1 / controlRadius
                * (dragging ? cosAlpha * mouseVector.length() / controlRadius : 1)

            var result = radialVector.times(coeff)
            return result.length() > 1 ? result.normalized() : result
        }

        property vector2d markerCenterPosition: dragging
            ? movementVector.times(markerMaxDistance).plus(centerPoint)
            : centerPoint

        /**
          * Eight-way ptz has 8 sectors with following layout:
          * 0 1 2
          * 3 - 4
          * 5 6 7
          * where:
          * 1 is "go up" command
          * 2 is "go up-left" command
          * .. etc
          *
          * All ptz with less ways count should map their sectors to listed above.
          * For example, four-way ptz layout is:
          *   1
          * 3 - 4
          *   6
          */

        property var currentSectionData:
        {
            switch(control.ptzType)
            {
                case kTwoWayHorizontal:
                    return kTwoWayHorizontalSectorData
                case kTwoWayVertical:
                    return kTwoWayVerticalSectorData
                case kFourWayPtz:
                    return kFourWayPtzSectorData
                case kEightWayPtz:
                    return kEightWayPtzSectorData
                case kFreeWayPtz:
                    return kFreeWayPtzSectorData
                default:
                    return null //< No ptz supported
            }
        }

        property int currentSectionIndex:
        {
            if (control.ptzType == kFreeWayPtz || !currentSectionData)
                return -1

            var sectorsCount = currentSectionData.sectorsMapping.length
            for (var i = 0; i !== sectorsCount; ++i)
            {
                var startAngle = mathHelpers.getAngleWithOffset(
                    currentSectionData.startAngle, currentSectionData.step, i)
                var finishAngle = mathHelpers.getAngleWithOffset(
                    currentSectionData.startAngle, currentSectionData.step, i + 1)

                if (mathHelpers.angleInSector(d.mousePos, d.centerPoint, startAngle, finishAngle))
                    return i
            }

            throw "Can't find sector"
        }

        property int currentSectionId:
        {
            return control.ptzType == kFreeWayPtz || currentSectionIndex < 0 || !currentSectionData
                ? -1
                : currentSectionData.sectorsMapping[currentSectionIndex]
        }

        readonly property var kTwoWayVerticalSectorData:
        {
            "startAngle": 0,
            "step": Math.PI,
            "sectorsMapping": [6, 1],
            "buttons": [6, 1]
        }

        readonly property var kTwoWayHorizontalSectorData:
        {
            "startAngle": -Math.PI / 2,
            "step": Math.PI,
            "sectorsMapping": [4, 3],
            "buttons": [4, 3]
        }

        readonly property var kFourWayPtzSectorData:
        {
            "startAngle": -Math.PI / 4,
            "step": Math.PI / 2,
            "sectorsMapping": [4, 6, 3, 1],
            "buttons": [4, 6, 3, 1]
        }

        readonly property var kEightWayPtzSectorData:
        {
            "startAngle": -Math.PI * 7 / 8,
            "step": Math.PI / 4,
            "sectorsMapping": [0, 1, 2, 3, 4, 5, 6, 7],
            "buttons": [4, 6, 3, 1]
        }

        readonly property var kFreeWayPtzSectorData:
        {
            "buttons": [4, 6, 3, 1]
        }

        readonly property var kButtonRotations: [-1, -90, -1,  180, 0, -1, 90, -1]
    }

    QtObject
    {
        id: mathHelpers

        function getAngleWithOffset(startAngle, step, count)
        {
            return startAngle + step * count
        }

        function directionToPosition(direction, maxLength, normalize)
        {
            if (normalize)
                direction = direction.normalized()
            var vector = direction.times(maxLength)
            return vector.plus(d.centerPoint)
        }

        function fuzzyIsNull(value)
        {
            var eps = 0.000001
            return value > -eps && value < eps
        }

        function getCosBetweenVectors(first, second)
        {
            return first.normalized().dotProduct(second.normalized())
        }

        function getAngle(vector)
        {
            var horizontalVector = Qt.vector2d(1, 0)
            var cosAlpha = getCosBetweenVectors(vector, horizontalVector)
            var sign = fuzzyIsNull(vector.y) ? 1 : Math.abs(vector.y) / vector.y
            var result = Math.acos(cosAlpha) * sign
            return result
        }

        function getRadialVector(radius, angle)
        {
            return Qt.vector2d(radius * Math.cos(angle), radius * Math.sin(angle))
        }

        /**
          * Checks if point (represented as vector2d) is inside circle with specified radius
          * and center.
          */
        function pointInCircle(point, center, radius)
        {
            return point.minus(center).length() <= radius
        }

        /**
          * Returns angle in range [0, 2 * PI)
          */
        function to2PIRange(angle)
        {
            var doublePi = Math.PI * 2
            while (angle < 0)
                angle += doublePi

            while (angle > doublePi)
                angle -= doublePi

            return angle
        }

        /**
          * Determnes if angle from "center" to "point" lays between specified angles.
          * Clockwise check.
          */
        function angleInSector(point, center, startAngle, finishAngle)
        {
            var vector = point.minus(center)
            var start = to2PIRange(startAngle)
            var finish = to2PIRange(finishAngle)
            var angle = to2PIRange(getAngle(vector))

            var result = start < finish
                ? angle >= start && angle <= finish
                : angle >= start || angle <= finish

            return result
        }
    }
}

