import QtQuick 2.4
import QtMultimedia 5.0

import "../controls"

QnZoomableFlickable {
    id: zf

    property var source
    property alias screenshotSource: screenshot.source
    property real aspectRatio: 0
    property int videoRotation: 0

    property real maxZoomFactor: 4

    readonly property real sourceContentWidth: screenshot.visible || !content.video ? screenshot.sourceSize.width : content.video.sourceRect.width
    readonly property real sourceContentHeight: screenshot.visible || !content.video ? screenshot.sourceSize.height : content.video.sourceRect.height

    minContentWidth: width
    minContentHeight: height
    maxContentWidth: Math.max(content.rotated90 ? sourceContentHeight : sourceContentWidth, width) * maxZoomFactor
    maxContentHeight: Math.max(content.rotated90 ? sourceContentWidth : sourceContentHeight, height) * maxZoomFactor

    readonly property bool zoomed: contentWidth > width * 1.05 || contentHeight > height * 1.05

    allowedHorizontalMargin: zoomed ? width / 3 : 0
    allowedVerticalMargin: zoomed ? height / 3 : 0

    clip: true

    onSourceChanged: {
        videoLoader.sourceComponent = undefined
        if (source)
            videoLoader.sourceComponent = videoComponent
    }

    Item {
        id: content

        property var video: videoLoader.item

        readonly property bool rotated90: zf.videoRotation % 90 == 0 && zf.videoRotation % 180 != 0
        onRotated90Changed: resetSize()

        width: contentWidth
        height: contentHeight

        Loader {
            id: videoLoader
            anchors.centerIn: parent
            width: content.rotated90 ? parent.height : parent.width
            height: content.rotated90 ? parent.width : parent.height
            rotation: zf.videoRotation

            onStatusChanged: {
                if (status == Loader.Ready)
                    item.source = zf.source
            }
        }

        Component {
            id: videoComponent

            VideoOutput {
                fillMode: VideoOutput.Stretch
                onSourceRectChanged: content.updateWithVideoSize(true)
            }
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
            var hasVideo = content.video && content.video.sourceRect.width > 0 && content.video.sourceRect.height > 0
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
            var w = content.video.sourceRect.width
            var h = content.video.sourceRect.height

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
        var hasVideo = content.video && content.video.sourceRect.width > 0 && content.video.sourceRect.height > 0
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
