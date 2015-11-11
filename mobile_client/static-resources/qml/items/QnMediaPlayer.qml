import QtQuick 2.4

import QtMultimedia 5.0
import QtQuick.Window 2.0

import com.networkoptix.qml 1.0

import ".."

QnObject {
    id: player

    property string resourceId

    readonly property bool loading: d.mediaPlayer && d.mediaPlayer.loading
    readonly property bool playing: d.mediaPlayer ? d.mediaPlayer.playbackState === MediaPlayer.PlayingState && d.mediaPlayer.position > 0 : false
    readonly property bool atLive: d.position < 0

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

        property bool paused: false
        property real position: -1
        property real chunkEnd: -1
        property int prevPlayerPosition: 0
        property bool updateTimeline: false
        property bool dirty: true
        property alias mediaPlayer: playerLoader.item
        property bool resetUrlOnConnect: false
    }

    Loader {
        id: playerLoader
        sourceComponent: resourceHelper.protocol != QnMediaResourceHelper.Mjpeg ? qtPlayer : mjpegPlayer
    }

    Component {
        id: qtPlayer

        MediaPlayer {
            source: resourceHelper.mediaUrl

            autoPlay: !d.paused

            onPositionChanged: {
                updatePosition()
            }

            onSourceChanged: console.log(source)

            onPlaybackStateChanged: {
                // A workaround for inconsistent playbackState changes of Qt MediaPlayer
                if (playbackState == MediaPlayer.PlayingState && d.paused)
                    pause()
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
        }
    }

    Component {
        id: mjpegPlayer

        QnMjpegPlayer {
            source: resourceHelper.mediaUrl

            onPositionChanged: updatePosition()
            onSourceChanged: console.log(source)
            reconnectOnPlay: atLive

            readonly property bool hasTimestamp: true

            readonly property bool loading: playbackState == MediaPlayer.PlayingState && position == 0
        }
    }

    QnMediaResourceHelper {
        id: resourceHelper

        screenSize: Qt.size(mainWindow.width, mainWindow.height)
        resourceId: player.resourceId
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
                    resourceHelper.setPosition(position)
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
        d.dirty = true
        play(-1)
    }

    function play(pos) {
        d.paused = false

        if (!d.dirty) {
            if (!playing)
                d.mediaPlayer.play()
            return
        }

        var live = (new Date()).getTime()
        var aligned = -1
        if (live - pos > d.lastMinuteLength) {
            aligned = alignedPosition(pos)
            if (live - aligned <= d.lastMinuteLength)
                aligned = -1
        }
        if (d.position == -1 && aligned == -1) {
            timelinePositionRequest(d.position)
            if (!playing)
                d.mediaPlayer.play()
            return
        }

        d.mediaPlayer.stop()

        d.position = aligned >= 0 ? Math.max(aligned, pos) : -1
        d.chunkEnd = (d.position >= 0) ? chunkProvider.closestChunkEndMs(pos, true) : -1

        timelinePositionRequest(d.position)
        d.updateTimeline = true
        resourceHelper.setPosition(d.position)

        d.prevPlayerPosition = 0
        d.mediaPlayer.play()

        d.dirty = false
    }

    function pause() {
        d.mediaPlayer.pause()
        d.paused = true
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

    function updatePosition() {
        if (d.position < 0)
            return

        if (mediaPlayer.hasTimestamp) {
            if (mediaPlayer.timestamp >= 0) {
                d.position = mediaPlayer.timestamp
                timelineCorrectionRequest(d.position)
            }
            return
        } else {
            if (d.prevPlayerPosition == 0 && d.mediaPlayer.position > d.maximumInitialPosition) {
                /* A workaround for Android issue 11590
                   Sometimes Android MediaPlayer returns invalid position so we can't calculate the real timestamp.
                   To make the approximate timestamp a bit closer to the real timestamp we assign a magic number to d.position found experimentally.
                 */
                d.position += 300
            } else {
                d.position += d.mediaPlayer.position - d.prevPlayerPosition
            }
            d.prevPlayerPosition = d.mediaPlayer.position
        }

        if (d.chunkEnd >= 0 && d.position >= d.chunkEnd) {
            seek(d.chunkEnd + 1)
            return
        }

        if (d.updateTimeline) {
            timelineCorrectionRequest(d.position)
            d.updateTimeline = false
        }
    }
}

