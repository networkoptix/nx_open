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

    // TODO: #vkutin After the old timeline removal name these functions properly.
    // Maybe merge this component with VideoScreenController.

    function setTimelinePosition(positionMs, immediate)
    {
        timelineJump(positionMs)
        timeline.followPosition()
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
            if (!timeline.positioning)
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

            if ((timeline.positioning && !controller.playingLive) || !timeline.positionAtLive)
                controller.setPosition(timeline.positionMs, /*save*/ !timeline.positioning)
        }

        function onPositionAtLiveChanged()
        {
            if (timeline.positionAtLive && !timeline.positioning)
                controller.playLive(/*suppressSavePosition*/ true)
        }

        function onPositioningChanged()
        {
            if (timeline.positioning)
                controller.preview()
            else if (timeline.positionAtLive)
                controller.playLive()
            else if (controller.playing)
                controller.play()
        }
    }
}
