import QtQuick 2.6
import QtMultimedia 5.0
import Nx.Media 1.0
import Nx.Controls 1.0
import Nx.Items 1.0
import com.networkoptix.qml 1.0

ZoomableFlickable
{
    id: zf

    property alias mediaPlayer: video.mediaPlayer
    property alias resourceHelper: video.resourceHelper

    property real maxZoomFactor: 4
    property alias videoCenterHeightOffsetFactor: content.videoCenterHeightOffsetFactor
    property size fitSize: content.boundedSize(width, height)
    function getMoveViewportData(position)
    {
        var mapped = mapToItem(video, position.x, position.y)
        return video.getMoveViewportData(mapped)
    }

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

    allowedLeftMargin: zoomed ? width / 4 - content.leftPadding : 0
    allowedRightMargin: zoomed ? width / 4 - content.rightPadding : 0
    allowedTopMargin: zoomed ? height / 4 - content.topPadding : 0
    allowedBottomMargin: zoomed ? height / 4 - content.bottomPadding : 0

    clip: true

    onDoubleClicked:
    {
        var videoMappedPosition = mapToItem(video, mouseX, mouseY)
        if (!video.pointInVideo(videoMappedPosition))
            return

        var twiceTargetScale = 2
        var initialTargetScale = 1
        var eps = 0.000001
        var zoomIn = d.scale < twiceTargetScale - eps
        var targetScale = zoomIn ? twiceTargetScale : initialTargetScale

        var baseWidth = contentWidth / d.scale
        var baseHeight = contentHeight / d.scale

        flickable.animating = true
        flickable.fixMargins()


        var point = mapToItem(content, mouseX, mouseY)
        var dx = point.x / d.scale
        var dy = point.y / d.scale

        contentWidth = baseWidth * targetScale
        contentHeight = baseHeight * targetScale

        var x = zoomIn ? (width / 2 - targetScale * dx) : (width - baseWidth) / 2
        var y = zoomIn ? (height / 2 - targetScale * dy) : (height - baseHeight) / 2

        console.log(allowedLeftMargin, allowedRightMargin, allowedTopMargin, allowedBottomMargin)

        if (x > allowedLeftMargin)
            x = allowedLeftMargin - 1
        else if (x + contentWidth < width - allowedRightMargin)
            x = width - contentWidth - allowedRightMargin + 1

        if (y > allowedTopMargin)
            y = allowedTopMargin - 1
        else if (y + contentHeight < height - allowedBottomMargin)
            y = height - contentHeight - allowedBottomMargin + 1

        contentX = -x
        contentY = -y

        flickable.animateToBounds()
    }

    QtObject
    {
        id: d

        property real contentFactor: contentWidth ? contentHeight / contentWidth : 0

        readonly property real scale:
        {
            if (!contentFactor)
                return 1

            var baseSize = 0
            var currentSize = 0

            var baseHeight = zf.width * contentFactor
            if (baseHeight <= zf.height)
            {

                baseSize = baseHeight
                currentSize = zf.contentHeight
            }
            else
            {
                baseSize = width
                currentSize = zf.contentWidth
            }

            return baseSize > 0 ? currentSize / baseSize : 1
        }
    }

    VideoPositioner
    {
        id: content

        width: contentWidth
        height: contentHeight
        sourceSize: Qt.size(video.implicitWidth, video.implicitHeight)
        videoRotation: resourceHelper ? resourceHelper.customRotation : 0
        customAspectRatio:
        {
            var aspectRatio = resourceHelper ? resourceHelper.customAspectRatio : 0.0
            if (aspectRatio === 0.0)
            {
                if (mediaPlayer)
                    aspectRatio = mediaPlayer.aspectRatio
                else
                    aspectRatio = sourceSize.width / sourceSize.height
            }

            var layoutSize = resourceHelper ? resourceHelper.layoutSize : Qt.size(1, 1)
            aspectRatio *= layoutSize.width / layoutSize.height

            return aspectRatio
        }

        item: video

        MultiVideoOutput { id: video }

        onSourceSizeChanged: fitToBounds()
    }

    onWidthChanged: fitToBounds()
    onHeightChanged: fitToBounds()

    function fitToBounds()
    {
        resizeContent(width, height, false, true)
    }

    function clear()
    {
        video.clear()
    }
}
