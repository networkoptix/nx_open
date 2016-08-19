import QtQuick 2.6
import QtMultimedia 5.0
import Nx 1.0

Item
{
    id: videoPositioner

    property Item item: null

    property real customAspectRatio: 0
    property int videoRotation: 0
    property size sourceSize

    property real visibleVideoWidth: Utils.isRotated90(videoRotation) ? item.height : item.width
    property real visibleVideoHeight: Utils.isRotated90(videoRotation) ? item.width : item.height

    readonly property real horizontalPadding:
    {
        return (Utils.isRotated90(videoRotation)
            ? (width - item.height) / 2
            : (width - item.width) / 2)
    }
    readonly property real verticalPadding:
    {
        return (Utils.isRotated90(videoRotation)
            ? (height - item.width) / 2
            : (height - item.height) / 2)
    }

    onCustomAspectRatioChanged: updateSize()
    onVideoRotationChanged: updateSize()
    onSourceSizeChanged: updateSize()
    onWidthChanged: updateSize()
    onHeightChanged: updateSize()

    function updateSize()
    {
        var size = boundedSize(width, height)
        if (Utils.isRotated90(videoRotation))
        {
            item.width = size.height
            item.height = size.width
        }
        else
        {
            item.width = size.width
            item.height = size.height
        }
    }

    onItemChanged:
    {
        if (!item)
            return

        item.parent = videoPositioner
        item.anchors.centerIn = videoPositioner
        item.rotation = Qt.binding(function() { return videoRotation })
    }

    function boundedSize(width, height)
    {
        var rotated90 = Utils.isRotated90(videoRotation)
        var boundAr = width / height
        var videoAr = (customAspectRatio == 0
            ? sourceSize.width / sourceSize.height
            : customAspectRatio)
        if (rotated90)
            videoAr = 1 / videoAr

        var w
        var h

        if (boundAr > videoAr)
        {
            h = height
            w = h * videoAr
        }
        else
        {
            w = width
            h = w / videoAr
        }

        return Qt.size(w, h)
    }
}
