import QtQuick 2.6
import QtMultimedia 5.0
import Nx.Media 1.0
import Nx.Controls 1.0
import Nx.Items 1.0
import com.networkoptix.qml 1.0
import Nx 1.0

ZoomableFlickable
{
    id: zf

    property alias mediaPlayer: content.mediaPlayer
    property alias resourceHelper: content.resourceHelper
    property MotionController motionSearchController: null

    property real maxZoomFactor: 4
    property alias videoCenterHeightOffsetFactor: content.videoCenterHeightOffsetFactor
    property size fitSize: content.boundedSize(width, height)
    property rect videoRect

    function getMoveViewportData(position)
    {
        var videoItem = content.videoOutput
        var mapped = mapToItem(videoItem, position.x, position.y)
        return videoItem.getMoveViewportData(mapped)
    }

    Connections
    {
        target: resourceHelper
        onResourceIdChanged: to1xScale()
    }

    allowCompositeEvents: !motionSearchController || !motionSearchController.drawingRoi
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

    allowedLeftMargin: d.allowedLeftMargin(contentWidth, contentHeight)
    allowedRightMargin: d.allowedRightMargin(contentWidth, contentHeight)
    allowedTopMargin: d.allowedTopMargin(contentWidth, contentHeight)
    allowedBottomMargin: d.allowedBottomMargin(contentWidth, contentHeight)

    clip: true

    function to1xScale()
    {
        d.toggleScale(false, width / 2, height / 2)
    }

    doubleTapStartCheckFuncion:
        function(initialPosition)
        {
            var videoItem = content.videoOutput
            var videoMappedPosition = mapToItem(videoItem, initialPosition.x, initialPosition.y)
            return videoItem.pointInVideo(videoMappedPosition);
        }

    onDoubleClicked:
    {
        var videoItem = content.videoOutput
        var videoMappedPosition = mapToItem(videoItem, mouseX, mouseY)
        if (!videoItem.pointInVideo(videoMappedPosition))
            return

        var twiceTargetScale = 2
        var eps = 0.000001
        var zoomIn = zf.contentScale < twiceTargetScale - eps

        d.toggleScale(zoomIn, mouseX, mouseY)
    }

    Object
    {
        id: d

        Connections
        {
            target: content.videoOutput
            onXChanged: d.updateVideoRect()
            onYChanged: d.updateVideoRect()
            onHeightChanged: d.updateVideoRect()
            onWidthChanged: d.updateVideoRect()
        }

        Connections
        {
            target: zf
            onContentRectChanged: d.updateVideoRect()
        }

        function updateVideoRect()
        {
            var output = content.videoOutput
            var x = output.x
            var y = output.y

            var topLeft = parent.mapFromItem(content, x, y)
            var bottomRight = parent.mapFromItem(content, x + output.width, y + output.height)
            var width = Math.abs(bottomRight.x - topLeft.x)
            var height = Math.abs(bottomRight.y - topLeft.y)

            videoRect = Qt.rect(topLeft.x, topLeft.y, width, height)
        }


        function allowedMargin(targetWidth, targetHeight, size, padding, factor)
        {
            var zoomed = targetWidth > width * 1.05 || targetHeight > height * 1.05;
            return zoomed ? size * factor - padding : 0
        }

        function allowedLeftMargin(targetWidth, targetHeight)
        {
            return allowedMargin(targetWidth, targetHeight, width, leftPadding + content.leftPadding, 0.3)
        }

        function allowedRightMargin(targetWidth, targetHeight)
        {
            return allowedMargin(targetWidth, targetHeight, width, rightPadding + content.rightPadding, 0.3)
        }

        function allowedTopMargin(targetWidth, targetHeight)
        {
            return allowedMargin(targetWidth, targetHeight, height, topPadding + content.topPadding, 0.3)
        }

        function allowedBottomMargin(targetWidth, targetHeight)
        {
            return allowedMargin(targetWidth, targetHeight, height, bottomPadding + content.bottomPadding, 0.5)
        }

        function toggleScale(to2x, mouseX, mouseY)
        {
            var targetScale = to2x ? 2 : 1

            var baseWidth = contentWidth / zf.contentScale
            var baseHeight = contentHeight / zf.contentScale

            flickable.animating = true
            flickable.fixMargins()

            var point = mapToItem(content, mouseX, mouseY)
            var dx = point.x / zf.contentScale
            var dy = point.y / zf.contentScale

            var newContentWidth = baseWidth * targetScale
            var newContentHeight = baseHeight * targetScale

            var newX = to2x ? (width / 2 - targetScale * dx) : (width - newContentWidth) / 2
            var newY = to2x ? (height / 2 - targetScale * dy) : (height - newContentHeight) / 2

            var leftMargin = Math.abs(d.allowedLeftMargin(newContentWidth, newContentHeight))
            var rightMargin = Math.abs(d.allowedRightMargin(newContentWidth, newContentHeight))
            var topMargin = Math.abs(d.allowedTopMargin(newContentWidth, newContentHeight))
            var bottomMargin = Math.abs(d.allowedBottomMargin(newContentWidth, newContentHeight))

            if (newX > leftMargin)
                newX = leftMargin - 1
            if (newX + newContentWidth < width - rightMargin)
                newX = width - newContentWidth - rightMargin + 1

            if (newY > topMargin)
                newY = topMargin - 1
            if (newY + newContentHeight < height - bottomMargin)
                newY = height - newContentHeight - bottomMargin + 1

            var easing = newContentWidth > contentWidth ? Easing.InOutCubic : Easing.OutCubic
            flickable.animateTo(-newX, -newY, newContentWidth, newContentHeight, easing)
        }
    }

    MultiVideoPositioner
    {
        id: content

        resourceHelper: zf.resourceHelper

        width: contentWidth
        height: contentHeight

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
        content.videoOutput.clear()
    }

    Component.onCompleted: d.updateVideoRect()
}
