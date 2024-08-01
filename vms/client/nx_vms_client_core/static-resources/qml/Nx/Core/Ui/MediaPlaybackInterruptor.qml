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
            if (!player || player.playbackState !== MediaPlayer.Playing || d.interrupted)
                return

            d.interrupted = true
            d.interruptedPosition = currentPosition()
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
