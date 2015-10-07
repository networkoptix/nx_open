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

        width: zf.contentWidth
        height: zf.contentHeight

        onSourceRectChanged: {
            var w = sourceRect.width
            var h = sourceRect.height

            if (w < 0 || h < 0)
                return

            var scale = Math.min(zf.width / w, zf.height / h)
            w *= scale
            h *= scale

            animateToSize(w, h, true)
        }
    }

    onWidthChanged: zf.fitToBounds()
    onHeightChanged: zf.fitToBounds()

    function fitToBounds() {
        animateToSize(zf.width, zf.height)
    }
}
