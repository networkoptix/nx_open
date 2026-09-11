// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick

import Nx.Core
import nx.vms.client.core

NxObject
{
    id: interruptor

    property bool interruptOnInactivity: true
    property bool playable: true

    // Decides if unplayable video should be stopped or paused.
    property bool forceStopWhenNotPlayable: false

    property MediaPlayer player: null

    property bool applicationActive: Qt.application.state === Qt.ApplicationActive

    function setInterruptedPosition(timestamp)
    {
        d.interruptedPosition = timestamp
    }

    onPlayerChanged:
    {
        setInterruptedPosition(-1)
        d.interruptedState = MediaPlayer.Stopped //< Meaning there's no active interruption.
    }

    onForceStopWhenNotPlayableChanged:
    {
        if (forceStopWhenNotPlayable)
            d.forceStopIfInterrupted()
    }

    QtObject
    {
        id: d

        readonly property bool interruptedOnInactivity:
            interruptOnInactivity && !interruptor.applicationActive

        readonly property bool canPlay: interruptor.playable && !interruptedOnInactivity

        property real interruptedPosition: -1

        property int interruptedState: MediaPlayer.Stopped
        readonly property bool interrupted: interruptedState !== MediaPlayer.Stopped

        onInterruptedOnInactivityChanged:
        {
            // As we have problems on iOS that after application awake stream can jump forward to
            // the next I-frame, we need to forcibly stop media player on app moving to background.
            // Playback which is already interrupted is only escalated to the stopped state here,
            // the interruption itself is always started by the canPlay handler below.
            if (interruptedOnInactivity)
                forceStopIfInterrupted()
        }

        onCanPlayChanged:
        {
            if (canPlay)
                tryRestorePlayback()
            else
                tryInterrupt(interruptedOnInactivity || forceStopWhenNotPlayable)
        }

        function tryInterrupt(forceStop)
        {
            if (!player || d.interrupted)
                return

            if (player.playbackState === MediaPlayer.Stopped)
                return

            d.interruptedPosition = currentPosition()
            d.interruptedState = player.playbackState

            if (forceStop)
                player.stop(/*clearVideoOutput*/ false)
            else if (player.playbackState === MediaPlayer.Playing)
                player.pause()
        }

        function tryRestorePlayback()
        {
            if (!player || !d.interrupted)
                return

            const resume = d.interruptedState === MediaPlayer.Playing
            d.interruptedState = MediaPlayer.Stopped

            if (player.playbackState === MediaPlayer.Stopped)
                player.position = interruptedPosition
            else if (player.playbackState === MediaPlayer.Playing)
                return //< Playback was externally restarted.

            if (resume)
            {
                player.play()
            }
            else if (player.playbackState === MediaPlayer.Stopped)
            {
                // We have to emulate paused state with previewing in current implementation.
                player.preview()
            }
        }

        function forceStopIfInterrupted()
        {
            if (player && d.interrupted && player.playbackState !== MediaPlayer.Stopped)
                player.stop(/*clearVideoOutput*/ false)
        }

        function currentPosition()
        {
            // MediaPlayer keeps the live mode while being paused, so the live position may only
            // be stored for actually running playback.
            return player.liveMode && player.playbackState === MediaPlayer.Playing
                ? -1
                : player.position
        }
    }
}
