import QtQuick 2.6
import Nx.Media 1.0
import Nx.Core 1.0

MultiVideoLayout
{
    property MediaPlayer mediaPlayer: null
    property Component channelOverlay: null

    function clear()
    {
        for (var i = 0; i < repeater.count; ++i)
            repeater.itemAt(i).clear()
    }

    // Implementation.

    property size __sourceSize

    implicitWidth: __sourceSize.width * layoutSize.width
    implicitHeight: __sourceSize.height * layoutSize.height

    delegate: VideoOutput
    {
        id: videoOutput
        fillMode: VideoOutput.Stretch

        readonly property int index: model.index

        onSourceRectChanged:
        {
            if (index === 0 && __sourceSize.width <= 0.0)
                __sourceSize = Qt.size(sourceRect.width, sourceRect.height)
        }

        Component.onCompleted: setPlayer(mediaPlayer, index)

        Loader
        {
            anchors.fill: parent
            sourceComponent: channelOverlay

            onLoaded:
            {
                if (item.hasOwnProperty("index"))
                    item.index = index
            }
        }
    }
}
