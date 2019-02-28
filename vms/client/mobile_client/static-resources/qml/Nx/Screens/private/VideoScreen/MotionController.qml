import QtQuick 2.6
import Nx 1.0
import Nx.Core.Items 1.0
import nx.client.core 1.0
import "utils.js" as Utils
Item
{
    id: controller

    property bool allowDrawing: true
    property int cameraRotation: 0
    property Item viewport: null

    property bool motionSearchMode: false
    property alias motionProvider: mediaPlayerMotionProvider
    property string motionFilter

    readonly property bool customRoiExists: d.hasFinishedCustomRoi || drawingRoi
    readonly property bool drawingRoi: allowDrawing && d.toBool(
        d.customInitialPoint && d.selectionRoi && d.selectionRoi.expandingFinished)

    signal requestDrawing()
    signal emptyRoiCleared()

    onAllowDrawingChanged:
    {
        if (allowDrawing && d.customInitialPoint && d.selectionRoi)
            Utils.executeLater(function() { d.selectionRoi.start() }, this)
    }

    function clearCustomRoi()
    {
        if (d.customRoi)
            d.customRoi.hide()

        d.customRoi = null
        d.selectionRoi = null

        d.customFirstPoint = undefined
        d.customSecondPoint = undefined
        updateDefaultRoi()
    }

    function updateDefaultRoi()
    {
        if (!controller.motionSearchMode || d.hasFinishedCustomRoi)
            return

        var first = viewport.mapToItem(controller, 0, 0)
        var second = viewport.mapToItem(controller, viewport.width, viewport.height)
        var viewportRect = d.getRectangle(first, second);
        var cameraRect = Qt.rect(0, 0, width, height);

        var rect = d.intersection(viewportRect, cameraRect)
        d.setRoiPoints(
            rect ? d.toRelative(Qt.vector2d(rect.left, rect.top)): undefined,
            rect ? d.toRelative(Qt.vector2d(rect.right, rect.bottom)) : undefined)
    }

    function handleLongPressed()
    {
        if (!controller.allowDrawing)
            controller.requestDrawing()
    }

    function handlePositionChanged(pos)
    {
        var relativePos = d.toRelative(pos)
        if (drawingRoi)
        {
            d.customSecondPoint = relativePos
            d.selectionRoi.endPoint = Qt.binding(function() { return d.fromRelative(relativePos) })
            d.selectionRoi.singlePoint = d.nearPositions(d.customFirstPoint, d.customSecondPoint)
        }
        else if (!d.nearPositions(d.customInitialPoint, relativePos))
        {
            handleCancelled()
        }
    }

    function handlePressed(pos)
    {
        if (pressFilterTimer.running)
            return // We don't allow to start long tap after fast clicks (double click, for example).

        pressFilterTimer.restart()
        var relativePos = d.toRelative(pos)
        d.customInitialPoint = relativePos
        pressAndHoldTimer.restart()

        d.selectionRoi = d.customRoi == firstRoi
            ? secondRoi
            : firstRoi

        d.selectionRoi.startPoint = Qt.binding(function() { return d.fromRelative(relativePos) })
        d.selectionRoi.endPoint = Qt.binding(function() { return d.fromRelative(relativePos) })
        d.selectionRoi.singlePoint = true
        if (allowDrawing)
            d.selectionRoi.start()
    }

    function handleReleased()
    {
        d.handleMouseReleased()
    }

    function handleCancelled()
    {
        d.handleMouseReleased()
    }

    RoundedMask
    {
        // TODO: turn off motion area data gethering when not visible.
        visible: controller.motionSearchMode

        anchors.fill: parent

        opacity: 1
        cellCountX: 44
        cellCountY: 32
        fillColor: ColorTheme.transparent(ColorTheme.red_l2, 0.15)
        lineColor: ColorTheme.transparent(ColorTheme.red_l2, 0.5)

        MediaPlayerMotionProvider
        {
            id: mediaPlayerMotionProvider

            onMotionMaskChanged: motionMaskItem.motionMask = mediaPlayerMotionProvider.motionMask(0)
        }

        maskTextureProvider: MotionMaskItem
        {
            id: motionMaskItem
        }
    }

    MotionRoi
    {
        id: firstRoi

        roiColor: d.lineColor
        shadowColor: d.shadowColor
        drawing: controller.drawingRoi && d.selectionRoi == firstRoi
    }

    MotionRoi
    {
        id: secondRoi

        roiColor: d.lineColor
        shadowColor: d.shadowColor
        drawing: controller.drawingRoi && d.selectionRoi == secondRoi
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

    onMotionSearchModeChanged:
    {
        if (!motionSearchMode)
        {
            clearCustomRoi()
            controller.motionFilter = ""
        }
        else if (!d.customInitialPoint)
        {
            updateDefaultRoi()
        }
    }

    onDrawingRoiChanged:
    {
        if (drawingRoi)
        {
            d.customFirstPoint = d.customInitialPoint
            d.customSecondPoint = d.customInitialPoint
            d.selectionRoi.show()
            if (d.customRoi)
                d.customRoi.hide()

            makeShortVibration()
        }
        else
        {
            d.customRoi = d.selectionRoi
            d.selectionRoi = null
            d.setRoiPoints(d.customFirstPoint, d.customSecondPoint)
        }
    }

    onCameraRotationChanged: updateDefaultRoi()

    QtObject
    {
        id: d

        property MotionRoi selectionRoi: null
        property MotionRoi customRoi: null

        readonly property color lineColor: ColorTheme.contrast1
        readonly property color shadowColor: ColorTheme.transparent(ColorTheme.base1, 0.2)
        readonly property bool hasFinishedCustomRoi: toBool(
            !controller.drawingRoi && customFirstPoint && customSecondPoint)

        property var customInitialPoint
        property var customFirstPoint
        property var customSecondPoint

        property var currentFirstPoint
        property var currentSecondPoint

        function toRelative(absolute)
        {
            return Qt.point(absolute.x / controller.width, absolute.y / controller.height)
        }

        function fromRelative(relative)
        {
            return relative
                ? Qt.point(relative.x * controller.width, relative.y * controller.height)
                : Qt.point(0, 0)
        }

        function nearPositions(first, second)
        {
            if (!first || !second)
                return false

            var firstVector = Qt.vector2d(first.x, first.y)
            var secondVector = Qt.vector2d(second.x, second.y)
            return firstVector.minus(secondVector).length() < 0.03
        }

        function getMotionFilter(first, second)
        {
            var result = { filter: "", correctBounds: true }

            if (!first|| !second)
                return result

            var topLeft = Qt.point(
                Math.min(first.x, second.x),
                Math.min(first.y, second.y))
            var bottomRight = Qt.point(
                Math.max(first.x, second.x),
                Math.max(first.y, second.y))

            var horizontalRange = 44
            var verticalRange = 32
            var maxHorizontalValue = horizontalRange - 1
            var maxVerticalValue = verticalRange - 1

            var left = Math.floor(topLeft.x * horizontalRange)
            var right = Math.floor(bottomRight.x * horizontalRange)
            var top = Math.floor(topLeft.y * verticalRange)
            var bottom = Math.floor(bottomRight.y * verticalRange)

            result.correctBounds =
                left < horizontalRange
                && right >= 0
                && top < verticalRange
                && bottom >= 0

            left = result.correctBounds ? Math.max(left, 0) : 0
            right = result.correctBounds ? Math.min(right, maxHorizontalValue) : maxHorizontalValue
            top = result.correctBounds ? Math.max(top, 0) : 0
            bottom = result.correctBounds ? Math.min(bottom, maxVerticalValue) : maxVerticalValue

            result.filter = JSON.stringify([[
                { "x": left, "y": top, "width": right - left + 1, "height": bottom - top + 1 }
                ]])

            return result
        }

        function getRectangle(first, second)
        {
            var width = Math.abs(second.x - first.x)
            var height = Math.abs(second.y - first.y)
            var x = Math.min(first.x, second.x)
            var y = Math.min(first.y, second.y)
            return Qt.rect(x, y, width, height)
        }

        function intersection(first, second)
        {
            var left = Math.max(first.left, second.left)
            var right = Math.min(first.right, second.right)
            var bottom = Math.min(first.bottom, second.bottom)
            var top = Math.max(first.top, second.top)

            return left < right && top < bottom
                ? Qt.rect(left, top, right - left, bottom - top)
                : undefined
        }

        function toBool(value)
        {
            return !!value
        }

        function handleMouseReleased()
        {
            d.customInitialPoint = undefined
            if (d.selectionRoi)
            {
                d.selectionRoi.hide()
                d.selectionRoi = null
            }
            pressAndHoldTimer.stop()
        }

        function setRoiPoints(first, second)
        {
            currentFirstPoint = first
            currentSecondPoint = second

            var filterResult = getMotionFilter(first, second)
            controller.motionFilter = filterResult.filter
            if (!filterResult.correctBounds)
            {
                controller.clearCustomRoi()
                controller.emptyRoiCleared()
            }
        }
    }
}
