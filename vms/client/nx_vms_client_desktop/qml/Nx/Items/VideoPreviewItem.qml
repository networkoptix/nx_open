// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick

import Nx.Common
import Nx.Items
import Nx.Core.Items

import nx.vms.client.core
import nx.vms.client.desktop

CameraDisplay
{
    id: videoPreview

    videoQuality: MediaPlayer.LowVideoQuality
    audioEnabled: false

    onVisibleChanged: updatePlayingState()

    Connections
    {
        target: videoPreview.mediaPlayer

        function onResourceChanged()
        {
            videoPreview.updatePlayingState()
        }
    }

    function updatePlayingState()
    {
        if (visible)
            mediaPlayer.playLive()
        else
            mediaPlayer.stop()
    }
}
