import QtQuick 2.4

import QtMultimedia 5.0
import QtQuick.Window 2.0

import com.networkoptix.qml 1.0

import ".."

QnObject {
    id: player

    property string resourceId

    readonly property bool loading: !d.paused && d.mediaPlayer && d.mediaPlayer.loading
    readonly property bool playing: d.mediaPlayer ? d.mediaPlayer.playing : false
    readonly property bool atLive: d.position < 0

    readonly property alias failed: d.failed

    readonly property alias position: d.position
    readonly property bool hasTimestamp: d.mediaPlayer && d.mediaPlayer.hasTimestamp

    readonly property var mediaPlayer: d.mediaPlayer
    readonly property var chunkProvider: chunkProvider
    readonly property var resourceHelper: resourceHelper

    readonly property alias resourceName: resourceHelper.resourceName

    signal timelineCorrectionRequest(real position)
    signal timelinePositionRequest(real position)

    QtObject {
        id: d

        readonly property int lastMinuteLength: 60000
        readonly property int maximumInitialPosition: 2000

        property bool failed: false
        property int startPosition: 0

        property bool paused: false
        property real position: -1
        property real chunkEnd: -1
        property int prevPlayerPosition: 0
        property bool updateTimeline: false
        property bool dirty: true
        property var mediaPlayer: nxPlayer
        property bool resetUrlOnConnect: false

        /* Qt MediaPlayer fails to play video if media source is frequently changed at first video load.
           Workaround it by blocking redundant URL update at first video load.
         */
        property bool firstPlay: true

        onPausedChanged: failureTimer.updateTimer()
    }

    Timer {
        id: failureTimer

        interval: resourceHelper.protocol == QnMediaResourceHelper.Mjpeg ? 20 * 1000 : 120 * 1000
        repeat: false
        running: false
        onTriggered: {
            if (!d.paused && d.startPosition == d.mediaPlayer.position) {
                d.failed = true
                d.mediaPlayer.stop()
            }
        }

        function updateTimer() {
            if (d.paused) {
                stop()
                return
            }

            d.failed = false
            d.startPosition = d.mediaPlayer.position

            restart()
        }
    }

    QnPlayer {
		id: nxPlayer

        source: "camera://media/" + player.resourceId

        readonly property bool hasTimestamp: true
        readonly property bool loading: playbackState == QnPlayer.PlayingState && mediaStatus != QnPlayer.BufferedMedia
        readonly property bool playing: playbackState == QnPlayer.PlayingState && mediaStatus == QnPlayer.BufferedMedia

		readonly property real timestamp: position
		property real finalTimestamp: {
            var chunksEnd = resourceHelper.finalTimestamp
            if (chunksEnd != -1)
                chunksEnd = chunkProvider.closestChunkEndMs(chunksEnd - 1, false)
            return chunksEnd
        }

        reconnectOnPlay: atLive

        onTimestampChanged: {
            if (d.position < 0)
                return

            if (d.chunkEnd >= 0 && timestamp >= d.chunkEnd) {
                seek(d.chunkEnd + 1)
                return
            }

            if (mediaPlayer.timestamp >= 0) {
                d.position = mediaPlayer.timestamp
                timelineCorrectionRequest(d.position)
            }
        }

        onPlaybackFinished: {
            if (d.position == -1)
                return

            if (!d.paused)
                player.play(d.chunkEnd + 1)
        }

        onPositionChanged: failureTimer.updateTimer()

        function getFinalTimestamp(startPos) {
            if (startPos <= 0)
                return -1

            return Qt.binding(function() { return finalTimestamp })
        }
    }

    QnMediaResourceHelper {
        id: resourceHelper

        screenSize: Qt.size(mainWindow.width, mainWindow.height)
        resourceId: player.resourceId

        onMediaUrlChanged: {
            d.firstPlay = true
            console.log("Media URL changed: " + mediaUrl)
        }
    }

    QnCameraChunkProvider {
        id: chunkProvider
        resourceId: player.resourceId
    }

    Timer {
        interval: 30000
        triggeredOnStart: true
        running: true
        onTriggered: chunkProvider.update()
    }

    Timer {
        interval: 5000
        running: player.playing && d.position >= 0
        onTriggered: d.updateTimeline = true
    }

    Connections {
        target: connectionManager
        onConnectionStateChanged: {
            if (connectionManager.connectionState == QnConnectionManager.Connected) {
                if (d.resetUrlOnConnect) {
                    d.resetUrlOnConnect = false
                    resourceHelper.position = position
                }
            } else {
                d.resetUrlOnConnect = true
            }
        }
    }

    function alignedPosition(pos) {
        return pos >= 0 ? chunkProvider.closestChunkStartMs(pos, true) : -1
    }

    function playLive() {
        play(-1)
    }

    function play(pos) {
        d.failed = false

        if (d.position == -1 || pos && (pos != d.position || pos == -1))
            d.dirty = true

        d.paused = false

        if (!d.dirty) {
            if (!playing) {
                d.mediaPlayer.play()
                failureTimer.updateTimer()
            }
            return
        }

        var live = (new Date()).getTime()
        var aligned = -1
        if (pos && pos > 0 && live - pos > d.lastMinuteLength) {
            aligned = alignedPosition(pos)
            if (live - aligned <= d.lastMinuteLength)
                aligned = -1
        }

        d.mediaPlayer.stop()

        d.position = aligned >= 0 ? Math.max(aligned, pos) : -1

        d.chunkEnd = (d.position >= 0) ? d.mediaPlayer.getFinalTimestamp(pos) : -1

        timelinePositionRequest(d.position)
        d.updateTimeline = true

        if (d.position == -1 && resourceHelper.position == -1) {
            if (!d.firstPlay)
                resourceHelper.updateUrl()
            else
                d.firstPlay = false
        } else {
            resourceHelper.position = d.position
        }

        d.prevPlayerPosition = 0
        d.mediaPlayer.play()
        failureTimer.updateTimer()

        d.dirty = false
    }

    function pause() {
        d.paused = true
        d.mediaPlayer.pause()
    }

    function stop() {
        d.dirty = true
        d.mediaPlayer.stop()
    }

    function seek(pos) {
        d.dirty = true

        if (d.paused) {
            d.position = pos
            timelinePositionRequest(d.position)
            d.updateTimeline = false
        } else {
            play(pos)
        }
    }
}

