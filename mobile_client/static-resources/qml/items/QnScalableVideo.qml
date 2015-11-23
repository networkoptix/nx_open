import QtQuick 2.4
import QtMultimedia 5.0

import "../controls"

QnZoomableFlickable {
    id: zf

    property alias source: video.source
    property alias screenshotSource: screenshot.source
    property real aspectRatio: 0
    property int videoRotation: 0

    property real maxZoomFactor: 4

    minContentWidth: width
    minContentHeight: height
    maxContentWidth: (screenshot.visible ? screenshot.sourceSize.width : video.sourceRect.width) * maxZoomFactor
    maxContentHeight: (screenshot.visible ? screenshot.sourceSize.height : video.sourceRect.height) * maxZoomFactor

    clip: true

    Item {
        id: content

        readonly property bool rotated90: zf.videoRotation % 90 == 0 && zf.videoRotation % 180 != 0
        onRotated90Changed: resetSize()

        width: contentWidth
        height: contentHeight

        VideoOutput {
            id: video

            anchors.centerIn: parent
            width: content.rotated90 ? parent.height : parent.width
            height: content.rotated90 ? parent.width : parent.height
            rotation: zf.videoRotation

            fillMode: VideoOutput.Stretch

            onSourceRectChanged: content.updateWithVideoSize(true)
        }

        Image {
            id: screenshot

            anchors.centerIn: parent
            width: content.rotated90 ? parent.height : parent.width
            height: content.rotated90 ? parent.width : parent.height
            rotation: zf.videoRotation

            visible: source != ""

            onStatusChanged: {
                if (!visible)
                    return

                if (status != Image.Ready)
                    return

                content.updateWithScreenshotSize(true)
            }
        }

        function resetSize() {
            var hasVideo = video.sourceRect.width > 0 && video.sourceRect.height > 0
            var hasScreenshot = screenshot.sourceSize.width > 0 && screenshot.sourceSize.height > 0
            var emptySize = width <= 0 || height <= 0

            if ((!hasVideo && !hasScreenshot) || emptySize)
                return

            if (hasVideo)
                updateWithVideoSize()
            else if (hasScreenshot)
                updateWithScreenshotSize()
        }

        function updateSize(w, h, saveZoom) {
            if (w <= 0 || h <= 0)
                return

            if (rotated90) {
                var tmp = w
                w = h
                h = tmp
            }

            var scale = 1.0
            if (saveZoom && contentWidth > 0 && contentHeight > 0)
                scale = Math.min(contentWidth / w, contentHeight / h)
            else
                scale = Math.min(zf.width / w, zf.height / h)

            w *= scale
            h *= scale

            var animate = contentWidth > 0 && contentHeight > 0
            resizeContent(w, h, animate, true)
        }

        function updateWithVideoSize(saveZoom) {
            var w = video.sourceRect.width
            var h = video.sourceRect.height

            if (aspectRatio > 0)
                w = h * aspectRatio

            content.updateSize(w, h, saveZoom)
        }

        function updateWithScreenshotSize(saveZoom) {
            var w = screenshot.sourceSize.width
            var h = screenshot.sourceSize.height

            if (aspectRatio > 0)
                w = h * aspectRatio

            content.updateSize(w, h, saveZoom)
        }
    }

    onWidthChanged: fitToBounds()
    onHeightChanged: fitToBounds()

    function fitToBounds() {
        var hasVideo = video.sourceRect.width > 0 && video.sourceRect.height > 0
        var hasScreenshot = screenshot.sourceSize.width > 0 && screenshot.sourceSize.height > 0
        var emptySize = width <= 0 || height <= 0

        if ((!hasVideo && !hasScreenshot) || emptySize)
            return

        if (contentWidth <= 0 || contentHeight <= 0) {
            if (hasVideo) {
                content.updateWithVideoSize()
                return
            } else if (hasScreenshot) {
                content.updateWithScreenshotSize()
                return
            }
        }

        resizeContent(width, height)
    }
}
