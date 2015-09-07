import QtQuick 2.4

import QtMultimedia 5.0
import QtQuick.Window 2.0

import com.networkoptix.qml 1.0

import ".."

QnObject {
    id: player

    property string resourceId

    readonly property bool playing: d.mediaPlayer ? d.mediaPlayer.playbackState === MediaPlayer.PlayingState && d.mediaPlayer.position > 0 : false
    readonly property bool atLive: d.position < 0

    readonly property alias position: d.position

    readonly property var mediaPlayer: d.mediaPlayer
    readonly property var chunkProvider: chunkProvider
    readonly property var resourceHelper: resourceHelper

    readonly property alias resourceName: resourceHelper.resourceName

    signal timelineCorrectionRequest(real position)
    signal timelinePositionRequest(real position)

    QtObject {
        id: d

        readonly property int liveBound: 60000

        property bool paused: false
        property real position: -1
        property real chunkEnd: -1
        property int prevPlayerPosition: 0
        property bool updateTimeline: false
        property bool dirty: true
        property alias mediaPlayer: playerLoader.item
    }

    Loader {
        id: playerLoader
        sourceComponent: resourceHelper.protocol != "mpjpeg" ? qtPlayer : mjpegPlayer
    }

    Component {
        id: qtPlayer

        MediaPlayer {
            source: resourceHelper.mediaUrl

            autoPlay: !d.paused

            onPositionChanged: updatePosition()
            onSourceChanged: console.log(source)
        }
    }

    Component {
        id: mjpegPlayer

        QnMjpegPlayer {
            source: resourceHelper.mediaUrl

            onPositionChanged: updatePosition()
            onSourceChanged: console.log(source)
            reconnectOnPlay: atLive
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

    function alignedPosition(pos) {
        return pos >= 0 ? chunkProvider.closestChunkStartMs(pos, true) : -1
    }

    function playLive() {
        d.dirty = true
        play(-1)
    }

    function play(pos) {
        if (!d.dirty) {
            if (!playing)
                d.mediaPlayer.play()
            return
        }

        var live = (new Date()).getTime()
        var aligned = -1
        if (live - pos > d.liveBound) {
            aligned = alignedPosition(pos)
            if (live - aligned <= d.liveBound)
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
        d.paused = false

        d.dirty = false
    }

    function pause() {
        d.mediaPlayer.pause()
        d.paused = true
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

        d.position += d.mediaPlayer.position - d.prevPlayerPosition
        d.prevPlayerPosition = d.mediaPlayer.position

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

