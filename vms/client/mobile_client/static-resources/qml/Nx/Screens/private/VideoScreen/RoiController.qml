// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick

import Nx.Core

// Lets the user draw a custom region of interest over the video: a long press starts drawing,
// a drag sets the region, the release finishes it. The press handlers are called by the owner,
// with positions in the coordinates of this item.
Item
{
    id: controller

    readonly property bool customRoiExists: !!d.firstPoint
    readonly property bool drawingRoi: d.drawing

    // Relative to the item size. The whole frame if there's no custom region.
    readonly property rect customRoi: d.finishedRoi ?? Qt.rect(0, 0, 1, 1)

    // Motion search filter of the custom region, empty if there's none.
    readonly property string motionFilter: d.finishedRoi ? d.motionFilter(d.finishedRoi) : ""

    signal emptyRoiCleared()

    function clearCustomRoi()
    {
        d.cancelPress()

        d.firstPoint = undefined
        d.secondPoint = undefined
        d.finishedRoi = undefined
    }

    function handlePressed(pos)
    {
        // The previous press may have never got its release.
        if (d.initialPoint)
            handleCancelled()

        // A long press must not start right after fast clicks (a double click, for example).
        if (pressFilterTimer.running)
            return

        pressFilterTimer.restart()
        d.initialPoint = d.toRelative(pos)
        longPressTimer.restart()
    }

    function handlePositionChanged(pos)
    {
        if (!d.initialPoint)
            return

        const point = d.toRelative(pos)

        if (d.drawing)
        {
            d.secondPoint = point
            d.crosshairPoint = point
        }
        else if (!CoreUtils.nearPositions(d.initialPoint, point, d.kMaxDistance))
        {
            d.cancelPress() //< It's a drag, not a long press.
        }
    }

    function handleReleased()
    {
        const wasDrawing = d.drawing
        d.cancelPress()

        if (wasDrawing)
            d.finishDrawing()
    }

    // The region being drawn is finished as on a release.
    function handleCancelled()
    {
        handleReleased()
    }

    // A disabled controller doesn't get the input events, so the current press would never end.
    onEnabledChanged:
    {
        if (!enabled)
            handleCancelled()
    }

    // Indicates a long press, and the finger position while drawing.
    RoiCrosshair
    {
        id: drawingCrosshair

        color: d.lineColor
        shadowColor: d.shadowColor
        centerPoint: d.fromRelative(d.crosshairPoint)
        expanded: d.longPressed || d.drawing

        onExpandingFinishedChanged:
        {
            if (expandingFinished && d.longPressed && !d.drawing)
                d.startDrawing()
        }
    }

    RoiMarker
    {
        id: marker

        visible: controller.customRoiExists
        startPoint: d.fromRelative(d.firstPoint)
        endPoint: d.fromRelative(d.secondPoint)
        drawing: d.drawing
        singlePoint: CoreUtils.nearPositions(d.firstPoint, d.secondPoint, d.kMaxDistance)

        roiColor: d.lineColor
        shadowColor: d.shadowColor
    }

    Timer
    {
        id: longPressTimer

        interval: 600

        onTriggered:
        {
            d.longPressed = true
            d.crosshairPoint = d.initialPoint
        }
    }

    Timer
    {
        id: pressFilterTimer

        interval: longPressTimer.interval
    }

    QtObject
    {
        id: d

        readonly property int kFilterHorizontalRange: 44
        readonly property int kFilterVerticalRange: 32

        readonly property real kMaxDistance: 0.05
        readonly property color lineColor: ColorTheme.colors.light1
        readonly property color shadowColor: ColorTheme.transparent(ColorTheme.colors.dark1, 0.2)

        // All points are relative to the item size.

        property var initialPoint //< Of the current press.
        property bool longPressed: false
        property bool drawing: false
        property var crosshairPoint

        // Of the region being drawn or the finished one.
        property var firstPoint
        property var secondPoint

        // A rect, updated only when the drawing is finished.
        property var finishedRoi

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

        function startDrawing()
        {
            drawing = true
            firstPoint = initialPoint
            secondPoint = initialPoint

            windowContext.ui.windowHelpers.makeShortVibration()
        }

        function finishDrawing()
        {
            const roi = Qt.rect(
                Math.min(firstPoint.x, secondPoint.x),
                Math.min(firstPoint.y, secondPoint.y),
                Math.abs(secondPoint.x - firstPoint.x),
                Math.abs(secondPoint.y - firstPoint.y))

            // The region may be drawn outside of the frame entirely.
            const intersectsFrame = roi.x < 1 && roi.x + roi.width >= 0
                && roi.y < 1 && roi.y + roi.height >= 0

            if (intersectsFrame)
            {
                finishedRoi = roi
            }
            else
            {
                controller.clearCustomRoi()
                controller.emptyRoiCleared()
            }
        }

        function cancelPress()
        {
            longPressTimer.stop()
            initialPoint = undefined
            longPressed = false
            drawing = false
        }

        function motionFilter(roi)
        {
            const left = Math.max(Math.floor(roi.x * kFilterHorizontalRange), 0)
            const right = Math.min(Math.floor((roi.x + roi.width) * kFilterHorizontalRange),
                kFilterHorizontalRange - 1)
            const top = Math.max(Math.floor(roi.y * kFilterVerticalRange), 0)
            const bottom = Math.min(Math.floor((roi.y + roi.height) * kFilterVerticalRange),
                kFilterVerticalRange - 1)

            return JSON.stringify([[
                { "x": left, "y": top, "width": right - left + 1, "height": bottom - top + 1 }
                ]])
        }
    }
}
