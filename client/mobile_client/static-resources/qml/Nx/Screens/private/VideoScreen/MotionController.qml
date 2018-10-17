import QtQuick 2.6

Item
{
    id: controller

    property bool motionSearchMode: false
    property Item viewport: null

    readonly property bool hasCustomRoi:
        d.customFirstRelativePoint && d.customSecondRelativePoint ? true : false

    readonly property bool drawingRoi:
    {
        if (d.customInitialRelativePoint
            && d.nearPositions(d.customInitialRelativePoint, d.customFirstRelativePoint))
        {
            return true
        }
        return false
    }

    function clearCustomRoi()
    {
        d.customFirstRelativePoint = undefined
        d.customSecondRelativePoint = undefined
        updateDefaultRoi()
    }

    function updateDefaultRoi()
    {
        if (!viewport)
            return

        if (controller.hasCustomRoi)
        {
            d.defaultTopLeftRelative = undefined
            d.defaultBottomRightRelative = undefined
            return
        }

        var first = viewport.mapToItem(controller, 0, 0)
        var second = viewport.mapToItem(controller, viewport.width, viewport.height)

        var topLeft = Qt.point(Math.min(first.x, second.x), Math.min(first.y, second.y))
        var bottomRight = Qt.point(Math.max(first.x, second.x), Math.max(first.y, second.y))
        if (topLeft.x < 0)
            topLeft.x = 0
        if (topLeft.y < 0)
            topLeft.y = 0
        if (bottomRight.x >= controller.width)
            bottomRight.x = controller.width - 1
        if (bottomRight.y >= controller.height)
            bottomRight.y = controller.height - 1

        d.defaultTopLeftRelative = d.toRelative(topLeft)
        d.defaultBottomRightRelative = d.toRelative(bottomRight)
    }

    function handleLongPressed()
    {
        d.customFirstRelativePoint = d.customInitialRelativePoint
        d.customSecondRelativePoint = d.customInitialRelativePoint
        updateDefaultRoi()
    }

    function handlePressed(pos)
    {
        if (pressFilterTimer.running)
            return // We don't allow to start long tap after fast clicks (double click, for example).

        pressFilterTimer.restart()
        d.customInitialRelativePoint = d.toRelative(pos)
        pressAndHoldTimer.restart()
    }

    function handleReleased()
    {
        d.customInitialRelativePoint = undefined
        pressAndHoldTimer.stop()
    }

    function handleCancelled()
    {
        pressAndHoldTimer.stop()
        d.customInitialRelativePoint = undefined
    }

    function handlePositionChanged(pos)
    {
        if (drawingRoi)
            d.customSecondRelativePoint = d.toRelative(pos)
        else
            handleCancelled()
    }

    onHasCustomRoiChanged:
    {
        if (hasCustomRoi)
            motionSearchMode = true
    }

    Component.onCompleted: updateDefaultRoi()

    // Test control. Represents simple preloder for the first point
    Rectangle
    {
        id: tapPreloader

        readonly property vector2d center: d.fromRelative(d.customInitialRelativePoint)
        x: center.x - width / 2
        y: center.y - height / 2

        border.width: 2
        border.color: "red"
        radius: width / 2
        color: "transparent"
        visible: d.customInitialRelativePoint ? true : false
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
        border.width: 5
        border.color: "yellow"
        color: singlePoint ? "red" : "transparent"

        property vector2d first: d.fromRelative(controller.hasCustomRoi
            ? d.customFirstRelativePoint
            : d.defaultTopLeftRelative)
        property vector2d second: d.fromRelative(controller.hasCustomRoi
            ? d.customSecondRelativePoint
            : d.defaultBottomRightRelative)

        x: getCoordinate(first.x, second.x, width)
        y: getCoordinate(first.y, second.y, height)

        width: getLength(first.x, second.x)
        height: getLength(first.y, second.y)
        visible: controller.motionSearchMode

        readonly property bool singlePoint:
            d.nearPositions(d.customFirstRelativePoint, d.customSecondRelativePoint)

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

        interval: 800
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

        property var customInitialRelativePoint
        property var customFirstRelativePoint
        property var customSecondRelativePoint

        property var defaultTopLeftRelative
        property var defaultBottomRightRelative

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
            return first && second && first.minus(second).length() < 0.005 ? true : false
        }
    }
}
