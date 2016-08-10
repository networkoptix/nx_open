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
        var rotated90 = Utils.isRotated90(videoRotation)
        var positionerAr = videoPositioner.width / videoPositioner.height
        var videoAr = (customAspectRatio == 0
            ? sourceSize.width / sourceSize.height
            : customAspectRatio)
        if (rotated90)
            videoAr = 1 / videoAr

        var w
        var h

        if (positionerAr > videoAr)
        {
            h = videoPositioner.height
            w = h * videoAr
        }
        else
        {
            w = videoPositioner.width
            h = w / videoAr
        }

        if (rotated90)
        {
            item.width = h
            item.height = w
        }
        else
        {
            item.width = w
            item.height = h
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
}
