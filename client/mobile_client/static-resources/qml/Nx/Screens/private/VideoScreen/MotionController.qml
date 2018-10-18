import QtQuick 2.6
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
            && d.nearPositions(d.customInitialRelative, d.customTopLeftRelative)
        return result ? true : false
    }

    function clearCustomRoi()
    {
        d.customTopLeftRelative = undefined
        d.customBottomRightRelative = undefined
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
        d.customTopLeftRelative = d.customInitialRelative
        d.customBottomRightRelative = d.customInitialRelative
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
        if (d.customTopLeftRelative && d.customBottomRightRelative)
        {
            var rect = d.getRectangle(d.customTopLeftRelative, d.customBottomRightRelative)
            d.customTopLeftRelative = Qt.vector2d(rect.left, rect.top)
            d.customBottomRightRelative = Qt.vector2d(rect.right, rect.bottom)
        }

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
            d.customBottomRightRelative = d.toRelative(pos)
        else
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
        anchors.fill: parent

        opacity: 0.5
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
    Rectangle
    {
        id: tapPreloader

        readonly property vector2d center: d.fromRelative(d.customInitialRelative)
        x: center.x - width / 2
        y: center.y - height / 2

        border.width: 2
        border.color: "red"
        radius: width / 2
        color: "transparent"
        visible: d.customInitialRelative ? true : false
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

    // Test roi for now.
    MotionRoi
    {
        id: customRoiMarker

        topLeft: d.fromRelative(d.customTopLeftRelative)
        bottomRight: d.fromRelative(d.customBottomRightRelative)
        visible: controller.motionSearchMode && controller.customRoiExists
    }

    // TODO: remove me, this is just for tests
    MotionRoi
    {
        id: defaultRoi

        topLeft: d.fromRelative(d.defaultTopLeftRelative)
        bottomRight: d.fromRelative(d.defaultBottomRightRelative)
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

        property var customInitialRelative
        property var customTopLeftRelative
        property var customBottomRightRelative

        property var defaultTopLeftRelative
        property var defaultBottomRightRelative

        readonly property bool hasFinishedCustomRoi:
            !controller.drawingCustomRoi && customTopLeftRelative && customBottomRightRelative
                ? true
                : false

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

