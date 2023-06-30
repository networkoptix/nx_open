// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.6
import QtMultimedia

import Nx.Core 1.0
import Nx.Core.Items 1.0

import nx.vms.client.core 1.0

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
        videoSurface: videoOutput.videoSink
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
        }
    }

    onVisibleChanged:
        mediaPlayer.updatePlayingState()
}
