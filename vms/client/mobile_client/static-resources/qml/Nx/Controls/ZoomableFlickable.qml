import QtQuick 2.6
import nx.client.core 1.0
import Nx 1.0

Item
{
    id: rootItem

    property string message

    property alias contentX: flick.contentX
    property alias contentY: flick.contentY
    property alias contentWidth: flick.contentWidth
    property alias contentHeight: flick.contentHeight

    default property alias data: contentItem.data

    property real minContentWidth: 0
    property real minContentHeight: 0

    property real maxContentWidth: Infinity
    property real maxContentHeight: Infinity

    property real allowedHorizontalMargin: 0
    property real allowedVerticalMargin: 0

    property real allowedLeftMargin: 0
    property real allowedRightMargin: 0
    property real allowedTopMargin: 0
    property real allowedBottomMargin: 0

    property bool allowCompositeEvents: true

    readonly property alias flickable: flick
    readonly property real contentScale: Geometry.scaleFactor(
        Qt.size(width, height), Qt.size(contentWidth, contentHeight))

    signal pressed(int mouseX, int mouseY)
    signal released()
    signal positionChanged(int mouseX, int mouseY)
    signal cancelled()
    signal clicked()
    signal doubleClicked(int mouseX, int mouseY)
    signal movementEnded()
    signal contentRectChanged()
    property bool moving: false

    property var doubleTapStartCheckFuncion

    function resizeContent(width, height, animate, forceSize)
    {
        flick.animateToSize(width, height, animate, forceSize)
    }

    Flickable
    {
        id: flick

        anchors.fill: parent

        topMargin: 0
        leftMargin: 0
        bottomMargin: 0
        rightMargin: 0

        readonly property bool allowOvershoot:
            allowedLeftMargin > 0
                || allowedRightMargin > 0
                || allowedTopMargin > 0
                || allowedBottomMargin > 0
        property bool animating: false

        flickableDirection: Flickable.HorizontalAndVerticalFlick
        boundsBehavior: allowOvershoot ? Flickable.DragOverBounds : Flickable.StopAtBounds

        onMovementEnded: rootItem.movementEnded()

        interactive:
            !mouseArea.doubleTapScaleMode
            && (mouseArea.doubleTapDownPos ? false : true)
            && rootItem.allowCompositeEvents

        Item
        {
            id: contentItem
            width: flick.contentWidth
            height: flick.contentHeight
        }

        /* Workaround for qt bug: top and left margins are ignored. */
        onContentXChanged:
        {
            fixContentX()
            rootItem.contentRectChanged()
        }
        onContentYChanged:
        {
            fixContentY()
            rootItem.contentRectChanged()
        }

        onOriginXChanged: rootItem.contentRectChanged()
        onOriginYChanged: rootItem.contentRectChanged()
        onContentHeightChanged: rootItem.contentRectChanged()
        onContentWidthChanged: rootItem.contentRectChanged()

        onLeftMarginChanged: fixContentX()
        onTopMarginChanged: fixContentY()

        function fixContentX()
        {
            if (pinchArea.pinch.active || allowOvershoot || animating)
                return

            var newContentX = contentX

            if (leftMargin > 0 && -newContentX > leftMargin)
                newContentX = -leftMargin

            if (rightMargin > 0 && -newContentX + contentWidth < width - rightMargin)
                newContentX = Math.min(newContentX, contentWidth - width + rightMargin)

            contentX = newContentX
        }

        function fixContentY()
        {
            if (pinchArea.pinch.active || allowOvershoot || animating)
                return

            var newContentY = contentY

            if (topMargin > 0 && -newContentY > topMargin)
                newContentY = -topMargin

            if (bottomMargin > 0 && -newContentY + contentHeight < height - bottomMargin)
                newContentY = Math.min(newContentY, contentHeight - height + bottomMargin)

            contentY = newContentY
        }

        function bindMargins()
        {
            var x = contentX
            var y = contentY

            leftMargin = Qt.binding(function() { return allowedLeftMargin })
            topMargin = Qt.binding(function() { return allowedTopMargin })
            rightMargin = Qt.binding(function() { return allowedRightMargin })
            bottomMargin = Qt.binding(function() { return allowedBottomMargin })

            contentX = x
            contentY = y
        }

        function fixMargins()
        {
            leftMargin = leftMargin
            topMargin = topMargin
            rightMargin = rightMargin
            bottomMargin = bottomMargin
        }

        function animateToBounds()
        {
            if (contentWidth < minContentWidth && contentHeight < minContentHeight)
            {
                animateToSize(minContentWidth, minContentHeight, true)
                return
            }

            /* Cannot use returnToBounds for smooth animation due to the mentioned Flickable bug. */
            animateToSize(contentWidth, contentHeight, true)
            /*
            boundsAnimation.stop()

            var x = contentX
            var y = contentY

            updateMargins()

            contentX = x
            contentY = y

            returnToBounds()
            */
        }

        function animateToSize(cw, ch, animate, forceSize)
        {
            boundsAnimation.stop()

            var w = contentWidth
            var h = contentHeight
            var x = contentX
            var y = contentY

            if (w > 0 && h > 0 && forceSize != true)
            {
                var scale = Math.min(cw / w, ch / h)
                w *= scale
                h *= scale
            }
            else
            {
                w = cw
                h = ch
            }

            if (-x + w < width - allowedRightMargin)
                x = w - width + allowedRightMargin
            if (-x > allowedLeftMargin)
                x = -allowedLeftMargin

            if (-y + h < rootItem.height - allowedBottomMargin)
                y = h - rootItem.height + allowedBottomMargin
            if (-y > allowedTopMargin)
                y = -allowedTopMargin

            if (animate)
            {
                setAnimationParameters(x, y , w, h, Easing.Linear)
                fixMargins()
                boundsAnimation.start()
            }
            else
            {
                contentWidth = w
                contentHeight = h
                contentX = x
                contentY = y

                flick.animating = false

                bindMargins()
                rootItem.movementEnded()
            }
        }

        function setAnimationParameters(x, y, width, height, easing)
        {
            widthAnimation.easing.type = easing
            heightAnimation.easing.type = easing
            xAnimation.easing.type = easing
            yAnimation.easing.type = easing

            widthAnimation.to = width
            heightAnimation.to = height
            xAnimation.to = x
            yAnimation.to = y
        }

        function animateTo(x, y, width, height, easing)
        {
            boundsAnimation.stop()
            setAnimationParameters(x,y, width, height, easing)
            boundsAnimation.start()
        }

        Component.onCompleted:
        {
            bindMargins()
        }

        ParallelAnimation
        {
            id: boundsAnimation

            property int duration: 250

            NumberAnimation
            {
                id: widthAnimation
                target: flick
                property: "contentWidth"
                duration: boundsAnimation.duration
            }
            NumberAnimation
            {
                id: heightAnimation
                target: flick
                property: "contentHeight"
                duration: boundsAnimation.duration
            }
            NumberAnimation
            {
                id: xAnimation
                target: flick
                property: "contentX"
                duration: boundsAnimation.duration
            }
            NumberAnimation
            {
                id: yAnimation
                target: flick
                property: "contentY"
                duration: boundsAnimation.duration
            }

            onStopped:
            {
                flick.animating = false
                flick.bindMargins()

                rootItem.movementEnded()
            }
        }

        onMovingChanged:
        {
            if (moving)
                boundsAnimation.stop()
        }
    }

    PinchArea
    {
        id: pinchArea

        parent: flick

        anchors.fill: flick

        property real initialWidth
        property real initialHeight
        property real initialContentX
        property real initialContentY

        function startPinch()
        {
            boundsAnimation.stop()
            initialWidth = flick.contentWidth
            initialHeight = flick.contentHeight
            initialContentX = flick.contentX
            initialContentY = flick.contentY
            flick.leftMargin = 0
            flick.topMargin = 0
            flick.contentX = initialContentX
            flick.contentY = initialContentY
            flick.fixMargins()
        }

        function finishPinch()
        {
            flick.animating = true
            flick.fixMargins()
            flick.animateToBounds()
        }

        function updatePinch(center, previousCenter, targetScale)
        {
            flick.contentX += previousCenter.x - center.x
            flick.contentY += previousCenter.y - center.y

            var cx = center.x + flick.contentX
            var cy = center.y + flick.contentY

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
            flick.resizeContent(initialWidth * targetScale, initialHeight * targetScale, Qt.point(cx, cy))
        }

        onPinchStarted: startPinch()
        onPinchUpdated: updatePinch(pinch.center, pinch.previousCenter, pinch.scale)
        onPinchFinished: finishPinch()

        enabled: !flick.dragging

        MouseArea
        {
            id: mouseArea

            property var doubleTapDownPos: undefined
            property bool doubleTapScaleMode: false
            property point downPos

            anchors.fill: parent

            propagateComposedEvents: true

            onDoubleTapScaleModeChanged:
            {
                if (doubleTapScaleMode)
                {
                    pinchArea.startPinch()
                }
                else
                {
                    doubleTapDownPos = undefined
                    pinchArea.finishPinch()
                }
            }

            onPositionChanged:
            {
                rootItem.positionChanged(mouse.x, mouse.y)

                if (!doubleTapDownPos)
                    return

                var currentVector = Qt.vector2d(
                    doubleTapDownPos.x - mouseX,
                    doubleTapDownPos.y - mouseY)

                if (!doubleTapScaleMode)
                {
                    var minDoubleTapStartLength = 15
                    if (currentVector.length() > minDoubleTapStartLength)
                        doubleTapScaleMode = rootItem.allowCompositeEvents

                }

                if (!doubleTapScaleMode)
                    return

                var sideSize = Math.min(flick.width, flick.height) / 2
                var scaleChange = 1 + Math.abs(currentVector.y / sideSize)
                var targetScale = currentVector.y > 0 ? 1 / scaleChange : scaleChange
                pinchArea.updatePinch(doubleTapDownPos, doubleTapDownPos, targetScale)
            }

            onDoubleClicked:
            {
                delayedClickTimer.stop()
                doubleClickFilterTimer.restart();

                var mousePosition = Qt.point(mouseX, mouseY)
                if (rootItem.doubleTapStartCheckFuncion
                    && rootItem.doubleTapStartCheckFuncion(mousePosition))
                {
                    doubleTapDownPos = mousePosition
                }
            }

            onPressed:
            {
                downPos = Qt.point(mouse.x, mouse.y)
                rootItem.pressed(mouse.x, mouse.y)
                clickFilterTimer.restart()
            }

            onCanceled:
            {
                delayedClickTimer.stop()
                rootItem.cancelled()

                doubleTapScaleMode = false
                doubleTapDownPos = undefined
            }

            onReleased:
            {
                if (clickFilterTimer.running
                    && !doubleClickFilterTimer.running
                    && downPos.x == mouse.x && downPos.y == mouse.y)
                {

                    delayedClickTimer.restart()
                }
                else
                {
                    delayedClickTimer.stop()
                }

                rootItem.released()
                doubleTapScaleMode = false
                doubleTapDownPos = undefined
                if (!doubleClickFilterTimer.running)
                    return

                doubleClickFilterTimer.stop()
                rootItem.doubleClicked(mouse.x, mouse.y)
            }

            onClicked: mouse.accepted = true

            Timer
            {
                id: delayedClickTimer

                interval: 300
                onTriggered: rootItem.clicked()
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

            onWheel:
            {
                var cx = wheel.x + flick.contentX
                var cy = wheel.y + flick.contentY

                var scale = wheel.angleDelta.y > 0 ? 1.1 : 0.9
                var w = flick.contentWidth * scale
                var h = flick.contentHeight * scale

                if (w > maxContentWidth)
                    scale = maxContentWidth / flick.contentWidth
                if (h > maxContentHeight)
                    scale = Math.max(scale, maxContentHeight / flick.contentHeight)

                flick.animating = true
                flick.fixMargins()
                flick.resizeContent(flick.contentWidth * scale, flick.contentHeight * scale, Qt.point(cx, cy))
                flick.animateToBounds()
            }
        }
    }

    Object
    {
        id: d

        readonly property bool movingInternal:
            flick.flicking
            || flick.animating
            || flick.dragging
            || pinchArea.pinch.active
            || boundsAnimation.running

        Timer
        {
            id: movingFilterTimer

            property bool value: false
            interval: 100
            onTriggered: rootItem.moving = value
        }

        onMovingInternalChanged:
        {
            if (movingFilterTimer.value == movingInternal && movingFilterTimer.running)
                return

            movingFilterTimer.value = movingInternal
            movingFilterTimer.restart()
        }
    }
}
