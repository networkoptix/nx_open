import QtQuick 2.6
import Nx.Media 1.0
import Nx.Core 1.0

Item
{
    id: multiVideoOutput

    property MediaResourceHelper resourceHelper: null
    property MediaPlayer mediaPlayer: null

    readonly property size channelSize: __sourceSize
    readonly property size layoutSize: resourceHelper ? resourceHelper.layoutSize : Qt.size(1, 1)
    readonly property real cellWidth: width / layoutSize.width
    readonly property real cellHeight: height / layoutSize.height

    property size __sourceSize

    implicitWidth: __sourceSize.width * layoutSize.width
    implicitHeight: __sourceSize.height * layoutSize.height

    function getMoveViewportData(position)
    {
        for (var i = 0; i != repeater.count; ++i)
        {
            var child = repeater.itemAt(i);
            if (typeof child.getMoveViewportData !== "function")
                continue

            var mapped = mapToItem(child, position.x, position.y)
            if (mapped.x < 0 || mapped.y < 0 || mapped.x > child.width || mapped.y > child.height)
                continue

            return child.getMoveViewportData(mapped)
        }
        return null
    }

    Repeater
    {
        id: repeater

        model: resourceHelper && resourceHelper.channelCount

        VideoOutput
        {
            id: videoOutput

            property point layoutPosition: resourceHelper.channelPosition(index)

            fillMode: VideoOutput.Stretch
            width: cellWidth
            height: cellHeight
            x: layoutPosition.x * cellWidth
            y: layoutPosition.y * cellHeight

            function getMoveViewportData(position)
            {
                var source = videoOutput.sourceRect
                var scale =  source.width / width
                var pos = Qt.vector2d(position.x, position.y).times(scale)
                if (pos.x < 0 || pos.y < 0 || pos.x > source.width || pos.y >source.height)
                    return

                var center = Qt.vector2d(source.width / 2, source.height / 2)
                var topLeft = pos.minus(center)
                var newViewport = Qt.rect(
                    topLeft.x / source.width,
                    topLeft.y / source.height,
                    1, 1)

                var videoAspect = videoOutput.sourceRect.width / videoOutput.sourceRect.height
                var result = {"channeId": index, "viewport": newViewport, "aspect": videoAspect}
                return result
            }

            onSourceRectChanged:
            {
                if (index === 0 && __sourceSize.width === 0.0)
                    __sourceSize = Qt.size(sourceRect.width, sourceRect.height)
            }

            Component.onCompleted: setPlayer(mediaPlayer, index)
        }
    }

    function clear()
    {
        for (var i = 0; i < repeater.count; ++i)
            repeater.itemAt(i).clear()
    }
}
