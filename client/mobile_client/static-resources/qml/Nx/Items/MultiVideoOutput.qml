import QtQuick 2.6

import Nx.Items 1.0

import com.networkoptix.qml 1.0

Item
{
    id: multiVideoOutput

    property QnMediaResourceHelper resourceHelper: null
    property MediaPlayer mediaPlayer: null

    readonly property size channelSize: __sourceSize
    readonly property size layoutSize: resourceHelper ? resourceHelper.layoutSize : Qt.size(1, 1)
    readonly property real cellWidth: width / layoutSize.width
    readonly property real cellHeight: height / layoutSize.height
    property real customAspectRatio: mediaPlayer
        ? mediaPlayer.aspectRatio
        : resourceHelper
            ? resourceHelper.customAspectRatio
            : __channelAspectRatio

    property real __channelAspectRatio:
        __sourceSize.height > 0 ? __sourceSize.width / __sourceSize.height : 0
    property size __sourceSize: Qt.size(640, 480) // Just some default value

    implicitWidth: __sourceSize.width * layoutSize.width
    implicitHeight: __sourceSize.height * layoutSize.height

    Repeater
    {
        model: resourceHelper ? resourceHelper.channelCount : 0

        VideoPositioner
        {
            property point layoutPosition: resourceHelper.channelPosition(index)

            customAspectRatio: multiVideoOutput.customAspectRatio

            width: cellWidth
            height: cellHeight
            x: layoutPosition.x * cellWidth
            y: layoutPosition.y * cellHeight

            item: QnVideoOutput
            {
                fillMode: QnVideoOutput.Stretch

                Component.onCompleted:
                {
                    setPlayer(mediaPlayer, index)
                    multiVideoOutput.clearRequested.connect(
                        function()
                        {
                            clear()
                        })
                }
                onSourceRectChanged:
                {
                    if (index > 0 && __sourceSize.width > 0)
                        return

                    if (customAspectRatio > 0)
                    {
                        __sourceSize =
                            Qt.size(sourceRect.height * customAspectRatio, sourceRect.height)
                    }
                    else
                    {
                        __sourceSize = Qt.size(sourceRect.width, sourceRect.height)
                    }
                }
            }
        }
    }

    signal clearRequested()

    function clear()
    {
        clearRequested()
    }
}
