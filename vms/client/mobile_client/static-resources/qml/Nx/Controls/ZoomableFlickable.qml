import QtQuick 2.6
import nx.client.core 1.0
import Nx 1.0

import "private"

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

    property bool interactive: true
    readonly property alias flickable: flick
    readonly property real contentScale: Geometry.scaleFactor(
        Qt.size(width, height), Qt.size(contentWidth, contentHeight))

    signal clicked()
    signal doubleClicked(int mouseX, int mouseY)
    signal movementEnded()
    property bool moving: false

    property alias doubleTapStartCheckFuncion: controller.doubleTapStartCheckFunction
    property alias allowCompositeEvents: controller.allowCompositeEvents

    property alias pinchArea: pinchArea
    property alias mouseArea: mouseArea

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
            rootItem.interactive
            && !mouseArea.doubleTapScaleMode
            && (mouseArea.doubleTapDownPos ? false : true)
            && rootItem.allowCompositeEvents

        Item
        {
            id: contentItem
            width: flick.contentWidth
            height: flick.contentHeight
        }

        /* Workaround for qt bug: top and left margins are ignored. */
        onContentXChanged: fixContentX()
        onContentYChanged: fixContentY()
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
            if (w <= width)
                x = 0

            if (-y + h < rootItem.height - allowedBottomMargin)
                y = h - rootItem.height + allowedBottomMargin
            if (-y > allowedTopMargin)
                y = -allowedTopMargin
            if (h <= height)
                y = 0

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

    ZoomableFlickableController
    {
        id: controller

        enabled: !flick.dragging && rootItem.interactive

        pinchArea: pinchArea
        mouseArea: mouseArea
        flickable: flick

        onClicked: rootItem.clicked()
        onDoubleClicked: rootItem.doubleClicked(mouseX, mouseY)
    }

    PinchArea
    {
        id: pinchArea

        parent: flick
        anchors.fill: parent

        MouseArea
        {
            id: mouseArea

            propagateComposedEvents: true
            anchors.fill: parent
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
