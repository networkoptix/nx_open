import QtQuick 2.6
import QtMultimedia 5.0
import Nx.Controls 1.0
import Nx.Items 1.0

ZoomableFlickable
{
    id: zf

    property alias source: video.source
    property alias screenshotSource: screenshot.source
    property alias customAspectRatio: content.customAspectRatio
    property alias videoRotation: content.videoRotation

    property real maxZoomFactor: 4

    readonly property size sourceSize: (screenshot.visible
        ? screenshot.sourceSize
        : Qt.size(video.sourceRect.width, video.sourceRect.height))

    minContentWidth: width
    minContentHeight: height
    maxContentWidth:
        maxZoomFactor * Math.max(
            content.rotated90
                ? sourceSize.height
                : sourceSize.width,
            width)
    maxContentHeight:
        maxZoomFactor * Math.max(
            content.rotated90
                ? sourceSize.width
                : sourceSize.height,
            height)

    readonly property bool zoomed: contentWidth > width * 1.05 || contentHeight > height * 1.05

    allowedHorizontalMargin: zoomed ? width / 3 - content.horizontalPadding : 0
    allowedVerticalMargin: zoomed ? height / 3 - content.verticalPadding : 0

    clip: true

    VideoPositioner
    {
        id: content

        width: contentWidth
        height: contentHeight
        sourceSize: Qt.size(video.sourceRect.width, video.sourceRect.height)

        item: VideoOutput
        {
            id: video
            fillMode: VideoOutput.Stretch
        }

        Image
        {
            id: screenshot
            anchors.fill: parent
            fillMode: Image.PreserveAspectFit
            visible: source != ""
        }

        onSourceSizeChanged: fitToBounds()
    }

    onWidthChanged: fitToBounds()
    onHeightChanged: fitToBounds()

    function fitToBounds()
    {
        if (content.sourceSize.width <= 0 || content.sourceSize.height <= 0)
        {
            resizeContent(width, height, false, true)
            return
        }

        var size = content.boundedSize(width, height)
        resizeContent(size.width, size.height, false, true)
    }
}
