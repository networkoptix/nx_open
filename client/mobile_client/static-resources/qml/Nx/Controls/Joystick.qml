import QtQuick 2.6

Rectangle
{
    id: control

    color: "lightblue" // todo remove me

    property alias direction: _private.direction

    readonly property int kEightWayPtz: 0
    readonly property int kFourWayPtz: 1
    readonly property int kTwoWayHorizontal: 3
    readonly property int kTwoWayVertical: 4

    /**
     * "direction" property specifies blah blah TODO: write comment
     */

    property bool supportDrag: false
    property bool supportSingleShot: false

    signal singleShot(point direction)
    signal buttonPressed(point direction)
    signal buttonReleased()

    implicitWidth: 136;
    implicitHeight: implicitWidth
    radius: width / 2

    onDirectionChanged: { gradientDrawer.requestPaint() }

    Canvas
    {
        id: gradientDrawer

        property real angle: Math.PI / 2
        property real halfAngle: angle / 2

        anchors.fill: parent

        onPaint:
        {
            var context = getContext("2d")
            context.reset()

            var center = _private.centerPoint
            var radial = _private.radialPosition
            var radius = _private.controlRadius
            var startAngle = _private.currentAngle - halfAngle
            var finishAngle = _private.currentAngle + halfAngle

            context.fillStyle = createGradient(context, center, radial,
                Qt.rgba(1, 1, 1, 0), Qt.rgba(1, 1, 1, 0.3))

            drawSegment(context, center, radius, startAngle, finishAngle)

            var innerRadius = _private.active ? 0 : marker.width / 2
            drawDelimitter(context, kFourWayPtz, center, innerRadius, radius)
        }

        function createGradient(context, startPoint, endPoint, startColor, endColor)
        {
            var gradient = context.createLinearGradient(
                startPoint.x, startPoint.y, endPoint.x, endPoint.y)
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

        function drawLine(context, from, to, holeLength)
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

        function drawDelimitter(context, type, center, innerRadius, outerRadius)
        {
            var holeLength = innerRadius * 2
            switch(type)
            {
                case kTwoWayHorizontal:
                    var verticalOffset = Qt.vector2d(0, outerRadius)
                    var topPoint = center.minus(verticalOffset)
                    var bottomPoint = center.plus(verticalOffset)
                    drawLine(context, topPoint, bottomPoint, holeLength)
                    return

                case kTwoWayVertical:
                    var horizontalOffset = Qt.vector2d(outerRadius, 0)
                    var leftPoint = center.minus(horizontalOffset)
                    var rightPoint = center.plus(horizontalOffset)
                    drawLine(context, leftPoint, rightPoint, holeLength)
                    return

                case kFourWayPtz:
                case kEightWayPtz:
                    var radial = _private.getRadialVector(outerRadius, -Math.PI / 4)
                    var topRight = center.plus(radial)
                    var bottomLeft = center.minus(radial)
                    drawLine(context, topRight, bottomLeft, holeLength)

                    radial = _private.getRadialVector(outerRadius, Math.PI / 4)
                    var bottomRight = center.plus(radial)
                    var topLeft = center.minus(radial)
                    drawLine(context, bottomRight, topLeft, holeLength)
                    return

                default:
                    console.log("Invalid ptz type")
                    return
            }
        }
    }

    Rectangle
    {
        id: centerMarker

        width: 4
        height: 4
        radius: 2

        anchors.centerIn: parent
        visible: _private.active
    }

    Item
    {
        id: marker

        x: _private.markerCenterPosition.x - width / 2
        y: _private.markerCenterPosition.y - height / 2
        width: 32
        height: width

        Rectangle
        {
            id: markerShadow

            x: -8
            y: -8
            width: 48
            height: width
            radius: width / 2
            visible: _private.active

            color: "lightgrey"
            opacity: 0.2
        }

        Rectangle
        {
            id: circleMarker

            anchors.fill: parent
            radius: width / 2

            visible: control.supportDrag
            color: "transparent"
            border.color: "white" // todo replace with actual color
        }
    }

    MouseArea
    {
        id: mouseArea

        anchors.fill: parent
        hoverEnabled: true
    }

    QtObject
    {
        id: _private;

        property bool active: supportDrag && mouseArea.pressed

        property real controlRadius: control.width / 2
        property real markerRadius: marker.width / 2
        property real markerMaxDistance: controlRadius - markerRadius

        property vector2d direction: _private.positionToDirection(
            markerTargetPosition, markerMaxDistance)

        property vector2d centerPoint: Qt.vector2d(controlRadius, controlRadius)
        property vector2d radialPosition: directionToPosition(direction, controlRadius, true)
        property real currentAngle: getAngle(radialPosition.minus(centerPoint))
        property vector2d markerCenterPosition: directionToPosition(direction, markerMaxDistance)
        property vector2d markerTargetPosition: active
            ? Qt.vector2d(mouseArea.mouseX, mouseArea.mouseY)
            : _private.centerPoint

        function positionToDirection(position, maxLength)
        {
            var vector = position.minus(centerPoint)
            var aspect = 1 / Math.max(vector.length(), maxLength)
            return vector.times(aspect)
        }

        function directionToPosition(direction, maxLength, normalize)
        {
            if (normalize)
                direction = direction.normalized()
            var vector = direction.times(maxLength)
            return vector.plus(centerPoint)
        }

        function fuzzyIsNull(value)
        {
            var eps = 0.000001
            return value > -eps && value < eps
        }

        function getAngle(vector)
        {
            var horizontalVector = Qt.vector2d(1, 0)

            var normalized = vector.normalized()
            var cosAlpha = normalized.dotProduct(horizontalVector)
            var sign = fuzzyIsNull(normalized.y) ? 1 : Math.abs(normalized.y) / normalized.y
            var result = Math.acos(cosAlpha) * sign
            return result
        }

        function getRadialVector(radius, angle)
        {
            return Qt.vector2d(radius * Math.cos(angle), radius * Math.sin(angle))
        }
    }
}

