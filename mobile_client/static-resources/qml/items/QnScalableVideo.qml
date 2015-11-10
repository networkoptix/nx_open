import QtQuick 2.4
import QtMultimedia 5.0

import "../controls"

QnZoomableFlickable {
    id: zf

    property alias source: video.source
    property alias screenshotSource: screenshot.source
    property real screenshotAspectRatio: 0

    property real maxZoomFactor: 4

    minContentWidth: width
    minContentHeight: height
    maxContentWidth: (screenshot.visible ? screenshot.sourceSize.width : video.sourceRect.width) * maxZoomFactor
    maxContentHeight: (screenshot.visible ? screenshot.sourceSize.height : video.sourceRect.height) * maxZoomFactor

    clip: true

    Item {
        id: content

        width: contentWidth
        height: contentHeight

        VideoOutput {
            id: video

            anchors.fill: parent

            onSourceRectChanged: {
                content.updateSize(sourceRect.width, sourceRect.height)
            }
        }

        Image {
            id: screenshot

            anchors.fill: parent
            visible: source != ""

            onStatusChanged: {
                if (!visible)
                    return

                if (status != Image.Ready)
                    return

                content.updateWithScreenshotSize()
            }
        }

        function updateSize(w, h) {
            if (w <= 0 || h <= 0)
                return

            var scale = Math.min(zf.width / w, zf.height / h)
            w *= scale
            h *= scale

            var animate = contentWidth > 0 && contentHeight > 0
            resizeContent(w, h, false, true)
        }

        function updateWithScreenshotSize() {
            var w = screenshot.sourceSize.width

            if (screenshotAspectRatio > 0)
                w = screenshot.sourceSize.height * screenshotAspectRatio

            content.updateSize(w, screenshot.sourceSize.height)
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
                content.updateSize(video.sourceRect.width, video.sourceRect.height)
                return
            } else if (hasScreenshot) {
                content.updateWithScreenshotSize()
                return
            }
        }

        resizeContent(width, height)
    }
}
