import QtQuick

import Nx.Core
import nx.vms.client.core

NxObject
{
    id: interruptor

    property bool interruptOnInactivity: true
    property bool playable: true

    property MediaPlayer player: null

    function setInterruptedPosition(timestamp)
    {
        d.interruptedPosition = timestamp
    }

    onPlayerChanged: setInterruptedPosition(-1)

    QtObject
    {
        id: d

        readonly property bool interruptedOnInactivity: interruptOnInactivity
            && (Qt.application.state !== Qt.ApplicationActive)
        readonly property bool canPlay: interruptor.playable && !interruptedOnInactivity
        property real interruptedPosition: -1
        property bool interrupted: false

        onInterruptedOnInactivityChanged:
        {
            // As we have problems on iOS that after application awake stream can jump forward to
            // the next I frame, we need to forcibly stop media player on app moving to background.
            if (interruptOnInactivity)
                interrupt(/*forceStop*/ true)
        }

        onCanPlayChanged:
        {
            if (canPlay)
                tryResumePlaying()
            else
                tryInterrupt()
        }

        function tryResumePlaying()
        {
            if (!player || !d.interrupted)
                return

            d.interrupted = false
            player.position = interruptedPosition
            player.play()
        }

        function tryInterrupt()
        {
            if (player && player.playbackState === MediaPlayer.Playing && !d.interrupted)
                interrupt(/*forceStop*/ false)
        }

        function interrupt(forceStop)
        {
            // Store interrupted position only once per interruption cycle.
            if (!d.interrupted)
                d.interruptedPosition = currentPosition()

            d.interrupted = true
            if (forceStop)
                player.stop()
            else
                player.pause()
        }

        function currentPosition()
        {
            return player && !player.liveMode
                ? player.position
                : -1
        }
    }
}
