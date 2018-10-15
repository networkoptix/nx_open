import QtQuick 2.6

Item
{
    id: controller

    property bool motionSearchMode: false

    readonly property bool drawingRoi:
    {
        return d.initialRelative && d.nearPositions(d.initialRelative, d.firstRelative)
            ? true
            : false
    }

    function handleLongPressed()
    {
        d.firstRelative = d.initialRelative
        d.secondRelative = d.initialRelative
    }

    function handlePressed(pos)
    {
        if (pressFilterTimer.running)
            return // We don't allow to start long tap after fast clicks (double click, for example).

        pressFilterTimer.restart()
        d.initialRelative = d.toRelative(pos)
        pressAndHoldTimer.restart()
    }

    function handleReleased()
    {
        pressAndHoldTimer.stop()
        d.initialRelative = undefined
    }

    function handleCancelled()
    {
        pressAndHoldTimer.stop()
        d.initialRelative = undefined
    }

    function handlePositionChanged(pos)
    {
        if (drawingRoi)
            d.secondRelative = d.toRelative(pos)
        else
            handleCancelled()
    }

    // Test control. Represents simple preloder for the first point
    Rectangle
    {
        id: tapPreloader

        x: d.initial.x - width / 2
        y: d.initial.y - height / 2

        border.width: 2
        border.color: "red"
        radius: width / 2
        color: "transparent"
        visible: d.initialRelative ? true : false
        height: width
        width: visible ? 50 : 0
        Behavior on width
        {
            NumberAnimation
            {
                duration: pressAndHoldTimer.interval
                easing.type: Easing.InOutQuad
            }
        }
    }

    // Test control. Represents simple ROI
    Rectangle
    {
        id: roiRectangle
        border.width: 2
        border.color: "yellow"
        color: singlePoint ? "red" : "transparent"

        visible: d.firstRelative && d.secondRelative ? true : false

        x: visible ? getCoordinate(d.first.x, d.second.x, width) : 0
        y: visible ? getCoordinate(d.first.y, d.second.y, height) : 0

        width: visible ? getLength(d.first.x, d.second.x) : 0
        height: visible ? getLength(d.first.y, d.second.y) : 0

        readonly property bool singlePoint:
            visible && d.nearPositions(d.firstRelative, d.secondRelative)

        function getCoordinate(firstValue, secondValue, length)
        {
            return singlePoint ? firstValue - length / 2 : Math.min(firstValue, secondValue)
        }

        function getLength(firstValue, secondValue)
        {
            return singlePoint ? 10 : Math.abs(secondValue - firstValue)
        }
    }

    Timer
    {
        id: pressAndHoldTimer

        interval: 600
        onTriggered: controller.handleLongPressed()
    }

    Timer
    {
        id: pressFilterTimer
        interval: pressAndHoldTimer.interval
    }

    QtObject
    {
        id: d

        property var initialRelative
        property var firstRelative
        property var secondRelative

        property vector2d initial: fromRelative(initialRelative)
        property vector2d first: fromRelative(firstRelative)
        property vector2d second: fromRelative(secondRelative)

        function toRelative(absolute)
        {
            return Qt.vector2d(absolute.x / controller.width, absolute.y / controller.height)
        }

        function fromRelative(relative)
        {
            return relative
                ? Qt.vector2d(relative.x * controller.width, relative.y * controller.height)
                : Qt.vector2d(0, 0)
        }

        function nearPositions(first, second)
        {
            return first && second && first.minus(second).length() < 0.005
        }
    }
}
