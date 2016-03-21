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
    readonly property alias chunkProvider: resourceHelper.chunkProvider
    readonly property alias resourceHelper: resourceHelper

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
        property alias mediaPlayer: playerLoader.item
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
            if (d.mediaPlayer && !d.paused && d.startPosition == d.mediaPlayer.position) {
                d.failed = true
                d.mediaPlayer.stop()
            }
        }

        function updateTimer() {
            if (d.paused || !d.mediaPlayer) {
                stop()
                return
            }

            d.failed = false
            d.startPosition = d.mediaPlayer.position

            restart()
        }
    }

    Loader {
        id: playerLoader
    }

    Component {
        id: qtPlayerComponent

        MediaPlayer {
            autoPlay: !d.paused

            onPositionChanged: {
                failureTimer.updateTimer()

                if (d.position < 0)
                    return

                if (d.prevPlayerPosition == 0 && position > d.maximumInitialPosition) {
                    /* A workaround for Android issue 11590
                       Sometimes Android MediaPlayer returns invalid position so we can't calculate the real timestamp.
                       To make the approximate timestamp a bit closer to the real timestamp we assign a magic number to d.position found experimentally.
                     */
                    d.position += 300
                } else {
                    d.position += position - d.prevPlayerPosition
                }
                d.prevPlayerPosition = position

                if (d.chunkEnd >= 0 && d.position >= d.chunkEnd) {
                    player.seek(d.chunkEnd + 1)
                    return
                }

                if (d.updateTimeline) {
                    timelineCorrectionRequest(d.position)
                    d.updateTimeline = false
                }
            }

            onPlaybackStateChanged: {
                // A workaround for inconsistent playbackState changes of Qt MediaPlayer
                if (playbackState == MediaPlayer.PlayingState && d.paused)
                    pause()
            }

            onStatusChanged: {
                if (!d.paused && source && status == MediaPlayer.EndOfMedia)
                    player.play(d.chunkEnd + 1)
            }

            readonly property bool hasTimestamp: false

            readonly property bool loading: {
                if (playbackState == MediaPlayer.PlayingState)
                    return position == 0

                return status == MediaPlayer.Loading ||
                       status == MediaPlayer.Buffering ||
                       status == MediaPlayer.Stalled ||
                       status == MediaPlayer.Loaded ||
                       status == MediaPlayer.Buffered
            }

            readonly property bool playing: playbackState === MediaPlayer.PlayingState && position > 0

            function getFinalTimestamp(startPos) {
                if (startPos <= 0)
                    return -1

                return chunkProvider.closestChunkEndMs(startPos, true)
            }
        }
    }

    Component {
        id: mjpegPlayerComponent

        QnMjpegPlayer {
            readonly property bool hasTimestamp: true
            readonly property bool loading: playbackState == QnMjpegPlayer.PlayingState && mediaStatus != QnMjpegPlayer.BufferedMedia
            readonly property bool playing: playbackState == QnMjpegPlayer.PlayingState && mediaStatus == QnMjpegPlayer.BufferedMedia

            finalTimestamp: {
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
                    player.seek(d.chunkEnd + 1)
                    return
                }

                if (timestamp >= 0) {
                    d.position = timestamp
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
    }

    QnMediaResourceHelper {
        id: resourceHelper

        property bool resetWhenProtocolChanged: false

        property bool protocolHasChanged: false

        screenSize: Qt.size(mainWindow.width, mainWindow.height)
        resourceId: player.resourceId

        onMediaUrlChanged: {
            d.firstPlay = true
            console.log("Media URL changed: " + mediaUrl)

            if (protocolHasChanged)
            {
                protocolHasChanged = false
                stop()
                play(d.position)
            }
        }

        onProtocolChanged: {
            if (resetWhenProtocolChanged)
                protocolHasChanged = true
        }
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
                    if (d.mediaPlayer)
                        d.mediaPlayer.source = resourceHelper.mediaUrl
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

        if (!pos)
            pos = d.position

        var live = (new Date()).getTime()
        var aligned = -1
        if (pos > 0 && live - pos > d.lastMinuteLength) {
            aligned = alignedPosition(pos)
            if (live - aligned <= d.lastMinuteLength)
                aligned = -1
        }

        if (d.mediaPlayer)
            d.mediaPlayer.stop()

        d.position = aligned >= 0 ? Math.max(aligned, pos) : -1

        d.chunkEnd = (d.position >= 0 && d.mediaPlayer) ? d.mediaPlayer.getFinalTimestamp(pos) : -1

        timelinePositionRequest(d.position)
        d.updateTimeline = true

        resourceHelper.resetWhenProtocolChanged = false

        if (d.position == -1 && resourceHelper.position == -1) {
            if (!d.firstPlay)
                resourceHelper.updateUrl()
            else
                d.firstPlay = false
        } else {
            resourceHelper.position = d.position
        }

        playerLoader.sourceComponent = (resourceHelper.protocol == QnMediaResourceHelper.Mjpeg) ? mjpegPlayerComponent : qtPlayerComponent
        d.mediaPlayer.source = resourceHelper.mediaUrl

        d.prevPlayerPosition = 0
        d.mediaPlayer.play()
        failureTimer.updateTimer()

        resourceHelper.resetWhenProtocolChanged = true

        d.dirty = false
    }

    function pause() {
        d.paused = true

        if (d.mediaPlayer)
            d.mediaPlayer.pause()
    }

    function stop() {
        d.dirty = true

        if (d.mediaPlayer)
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

