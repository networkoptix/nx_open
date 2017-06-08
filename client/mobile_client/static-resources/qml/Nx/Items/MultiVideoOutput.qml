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

    signal clickedOnVideo(int channelId, rect viewport, real aspect, point mousePos)

    implicitWidth: __sourceSize.width * layoutSize.width
    implicitHeight: __sourceSize.height * layoutSize.height

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

            onSourceRectChanged:
            {
                if (index === 0 && __sourceSize.width === 0.0)
                    __sourceSize = Qt.size(sourceRect.width, sourceRect.height)
            }

            Component.onCompleted: setPlayer(mediaPlayer, index)

            MouseArea
            {
                anchors.fill: parent

                onClicked:
                {
                    var source = videoOutput.sourceRect
                    var scale =  source.width / width
                    var pos = Qt.vector2d(mouse.x, mouse.y).times(scale)
                    if (pos.x < 0 || pos.y < 0 || pos.x > source.width || pos.y >source.height)
                        return

                    var aspect = videoOutput.sourceRect.width / videoOutput.sourceRect.height
                    var center = Qt.vector2d(source.width / 2, source.height / 2)
                    var topLeft = pos.minus(center)
                    var newViewport = Qt.rect(
                        topLeft.x / source.width,
                        topLeft.y / source.height,
                        1, 1)

                    var mousePos = mapToItem(multiVideoOutput, mouse.x, mouse.y)
                    multiVideoOutput.clickedOnVideo(index, newViewport, aspect, mousePos)
                }
            }
        }
    }

    function clear()
    {
        for (var i = 0; i < repeater.count; ++i)
            repeater.itemAt(i).clear()
    }
}
