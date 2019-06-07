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

    property real videoCenterHeightOffsetFactor: 0

    property real visibleVideoWidth: Utils.isRotated90(videoRotation) ? item.height : item.width
    property real visibleVideoHeight: Utils.isRotated90(videoRotation) ? item.width : item.height

    readonly property real leftPadding: (width - visibleVideoWidth) / 2
    readonly property real rightPadding: leftPadding

    readonly property real topPadding:
        (height - visibleVideoHeight) / 2 * (1 - videoCenterHeightOffsetFactor)
    readonly property real bottomPadding: height - visibleVideoHeight - topPadding

    readonly property size implicitSize:
    {
        if (sourceSize.width === 0.0 || sourceSize.height === 0.0)
            return Qt.size(0, 0)

        var videoAr = (customAspectRatio == 0
            ? sourceSize.width / sourceSize.height
            : customAspectRatio)

        var width = sourceSize.width
        var height = width / videoAr

        if (Utils.isRotated90(videoRotation))
            return Qt.size(width, height)
        else
            return Qt.size(height, width)
    }

    implicitWidth: implicitSize.width
    implicitHeight: implicitSize.height

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
        item.anchors.verticalCenterOffset =
            Qt.binding(function()
            {
                return -(height - visibleVideoHeight) / 2 * videoCenterHeightOffsetFactor
            })
        item.rotation = Qt.binding(function() { return videoRotation })
    }

    function boundedSize(width, height)
    {
        if (height === 0.0 || sourceSize.height === 0.0)
            return Qt.size(0, 0)

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
