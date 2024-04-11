// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.6
import QtMultimedia

import Nx.Core 1.0

import nx.vms.client.core 1.0

MultiVideoLayout
{
    id: layout

    property MediaPlayer mediaPlayer: null
    property Component channelOverlay: null

    // Implementation.

    property size __sourceSize

    implicitWidth: __sourceSize.width * layoutSize.width
    implicitHeight: __sourceSize.height * layoutSize.height

    delegate: VideoOutput
    {
        id: videoOutput

        readonly property int index: model.index

        fillMode: VideoOutput.Stretch

        onSourceRectChanged:
        {
            if (index === 0 && __sourceSize.width <= 0.0)
                __sourceSize = Qt.size(sourceRect.width, sourceRect.height)
        }

        MediaPlayerChannelAdapter
        {
            mediaPlayer: layout.mediaPlayer
            channel: index
            videoSurface: videoOutput.videoSink
        }

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
