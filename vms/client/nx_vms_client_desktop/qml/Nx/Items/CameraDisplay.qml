// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.15
import QtQuick.Window 2.15

import Nx 1.0
import Nx.Core 1.0
import Nx.Core.Items 1.0
import Nx.Items 1.0

import nx.vms.client.core 1.0

Rectangle
{
    id: cameraDisplay
    color: ColorTheme.window

    property alias cameraResourceId: player.resourceId
    property bool audioEnabled: true

    property alias videoOverlayComponent: videoOverlayLoader.sourceComponent
    property alias channelOverlayComponent: multiVideoOutput.channelOverlay

    property alias videoOverlay: videoOverlayLoader.item
    property alias videoQuality: player.videoQuality

    readonly property alias mediaPlayer: player

    property alias tag: player.tag

    property real forcedAspectRatio: 0 //< Zero if no aspect ratio is forced.

    property alias defaultRotation: mediaResourceHelper.customRotation
    property int videoRotation: defaultRotation

    readonly property bool loaded: player.mediaStatus >= MediaPlayer.MediaStatus.Loaded
        && player.mediaStatus < MediaPlayer.MediaStatus.InvalidMedia

    readonly property alias audioSupported: mediaResourceHelper.audioSupported

    TextureSizeHelper
    {
        id: textureSizeHelper
    }

    MediaPlayer
    {
        id: player
        audioEnabled: cameraDisplay.audioEnabled && mediaResourceHelper.audioEnabled
        maxTextureSize: textureSizeHelper.maxTextureSize
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

        videoRotation: cameraDisplay.videoRotation

        customAspectRatio:
        {
            if (cameraDisplay.forcedAspectRatio > 0)
                return cameraDisplay.forcedAspectRatio

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
