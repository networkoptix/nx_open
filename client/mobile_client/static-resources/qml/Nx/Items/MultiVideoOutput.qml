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

    Repeater
    {
        id: repeater

        model: resourceHelper && resourceHelper.channelCount

        VideoOutput
        {
            property point layoutPosition: resourceHelper.channelPosition(index)

            fillMode: VideoOutput.Stretch
            width: cellWidth
            height: cellHeight
            x: layoutPosition.x * cellWidth
            y: layoutPosition.y * cellHeight

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
