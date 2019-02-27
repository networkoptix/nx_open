import QtQuick 2.6
import Nx 1.0

Object
{
    id: controller

    property bool enabled: true

    property PinchArea pinchArea: null
    property MouseArea mouseArea: null
    property Flickable flickable: null

    property var doubleTapStartCheckFunction
    property bool allowCompositeEvents: false

    signal clicked()
    signal doubleClicked(real mouseX, real mouseY)

    property bool draggingGesture: false

    Object
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

            onPinchStarted: pinchAreaHandler.startPinch()
            onPinchUpdated: pinchAreaHandler.updatePinch(pinch.center, pinch.previousCenter, pinch.scale)
            onPinchFinished: pinchAreaHandler.finishPinch()
        }
    }

    Object
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
            interval: 400
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

            onPositionChanged:
            {
                if (target.pressed)
                    controller.draggingGesture = true

                if (!mouseAreaHandler.doubleTapDownPos)
                    return

                var currentVector = Qt.vector2d(
                    mouseAreaHandler.doubleTapDownPos.x - mouseX,
                    mouseAreaHandler.doubleTapDownPos.y - mouseY)

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
                pinchAreaHand.updatePinch(mouseAreaHandler.doubleTapDownPos, mouseAreaHandler.doubleTapDownPos, targetScale)
            }

            onDoubleClicked:
            {
                delayedClickTimer.stop()
                doubleClickFilterTimer.restart();

                var mousePosition = Qt.point(mouse.x, mouse.y)
                var acceptDoubleTap = controller.doubleTapStartCheckFuncion
                    && controller.doubleTapStartCheckFuncion(mousePosition)

                if (acceptDoubleTap)
                    mouseAreaHandler.doubleTapDownPos = mousePosition
            }

            onPressed:
            {
                mouseAreaHandler.downPos = Qt.point(mouse.x, mouse.y)
                clickFilterTimer.restart()
            }

            onCanceled:
            {
                controller.draggingGesture = false

                delayedClickTimer.stop()
                mouseAreaHandler.doubleTapScaleMode = false
                mouseAreaHandler.doubleTapDownPos = undefined
            }

            onReleased:
            {
                controller.draggingGesture = false

                if (clickFilterTimer.running
                    && !doubleClickFilterTimer.running
                    && Utils.nearPositions(mouseAreaHandler.downPos,
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

            onClicked:
            {
                mouse.accepted = true
            }

            onWheel:
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

