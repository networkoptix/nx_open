import QtQuick 2.6

import Nx 1.0
import Nx.Core 1.0
import Nx.Items 1.0
import Nx.Media 1.0

Rectangle
{
    id: intro
    color: ColorTheme.window

    property alias introPath: mediaPlayer.source
    property alias aspectRatio: mediaPlayer.aspectRatio

    readonly property int maximumTextureSize: maxTextureSize //< Global context property.

    MediaPlayer
    {
        id: mediaPlayer
        maxTextureSize: intro.maximumTextureSize
        onSourceChanged: updatePlayingState()
        Component.onCompleted: updatePlayingState()

        function updatePlayingState()
        {
            if (intro.visible)
                play()
            else
                stop()
        }
    }

    VideoPositioner
    {
        id: videoPositioner
        anchors.fill: parent

        sourceSize: Qt.size(videoOutput.implicitWidth, videoOutput.implicitHeight)

        item: videoOutput

        VideoOutput
        {
            id: videoOutput
            player: mediaPlayer
        }
    }

    onVisibleChanged: 
        mediaPlayer.updatePlayingState()
}
