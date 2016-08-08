import QtQuick 2.6
import QtMultimedia 5.0

Item
{
    id: videoItem

    property real aspectRatio: 0
    property int videoRotation: 0
    property alias source: videoOutput.source

    readonly property size sourceSize:
        Qt.size(videoOutput.sourceRect.width, videoOutput.sourceRect.height)

    VideoOutput
    {
        id: videoOutput

        anchors.centerIn: parent
        fillMode: VideoOutput.Stretch
        rotation: videoRotation

        function updateSize()
        {
            var rotated90 =
                videoRotation % 90 == 0 && videoRotation % 180 != 0

            var itemAr = videoItem.width / videoItem.height
            var videoAr
            if (aspectRatio == 0)
                videoAr = sourceSize.width / sourceSize.height
            else
                videoAr = rotated90 ? 1 / aspectRatio : aspectRatio

            var w
            var h

            if (itemAr > videoAr)
            {
                h = videoItem.height
                w = h * videoAr
            }
            else
            {
                w = videoItem.width
                h = w / videoAr
            }

            if (rotated90)
            {
                width = h
                height = w
            }
            else
            {
                width = w
                height = h
            }
        }

        onSourceRectChanged:
        {
            if (aspectRatio == 0)
                updateSize()
        }
    }

    onAspectRatioChanged: videoOutput.updateSize()
    onVideoRotationChanged: videoOutput.updateSize()
}
