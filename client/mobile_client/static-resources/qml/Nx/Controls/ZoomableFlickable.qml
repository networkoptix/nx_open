import QtQuick 2.6

Item
{
    id: rootItem

    property string message

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

    readonly property alias contentX: flick.contentX
    readonly property alias contentY: flick.contentY

    signal clicked()
    signal doubleClicked()

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
                widthAnimation.to = w
                heightAnimation.to = h
                xAnimation.to = x
                yAnimation.to = y

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
            }
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

        onPinchStarted:
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

        onPinchUpdated:
        {
            flick.contentX += pinch.previousCenter.x - pinch.center.x
            flick.contentY += pinch.previousCenter.y - pinch.center.y

            var cx = pinch.center.x + flick.contentX
            var cy = pinch.center.y + flick.contentY

            var scale = pinch.scale

            var w = initialWidth * scale
            var h = initialHeight * scale

            if (w > maxContentWidth)
                scale = maxContentWidth / initialWidth
            if (h > maxContentHeight)
                scale = Math.min(scale, maxContentHeight / initialHeight)

            flick.resizeContent(initialWidth * scale, initialHeight * scale, Qt.point(cx, cy))
        }

        onPinchFinished:
        {
            flick.animating = true
            flick.fixMargins()
            flick.animateToBounds()
        }

        MouseArea
        {
            id: mouseArea

            readonly property real zoomFactor: 1.1
            readonly property real wheelStep: 120

            anchors.fill: parent

            propagateComposedEvents: true

            onDoubleClicked: rootItem.doubleClicked()
            onClicked:
            {
                rootItem.clicked()
                mouse.accepted = false
            }

            onWheel:
            {
                var cx = wheel.x + flick.contentX
                var cy = wheel.y + flick.contentY

                var scale = wheel.angleDelta.y / wheelStep * zoomFactor
                if (scale < 0)
                    scale = 1 / -scale

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
}
