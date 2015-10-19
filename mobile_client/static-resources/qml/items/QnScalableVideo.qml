import QtQuick 2.4
import QtMultimedia 5.0

import "../controls"

QnZoomableFlickable {
    id: zf

    property alias source: video.source

    property real maxZoomFactor: 4

    minContentWidth: width
    minContentHeight: height
    maxContentWidth: video.sourceRect.width * maxZoomFactor
    maxContentHeight: video.sourceRect.height * maxZoomFactor

    clip: true

    VideoOutput {
        id: video

        width: contentWidth
        height: contentHeight

        onSourceRectChanged: {
            var w = sourceRect.width
            var h = sourceRect.height

            if (w <= 0 || h <= 0)
                return

            var scale = Math.min(zf.width / w, zf.height / h)
            w *= scale
            h *= scale

            var animate = contentWidth > 0 && contentHeight > 0

            resizeContent(w, h, animate, true)
        }
    }

    onWidthChanged: fitToBounds()
    onHeightChanged: fitToBounds()

    function fitToBounds() {
        if (video.sourceRect.width <= 0 || video.sourceRect.height <= 0 || width == 0 || height == 0)
            return

        resizeContent(width, height)
    }
}
