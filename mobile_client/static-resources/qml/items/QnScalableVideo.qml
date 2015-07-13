import QtQuick 2.4
import QtMultimedia 5.0

import "../controls"

QnZoomableFlickable {
    id: zf

    property alias source: video.source

    property real maxZoomFactor: 4

    minContentWidth: width
    minContentHeight: height
    maxContentWidth: d.sourceWidth * maxZoomFactor
    maxContentHeight: d.sourceHeight * maxZoomFactor

    QtObject {
        id: d

        property bool stick: true
        property bool initialized: false

        property int sourceWidth: video.sourceRect.width
        property int sourceHeight: video.sourceRect.height

        onSourceWidthChanged: updateSize()
        onSourceHeightChanged: updateSize()

        function updateStick() {
            if (!d.initialized) {
                if (!updateSize())
                    return

                d.initialized = true
            }

            var ar = zf.width / zf.height
            var car = zf.contentWidth / zf.contentHeight

            if (car > ar)
                d.stick = Math.abs(zf.contentWidth - zf.width) < dp(56)
            else
                d.stick = Math.abs(zf.contentHeight - zf.height) < dp(56)
        }

        function updateSize() {
            if (zf.width <= 0 || zf.height <= 0 || d.sourceWidth <= 0 || d.sourceHeight <= 0)
                return false

            if (d.stick) {
                zf.contentWidth = zf.width
                zf.contentHeight = zf.width / d.sourceWidth * d.sourceHeight
            } else {
                zf.contentWidth = d.sourceWidth
                zf.contentHeight = d.sourceHeight
            }

            return true
        }

        function autoSize() {
            if (!d.stick)
                return

            zf.fitToBounds()
        }
    }

    VideoOutput {
        id: video

        width: zf.contentWidth
        height: zf.contentHeight
    }

    onContentWidthChanged: d.updateStick()
    onContentHeightChanged: d.updateStick()

    onWidthChanged: d.autoSize()
    onHeightChanged: d.autoSize()

    function fitToBounds() {
        animateToSize(zf.width, zf.height)
    }
}
