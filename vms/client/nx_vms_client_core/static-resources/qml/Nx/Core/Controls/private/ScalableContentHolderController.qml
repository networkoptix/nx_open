// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.14

import Nx.Core 1.0

NxObject
{
    id: controller

    property bool enabled: true

    property PinchArea pinchArea: null
    property MouseArea mouseArea: null
    property Flickable flickable: null

    property var doubleTapStartCheckFunction
    property bool allowCompositeEvents: true

    signal clicked()
    signal doubleClicked(real mouseX, real mouseY)

    property bool draggingGesture: false

    readonly property bool scaling:
        mouseAreaHandler.doubleTapScaleMode
        || (mouseAreaHandler.doubleTapDownPos ? true : false)

    NxObject
    {
        id: pinchAreaHandler

        property real initialWidth: 0
        property real initialHeight: 0
        property real initialContentX: 0
        property real initialContentY: 0

        function startPinch()
        {
            boundsAnimation.stop()
            initialWidth = flickable.contentWidth
            initialHeight = flickable.contentHeight
            initialContentX = flickable.contentX
            initialContentY = flickable.contentY
            flickable.leftMargin = 0
            flickable.topMargin = 0
            flickable.contentX = initialContentX
            flickable.contentY = initialContentY
            flickable.fixMargins()
        }

        function finishPinch()
        {
            flickable.animating = true
            flickable.fixMargins()
            flickable.animateToBounds()
        }

        function updatePinch(center, previousCenter, targetScale)
        {
            flickable.contentX += previousCenter.x - center.x
            flickable.contentY += previousCenter.y - center.y

            var cx = center.x + flickable.contentX
            var cy = center.y + flickable.contentY

            var w = initialWidth * targetScale
            var h = initialHeight * targetScale

            if (w > maxContentWidth)
                targetScale = maxContentWidth / initialWidth

            w = initialWidth * targetScale
            h = initialHeight * targetScale

            if (h > maxContentHeight)
                targetScale = Math.min(targetScale, maxContentHeight / initialHeight)

            if (targetScale < 0.1)
                targetScale = 0.1

            flickable.resizeContent(initialWidth * targetScale, initialHeight * targetScale, Qt.point(cx, cy))
        }

        Connections
        {
            target: pinchArea
            enabled: pinchArea && flickable && controller.enabled

            function onPinchStarted()
            {
                pinchAreaHandler.startPinch()
            }

            function onPinchUpdated(pinch)
            {
                pinchAreaHandler.updatePinch(pinch.center, pinch.previousCenter, pinch.scale)
            }

            function onPinchFinished()
            {
                pinchAreaHandler.finishPinch()
            }
        }
    }

    NxObject
    {
        id: mouseAreaHandler


        property var doubleTapDownPos: undefined
        property bool doubleTapScaleMode: false
        property point downPos

        Timer
        {
            id: delayedClickTimer

            interval: 300
            onTriggered: controller.clicked()
        }

        Timer
        {
            id: clickFilterTimer
            interval: 200
        }

        Timer
        {
            id: doubleClickFilterTimer
            interval: 300
        }

        onDoubleTapScaleModeChanged:
        {
            if (mouseAreaHandler.doubleTapScaleMode)
            {
                pinchAreaHandler.startPinch()
            }
            else
            {
                mouseAreaHandler.doubleTapDownPos = undefined
                pinchAreaHandler.finishPinch()
            }
        }

        Connections
        {
            target: mouseArea
            enabled: mouseArea && flickable && controller.enabled

            function onPositionChanged(mouse)
            {
                if (target.pressed && !mouseAreaHandler.doubleTapDownPos && mouseArea.pressed)
                    controller.draggingGesture = true

                if (!mouseAreaHandler.doubleTapDownPos)
                    return

                var currentVector = Qt.vector2d(
                    mouseAreaHandler.doubleTapDownPos.x - mouse.x,
                    mouseAreaHandler.doubleTapDownPos.y - mouse.y)

                if (!mouseAreaHandler.doubleTapScaleMode)
                {
                    var minDoubleTapStartLength = 15
                    if (currentVector.length() > minDoubleTapStartLength)
                        mouseAreaHandler.doubleTapScaleMode = controller.allowCompositeEvents
                }

                if (!mouseAreaHandler.doubleTapScaleMode)
                    return

                var sideSize = Math.min(flickable.width, flickable.height) / 2
                var scaleChange = 1 + Math.abs(currentVector.y / sideSize)
                var targetScale = currentVector.y > 0 ? 1 / scaleChange : scaleChange
                pinchAreaHandler.updatePinch(mouseAreaHandler.doubleTapDownPos, mouseAreaHandler.doubleTapDownPos, targetScale)
            }

            function onDoubleClicked(mouse)
            {
                delayedClickTimer.stop()
                doubleClickFilterTimer.restart();

                var mousePosition = Qt.point(mouse.x, mouse.y)
                var acceptDoubleTap = controller.doubleTapStartCheckFunction
                    && controller.doubleTapStartCheckFunction(mousePosition)

                if (acceptDoubleTap)
                    mouseAreaHandler.doubleTapDownPos = mousePosition
            }

            function onPressed(mouse)
            {
                mouseAreaHandler.downPos = Qt.point(mouse.x, mouse.y)
                clickFilterTimer.restart()
            }

            function onCanceled()
            {
                controller.draggingGesture = false

                delayedClickTimer.stop()
                mouseAreaHandler.doubleTapScaleMode = false
                mouseAreaHandler.doubleTapDownPos = undefined
            }

            function onReleased(mouse)
            {
                controller.draggingGesture = false

                if (clickFilterTimer.running
                    && !doubleClickFilterTimer.running
                    && CoreUtils.nearPositions(mouseAreaHandler.downPos,
                        Qt.point(mouse.x, mouse.y), mouseArea.drag.threshold))
                {

                    delayedClickTimer.restart()
                }
                else
                {
                    delayedClickTimer.stop()
                }

                mouseAreaHandler.doubleTapScaleMode = false
                mouseAreaHandler.doubleTapDownPos = undefined
                if (!doubleClickFilterTimer.running)
                    return

                doubleClickFilterTimer.stop()
                controller.doubleClicked(mouse.x, mouse.y)
            }

            function onClicked(mouse)
            {
                mouse.accepted = true
            }

            function onWheel(wheel)
            {
                var cx = wheel.x + flickable.contentX
                var cy = wheel.y + flickable.contentY

                var scale = wheel.angleDelta.y > 0 ? 1.1 : 0.9
                var w = flickable.contentWidth * scale
                var h = flickable.contentHeight * scale

                if (w > maxContentWidth)
                    scale = maxContentWidth / flickable.contentWidth
                if (h > maxContentHeight)
                    scale = Math.max(scale, maxContentHeight / flickable.contentHeight)

                flickable.animating = true
                flickable.fixMargins()
                flickable.resizeContent(flickable.contentWidth * scale, flickable.contentHeight * scale, Qt.point(cx, cy))
                flickable.animateToBounds()
            }
        }
    }
}
