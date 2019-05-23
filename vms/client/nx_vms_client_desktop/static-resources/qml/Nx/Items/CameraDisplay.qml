import QtQuick 2.6

import Nx 1.0
import Nx.Core 1.0
import Nx.Items 1.0
import Nx.Media 1.0

Rectangle
{
    id: cameraDisplay
    color: ColorTheme.window

    property alias cameraResourceId: player.resourceId
    property alias maxTextureSize: player.maxTextureSize
    property alias audioEnabled: player.audioEnabled

    property alias videoOverlayComponent: videoOverlayLoader.sourceComponent
    property alias channelOverlayComponent: multiVideoOutput.channelOverlayComponent

    property alias videoOverlay: videoOverlayLoader.item
    readonly property MediaPlayer mediaPlayer: player

    MediaPlayer
    {
        id: player
        readonly property bool loaded: mediaStatus == MediaPlayer.MediaStatus.Loaded
        onResourceIdChanged: multiVideoOutput.clear()
    }

    MediaResourceHelper
    {
        id: mediaResourceHelper
        resourceId: player.resourceId
    }

    VideoPositioner
    {
        id: videoPositioner
        anchors.fill: parent

        sourceSize: Qt.size(multiVideoOutput.implicitWidth, multiVideoOutput.implicitHeight)

        item: multiVideoOutput

        customAspectRatio:
        {
            var aspectRatio = mediaResourceHelper ? mediaResourceHelper.customAspectRatio : 0.0
            if (aspectRatio === 0.0)
            {
                if (mediaPlayer && mediaPlayer.loaded)
                    aspectRatio = mediaPlayer.aspectRatio
                else if (mediaResourceHelper)
                    aspectRatio = mediaResourceHelper.aspectRatio
                else
                    aspectRatio = sourceSize.width / sourceSize.height
            }

            var layoutSize = mediaResourceHelper ? mediaResourceHelper.layoutSize : Qt.size(1, 1)
            aspectRatio *= layoutSize.width / layoutSize.height

            return aspectRatio
        }

        MultiVideoOutput
        {
            id: multiVideoOutput
            resourceHelper: mediaResourceHelper
            mediaPlayer: cameraDisplay.mediaPlayer

            Loader
            {
                id: videoOverlayLoader
                anchors.fill: parent
            }
        }
    }
}
