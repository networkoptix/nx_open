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

    function to1xScale()
    {
        d.toggleScale(false, width / 2, height / 2)
    }

    doubleTapStartCheckFuncion:
        function(initialPosition)
        {
            var videoMappedPosition = mapToItem(video, initialPosition.x, initialPosition.y)
            return video.pointInVideo(videoMappedPosition);
        }

    onDoubleClicked:
    {
        var videoMappedPosition = mapToItem(video, mouseX, mouseY)
        if (!video.pointInVideo(videoMappedPosition))
            return

        var twiceTargetScale = 2
        var eps = 0.000001
        var zoomIn = zf.scale < twiceTargetScale - eps

        d.toggleScale(zoomIn, mouseX, mouseY)
    }

    QtObject
    {
        id: d

        function toggleScale(to2x, mouseX, mouseY)
        {
            var targetScale = to2x ? 2 : 1

            var baseWidth = contentWidth / zf.scale
            var baseHeight = contentHeight / zf.scale

            flickable.animating = true
            flickable.fixMargins()

            var point = mapToItem(content, mouseX, mouseY)
            var dx = point.x / zf.scale
            var dy = point.y / zf.scale

            contentWidth = baseWidth * targetScale
            contentHeight = baseHeight * targetScale

            var x = to2x ? (width / 2 - targetScale * dx) : (width - baseWidth) / 2
            var y = to2x ? (height / 2 - targetScale * dy) : (height - baseHeight) / 2

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
