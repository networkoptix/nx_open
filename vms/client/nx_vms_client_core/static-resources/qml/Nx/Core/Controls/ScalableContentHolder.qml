// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.6

import Nx.Core 1.0
import Nx.Core.Controls 1.0

import nx.client.core 1.0

import "private"

Item
{
    id: control

    default property alias data: contentItem.data

    property real maxZoomFactor: 4
    property Item content: contentItem.children.length ? contentItem.children[0] : null

    property alias contentX: flick.contentX
    property alias contentY: flick.contentY
    property alias contentWidth: flick.contentWidth
    property alias contentHeight: flick.contentHeight

    readonly property bool falseDragging:
        (Math.abs(flick.contentWidth - width) <= 1 && Math.abs(flick.contentHeight - height) <= 1)
        && controller.enabled
        && controller.draggingGesture

    property real minContentWidth: 0
    property real minContentHeight: 0

    property real maxContentWidth: Infinity
    property real maxContentHeight: Infinity

    property real allowedHorizontalMargin: 0
    property real allowedVerticalMargin: 0

    property real allowedLeftMargin: d.allowedLeftMargin(contentWidth, contentHeight)
    property real allowedRightMargin: d.allowedRightMargin(contentWidth, contentHeight)
    property real allowedTopMargin: d.allowedTopMargin(contentWidth, contentHeight)
    property real allowedBottomMargin: d.allowedBottomMargin(contentWidth, contentHeight)

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

    function to1xScale()
    {
        d.toggleScale(false, width / 2, height / 2)
    }

    function fitToBounds(animated)
    {
        resizeContent(width, height, animated, true)
    }

    onWidthChanged: fitToBounds()
    onHeightChanged: fitToBounds()

    onDoubleClicked: (mouseX, mouseY) =>
    {
        var videoItem = content.videoOutput
        var videoMappedPosition = mapToItem(videoItem, mouseX, mouseY)
        if (!videoItem.pointInVideo(videoMappedPosition))
            return

        var twiceTargetScale = 2
        var eps = 0.000001
        var zoomIn = control.contentScale < twiceTargetScale - eps

        d.toggleScale(zoomIn, mouseX, mouseY)
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

        onMovementEnded: control.movementEnded()

        interactive:
            control.interactive
            && !controller.scaling
            && control.allowCompositeEvents

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

            if (-y + h < control.height - allowedBottomMargin)
                y = h - control.height + allowedBottomMargin
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
                control.movementEnded()
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

                control.movementEnded()
            }
        }

        onMovingChanged:
        {
            if (moving)
                boundsAnimation.stop()
        }
    }

    ScalableContentHolderController
    {
        id: controller

        enabled: !flick.dragging && control.interactive

        pinchArea: pinchArea
        mouseArea: mouseArea
        flickable: flick

        onClicked: control.clicked()
        onDoubleClicked: (mouseX, mouseY) =>
        {
            control.doubleClicked(mouseX, mouseY)
        }
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

    NxObject
    {
        id: d

        readonly property real factor: 0.3
        readonly property bool movingInternal:
            flick.flicking
            || flick.animating
            || flick.dragging
            || pinchArea.pinch.active
            || boundsAnimation.running

        function getPadding(padding)
        {
            return padding ? padding : 0
        }

        function allowedMargin(targetWidth, targetHeight, size, padding, factor)
        {
            var zoomed = targetWidth > width * 1.05 || targetHeight > height * 1.05;
            return zoomed ? size * factor - padding : 0
        }

        function allowedLeftMargin(targetWidth, targetHeight)
        {
            return allowedMargin(targetWidth, targetHeight, width,
                control.leftPadding + getPadding(content.leftPadding), factor)
        }

        function allowedRightMargin(targetWidth, targetHeight)
        {
            return allowedMargin(targetWidth, targetHeight, width,
                control.rightPadding + getPadding(content.rightPadding), factor)
        }

        function allowedTopMargin(targetWidth, targetHeight)
        {
            return allowedMargin(targetWidth, targetHeight, height,
                control.topPadding + getPadding(content.topPadding), factor)
        }

        function allowedBottomMargin(targetWidth, targetHeight)
        {
            return allowedMargin(targetWidth, targetHeight, height,
                control.bottomPadding + getPadding(content.bottomPadding), factor)
        }

        function toggleScale(to2x, mouseX, mouseY)
        {
            var targetScale = to2x ? 2 : 1

            var baseWidth = contentWidth / control.contentScale
            var baseHeight = contentHeight / control.contentScale

            flickable.animating = true
            flickable.fixMargins()

            var point = mapToItem(content, mouseX, mouseY)
            var dx = point.x / control.contentScale
            var dy = point.y / control.contentScale

            var newContentWidth = baseWidth * targetScale
            var newContentHeight = baseHeight * targetScale

            var newX = to2x ? (width / 2 - targetScale * dx) : (width - newContentWidth) / 2
            var newY = to2x ? (height / 2 - targetScale * dy) : (height - newContentHeight) / 2

            var leftMargin = Math.abs(d.allowedLeftMargin(newContentWidth, newContentHeight))
            var rightMargin = Math.abs(d.allowedRightMargin(newContentWidth, newContentHeight))
            var topMargin = Math.abs(d.allowedTopMargin(newContentWidth, newContentHeight))
            var bottomMargin = Math.abs(d.allowedBottomMargin(newContentWidth, newContentHeight))

            if (newX > leftMargin)
                newX = leftMargin - 1
            if (newX + newContentWidth < width - rightMargin)
                newX = width - newContentWidth - rightMargin + 1

            if (newY > topMargin)
                newY = topMargin - 1
            if (newY + newContentHeight < height - bottomMargin)
                newY = height - newContentHeight - bottomMargin + 1

            var easing = newContentWidth > contentWidth ? Easing.InOutCubic : Easing.OutCubic
            flickable.animateTo(-newX, -newY, newContentWidth, newContentHeight, easing)
        }

        Timer
        {
            id: movingFilterTimer

            property bool value: false
            interval: 100
            onTriggered: control.moving = value
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
