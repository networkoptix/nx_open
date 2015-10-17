import QtQuick 2.4

Item {
    id: rootItem

    property string message

    property alias contentWidth: flick.contentWidth
    property alias contentHeight: flick.contentHeight
    default property alias data: contentItem.data

    property real minContentWidth: 0
    property real minContentHeight: 0

    property real maxContentWidth: Infinity
    property real maxContentHeight: Infinity

    readonly property alias contentX: flick.contentX
    readonly property alias contentY: flick.contentY

    signal clicked()
    signal doubleClicked()

    function resizeContent(width, height, animate, forceSize) {
        flick.animateToSize(width, height, animate, forceSize)
    }

    QnFlickable {
        id: flick

        anchors.fill: parent

        topMargin: 0
        leftMargin: 0
        bottomMargin: topMargin * 2
        rightMargin: leftMargin

        flickableDirection: Flickable.HorizontalAndVerticalFlick
        boundsBehavior: Flickable.StopAtBounds

        Item {
            id: contentItem
            width: flick.contentWidth
            height: flick.contentHeight
        }

        /* Workaround for qt bug: top and left margins are ignored. */
        onContentXChanged: {
            if (leftMargin > 0 && contentX > -leftMargin)
                contentX = -leftMargin
        }
        onContentYChanged: {
            if (topMargin > 0 && contentY > -topMargin)
                contentY = -topMargin
        }
        onLeftMarginChanged: {
            if (leftMargin > 0)
                contentX = -leftMargin
        }
        onTopMarginChanged: {
            if (topMargin > 0)
                contentY = -topMargin
        }

        function updateMargins() {
            leftMargin = Qt.binding(function() { return Math.max(0, (width - contentWidth) / 2) })
            topMargin = Qt.binding(function() { return Math.max(0, (height - contentHeight) / 3) })
        }

        function animateToBounds() {
            if (contentWidth < minContentWidth && contentHeight < minContentHeight) {
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

        function animateToSize(cw, ch, animate, forceSize) {
            boundsAnimation.stop()

            var w = contentWidth
            var h = contentHeight
            var x = contentX
            var y = contentY

            if (w > 0 && h > 0 && forceSize != true) {
                var scale = Math.min(cw / w, ch / h)
                w *= scale
                h *= scale
            } else {
                w = cw
                h = ch
            }

            var xMargin = Math.max(0, (width - w) / 2)
            var yMargin = Math.max(0, (height - h) / 3)

            if (-x + w < width - xMargin)
                x = w - width + xMargin
            if (x < xMargin)
                x = -xMargin
            if (-y + h < height - yMargin * 2)
                y = h - height + yMargin * 2
            if (y < yMargin)
                y = -yMargin

            if (animate) {
                widthAnimation.to = w
                heightAnimation.to = h
                xAnimation.to = x
                yAnimation.to = y

                boundsAnimation.start()
            } else {
                contentWidth = w
                contentHeight = h
                contentX = x
                contentY = y

                updateMargins()
            }
        }

        Component.onCompleted: {
            updateMargins()
        }

        ParallelAnimation {
            id: boundsAnimation
            NumberAnimation {
                id: widthAnimation
                target: flick
                property: "contentWidth"
            }
            NumberAnimation {
                id: heightAnimation
                target: flick
                property: "contentHeight"
            }
            NumberAnimation {
                id: xAnimation
                target: flick
                property: "contentX"
            }
            NumberAnimation {
                id: yAnimation
                target: flick
                property: "contentY"
            }
            onStopped: flick.updateMargins()
        }

        onMovingChanged: {
            if (moving)
                boundsAnimation.stop()
        }
    }

    PinchArea {
        id: pinchArea

        parent: flick

        anchors.fill: flick

        property real initialWidth
        property real initialHeight
        property real initialContentX
        property real initialContentY

        onPinchStarted: {
            boundsAnimation.stop()
            initialWidth = flick.contentWidth
            initialHeight = flick.contentHeight
            initialContentX = flick.contentX
            initialContentY = flick.contentY
            flick.leftMargin = 0
            flick.topMargin = 0
            flick.contentX = initialContentX
            flick.contentY = initialContentY
        }

        onPinchUpdated: {
            flick.contentX += pinch.previousCenter.x - pinch.center.x
            flick.contentY += pinch.previousCenter.y - pinch.center.y

            var cx = pinch.center.x + flick.contentX - flick.leftMargin
            var cy = pinch.center.y + flick.contentY - flick.topMargin

            var scale = pinch.scale

            var w = initialWidth * scale
            var h = initialHeight * scale

            if (w > maxContentWidth)
                scale = maxContentWidth / initialWidth
            if (h > maxContentHeight)
                scale = Math.max(scale, maxContentHeight / initialHeight)

            flick.resizeContent(initialWidth * scale, initialHeight * scale, Qt.point(cx, cy))
        }

        onPinchFinished: {
            flick.animateToBounds()
        }

        MouseArea {
            anchors.fill: parent

            onClicked: rootItem.clicked()
            onDoubleClicked: rootItem.doubleClicked()
        }
    }
}
