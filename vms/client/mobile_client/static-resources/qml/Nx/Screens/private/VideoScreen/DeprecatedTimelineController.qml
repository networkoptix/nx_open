// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick

import nx.vms.client.core

import ".."

AbstractTimelineController
{
    id: timelineController

    property DeprecatedTimeline timeline
    property VideoScreenController controller

    readonly property bool mediaLoaded: controller.mediaPlayer.mediaStatus === MediaPlayer.Loaded

    readonly property bool loadingChunks: timeline.chunkProvider.loading
        || timeline.chunkProvider.loadingMotion

    onLoadingChunksChanged:
    {
        if (loadingChunks || !controller.tryFixVirtualCameraPosition)
            return

        controller.tryFixVirtualCameraPosition = false

        if (controller.liveVirtualCamera)
            jumpToFirstChunk()
    }

    onMediaLoadedChanged:
    {
        updateTimelinePosition()
    }

    Connections
    {
        target: controller.mediaPlayer

        function onPositionChanged()
        {
            updateTimelinePosition()
        }
    }

    function setTimelinePosition(position, immediate)
    {
        if (immediate)
            timeline.timelineView.setPositionImmediately(position)
        else
            timeline.position = position
    }

    function updateTimelinePosition()
    {
        if (controller.mediaPlayer.mediaStatus !== MediaPlayer.Loaded)
            return

        const paused = controller.mediaPlayer.playbackState === MediaPlayer.Stopped
            || controller.mediaPlayer.playbackState === MediaPlayer.Paused

        var liveMode = controller.mediaPlayer.liveMode && !paused
        if (!liveMode && !timeline.moving)
        {
            timeline.autoReturnToBounds = false
            timeline.position = controller.mediaPlayer.position
        }
    }

    function timelineJump(position)
    {
        timeline.autoReturnToBounds = false
        timeline.timelineView.setPositionImmediately(position)
    }
}
