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

        onCanPlayChanged:
        {
            if (!canPlay)
                interrupt()
            else if (canPlay && player && player.playbackState === MediaPlayer.Paused)
                resumePlaying()
        }

        function resumePlaying()
        {
            if (!player)
                return

            player.position = interruptedPosition
            player.play()
        }

        function interrupt()
        {
            if (!player)
                return

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
