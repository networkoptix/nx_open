import QtQuick 2.6
import Nx 1.0
import Nx.Core.Items 1.0
import nx.client.core 1.0

Item
{
    id: controller

    property alias motionProvider: mediaPlayerMotionProvider
    property bool motionSearchMode: false
    property string motionFilter:
    {
        if (!motionSearchMode)
            return ""

        return d.hasFinishedCustomRoi
            ? d.getMotionFilter(d.customTopLeftRelative, d.customBottomRightRelative)
            : d.getMotionFilter(d.defaultTopLeftRelative, d.defaultBottomRightRelative)
    }

    property int cameraRotation: 0
    property Item viewport: null

    readonly property bool customRoiExists: d.hasFinishedCustomRoi || drawingCustomRoi
    readonly property bool drawingCustomRoi:
    {
        var result = d.customInitialRelative
            && d.nearPositions(d.customInitialRelative, d.customFirstPoint)
        return result ? true : false
    }

    function clearCustomRoi()
    {
        d.customFirstPoint = undefined
        d.customSecondPoint = undefined
        updateDefaultRoi()
    }

    function updateDefaultRoi()
    {
        if (!viewport)
            return

        if (controller.customRoiExists)
        {
            d.defaultTopLeftRelative = undefined
            d.defaultBottomRightRelative = undefined
            return
        }

        var first = viewport.mapToItem(controller, 0, 0)
        var second = viewport.mapToItem(controller, viewport.width, viewport.height)
        var viewportRect = d.getRectangle(first, second);
        var cameraRect = Qt.rect(0, 0, width, height);

        var rect = d.intersection(viewportRect, cameraRect)
        if (rect)
        {
            d.defaultTopLeftRelative = d.toRelative(Qt.vector2d(rect.left, rect.top))
            d.defaultBottomRightRelative = d.toRelative(Qt.vector2d(rect.right, rect.bottom))
        }
        else
        {
            d.defaultTopLeftRelative = undefined
            d.defaultBottomRightRelative = undefined
        }
    }

    function handleLongPressed()
    {
        d.customFirstPoint = d.customInitialRelative
        d.customSecondPoint = d.customInitialRelative
        updateDefaultRoi()
    }

    function handlePressed(pos)
    {
        if (pressFilterTimer.running)
            return // We don't allow to start long tap after fast clicks (double click, for example).

        pressFilterTimer.restart()
        d.customInitialRelative = d.toRelative(pos)
        pressAndHoldTimer.restart()
    }

    function handleReleased()
    {
        d.customInitialRelative = undefined
        pressAndHoldTimer.stop()
    }

    function handleCancelled()
    {
        pressAndHoldTimer.stop()
        d.customInitialRelative = undefined
    }

    function handlePositionChanged(pos)
    {
        if (drawingCustomRoi)
            d.customSecondPoint = d.toRelative(pos)
        else if (!d.nearPositions(d.customInitialRelative, d.toRelative(pos)))
            handleCancelled()
    }

    onCustomRoiExistsChanged:
    {
        if (customRoiExists)
            motionSearchMode = true
    }

    onMotionSearchModeChanged:
    {
        if (!motionSearchMode)
            clearCustomRoi()
    }

    onCameraRotationChanged:
    {
        updateDefaultRoi()
    }

    Component.onCompleted: updateDefaultRoi()

    MaskedUniformGrid
    {
        // TODO: turn off motion area data gethering when not visible.
        visible: controller.motionSearchMode

        anchors.fill: parent

        opacity: 1
        cellCountX: 44
        cellCountY: 32
        color: "red"

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

    // Test control. Represents simple preloder for the first point
    Circle
    {
        id: tapPreloader

        centerPoint: d.fromRelative(d.customInitialRelative)

        border.width: 1
        border.color: d.lineColor
        color: "transparent"
        visible: d.customInitialRelative ? true : false
        radius: visible ? 30 : 0
        Behavior on width
        {
            NumberAnimation
            {
                duration: pressAndHoldTimer.interval
                easing.type: Easing.InOutQuad
            }
        }
    }

    MotionRoi
    {
        id: customRoiMarker

        drawing: controller.drawingCustomRoi
        roiColor: d.lineColor
        shadowColor: d.shadowColor
        startPoint: d.fromRelative(d.customFirstPoint)
        endPoint: d.fromRelative(d.customSecondPoint)
        visible: controller.motionSearchMode && controller.customRoiExists
    }

    // TODO: remove me, this is just for tests
    MotionRoi
    {
        id: defaultRoi

        roiColor: d.lineColor
        shadowColor: d.shadowColor
        startPoint: d.fromRelative(d.defaultTopLeftRelative)
        endPoint: d.fromRelative(d.defaultBottomRightRelative)
        visible: controller.motionSearchMode && d.defaultTopLeftRelative && d.defaultBottomRightRelative
            ? true :false
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

        readonly property color lineColor: ColorTheme.contrast1
        readonly property color shadowColor: ColorTheme.transparent(ColorTheme.base1, 0.2)

        property var customInitialRelative
        property var customFirstPoint
        property var customSecondPoint

        property point customTopLeftRelative:
        {
            if (!customFirstPoint || !customSecondPoint)
                return Qt.point(0, 0)

            return Qt.point(
                Math.min(customFirstPoint.x, customSecondPoint.x),
                Math.min(customFirstPoint.y, customSecondPoint.y))
        }

        property point customBottomRightRelative:
        {
            if (!customFirstPoint || !customSecondPoint)
                return Qt.point(0, 0)

            return Qt.point(
                Math.max(customFirstPoint.x, customSecondPoint.x),
                Math.max(customFirstPoint.y, customSecondPoint.y))
        }

        property var defaultTopLeftRelative
        property var defaultBottomRightRelative

        readonly property bool hasFinishedCustomRoi:
            !controller.drawingCustomRoi && customFirstPoint && customSecondPoint
                ? true
                : false

        function toRelative(absolute)
        {
            return Qt.vector2d(absolute.x / controller.width, absolute.y / controller.height)
        }

        function fromRelative(relative)
        {
            return relative
                ? Qt.point(relative.x * controller.width, relative.y * controller.height)
                : Qt.point(0, 0)
        }

        function nearPositions(first, second)
        {
            return first && second && first.minus(second).length() < 0.01 ? true : false
        }

        function getMotionFilter(topLeft, bottomRight)
        {
            if (!topLeft || !bottomRight)
                return "";

            var horizontalRange = 43
            var verticalRange = 31

            var left = Math.floor(topLeft.x * horizontalRange)
            var right = Math.ceil(bottomRight.x * horizontalRange)
            var top = Math.floor(topLeft.y * verticalRange)
            var bottom = Math.ceil(bottomRight.y * verticalRange)

            return "[[{\"x\": %1, \"y\": %2, \"width\": %3, \"height\": %4}]]"
                .arg(left).arg(top).arg(right - left + 1).arg(bottom - top + 1)
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
    }
}

