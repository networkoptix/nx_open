import QtQuick 2.6
import QtMultimedia 5.0
import Nx.Controls 1.0
import Nx.Items 1.0
import com.networkoptix.qml 1.0

ZoomableFlickable
{
    id: zf

    property alias mediaPlayer: video.mediaPlayer
    property alias resourceHelper: video.resourceHelper

    property real maxZoomFactor: 4

    minContentWidth: width
    minContentHeight: height
    maxContentWidth:
        maxZoomFactor * Math.max(
            content.rotated90
                ? content.sourceSize.height
                : content.sourceSize.width,
            width)
    maxContentHeight:
        maxZoomFactor * Math.max(
            content.rotated90
                ? content.sourceSize.width
                : content.sourceSize.height,
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
        sourceSize: Qt.size(video.implicitWidth, video.implicitHeight)
        videoRotation: resourceHelper ? resourceHelper.customRotation : 0

        item: MultiVideoOutput { id: video }

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

    function clear()
    {
        video.clear()
    }
}
