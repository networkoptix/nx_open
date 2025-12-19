// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick

import Nx.Core

import nx.vms.client.core

import ".."

AbstractTimelineController
{
    id: timelineController

    property VideoScreenController controller
    property DataTimeline timeline

    property bool updating: false

    function setTimelinePosition(positionMs, immediate)
    {
        timelineJump(positionMs)
    }

    function timelineJump(positionMs)
    {
        updating = true

        const positionInWindow = timeline.contains(timeline.positionMs)

        timeline.positionAtLive = positionMs == -1
        timeline.positionMs = timeline.positionAtLive ? NxGlobals.syncNowMs() : positionMs

        updating = false

        if (positionInWindow)
            timeline.followPosition()
    }

    Connections
    {
        target: controller.mediaPlayer

        function onPositionChanged()
        {
            if (controller.mediaPlayer.mediaStatus !== MediaPlayer.Loaded)
                return

            if (!controller.playingLive && !timeline.positioning)
                timelineJump(controller.mediaPlayer.position)
        }
    }

    Connections
    {
        target: controller

        function onPlayingLiveChanged()
        {
            timeline.positionAtLive = controller.playingLive
        }
    }

    Connections
    {
        target: timeline

        function onPositionMsChanged()
        {
            if (timelineController.updating)
                return

            if (timeline.positioning)
                controller.setPosition(timeline.positionMs)
            else if (!timeline.positionAtLive)
                controller.forcePosition(timeline.positionMs, true)
        }

        function onPositioningChanged()
        {
            if (timeline.positioning)
                controller.preview()
            else if (controller.playing)
                controller.play()
        }
    }
}
